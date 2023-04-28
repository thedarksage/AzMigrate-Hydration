<?
/*
 *================================================================= 
 * FILENAME
 *    passphrase_update.php
 * DESCRIPTION
 *    This script used to do insert health factors on passphrase update on configuration server.
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
include_once $config_file;

try
{
	
	$conn = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("3");
	if(!$conn)
	{
		debugLog("passphrase_update_in_db ::mysqli connection failed");
	}
	
	$multi_query = array();	
	
	$sql = "select 
				sourcehostid, 
				destinationhostid, 
				pairId 
			from 
				srcLogicalvolumeDestLogicalVolume";
	if ($result = mysqli_query($conn, $sql)) 
	{
		while ($obj = mysqli_fetch_object($result)) 
		{
			$source_host_id = $obj->sourcehostid;
			$destination_host_id = $obj->destinationhostid;
			$pair_id = $obj->pairId;
			$protections_details[$obj->sourcehostid][$pair_id]= array("sourceHostId"=>$source_host_id, "destinationHostId"=>$destination_host_id);
		}
		mysqli_free_result($result);
	}
		
	$sql = "select 
				id, 
				name, 
				ipaddress, 
				processServerenabled, 
				sentinelenabled, 
				agentRole 
			from 
				hosts 
			where 
				id!='5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
	if ($result = mysqli_query($conn, $sql)) 
	{
		$cs_host_name = gethostname();
		while ($obj = mysqli_fetch_object($result)) 
		{
			$host_id = $obj->id;
			$host_name = $obj->name;
			$ip_address = $obj->ipaddress;
			if ($obj->processServerenabled == "1")
			{
				//PS Health EPH0021
				$place_holders = serialize(array("Name"=>$host_name,"IpAddress"=>$ip_address,"ConfigServer"=>$cs_host_name));
				$place_holders = $conn_obj->sql_escape_string($place_holders);
				$sql = "insert into infrastructurehealthfactors(hostId, healthFactorCode, healthFactor, priority, component, healthDescription,  healthCreationTime, healthUpdateTime, placeHolders) values ('$host_id','EPH00021','2',0,'Ps','PASSPHRASE_UPDATE',now(), now(), '$place_holders') on duplicate key update healthUpdateTime = now()";
				$multi_query['insert'][] = $sql;
			}
			if ($obj->agentRole == "MasterTarget")
			{
				//MT Helath EMH0004
				$place_holders = serialize(array("Name"=>$host_name,"IpAddress"=>$ip_address,"ConfigServer"=>$cs_host_name));
				$place_holders = $conn_obj->sql_escape_string($place_holders);
				$sql = "insert into infrastructurehealthfactors(hostId, healthFactorCode, healthFactor, priority, component, healthDescription,  healthCreationTime, healthUpdateTime, placeHolders) values ('$host_id','EMH0004','2',0,'Mt','PASSPHRASE_UPDATE',now(), now(), '$place_holders') on duplicate key update healthUpdateTime = now()";
				$multi_query['insert'][] = $sql;
			}
			if ($obj->sentinelenabled == "1" and $obj->agentRole == "Agent")
			{	
				//Source Health ECH00027
				if ($protections_details[$host_id])
				{
					foreach ($protections_details[$host_id] as $pair_id=>$pair_id_data)
					{
						$source_host_id = $pair_id_data['sourceHostId'];
						$destination_host_id = $pair_id_data['destinationHostId'];
						
						$sql = "insert INTO healthfactors(sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, updated, healthUpdateTime) VALUES ('$source_host_id', '$destination_host_id', '$pair_id', 'ECH00027','ECH00027', '2', '0','Source', 'PASSPHRASE_UPDATE', now(),'Y', now()) on duplicate key update healthUpdateTime = now()";
						$multi_query['insert'][] = $sql;
					}
				}
			}
		}
		mysqli_free_result($result);
	}
				
	if(sizeof($multi_query['insert']))
	{
		$transaction_status = dobatchupdates_imp($multi_query);
		if($transaction_status)
		{
			debugLog("passphrase_update_in_db :: Failed");
			print "3";
			exit(3);
		}
		else
		{
			debugLog("passphrase_update_in_db :: Success");
			print "1";
			exit(1);
		}
	}
}
catch(Exception $e)
{
	debugLog("passphrase_update_in_db ::::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
}


function dobatchupdates_imp($multi_query_list)
{
    global $DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME;
	$return_status = 1;	
	$conn_i = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("2");
	
	if(!$conn_i)
	{
		debugLog("doBatchUpdates::mysqli connection failed");
	}
	$multi_query_list = constructBatches($multi_query_list);
	debugLog("DEBUG :: passphrase_update_in_db :: doBatchUpdates::started::  ". print_r($multi_query_list,TRUE));
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
				debugLog("DEBUG ::passphrase_update_in_db :: doBatchUpdates::started executing the query $batch_query \n");

				if ($transaction_status = mysqli_multi_query($conn_i,$batch_query))
				{
				   do {
					   /* store first result set */
					   if ($result = mysqli_store_result($conn_i))
					   {
						   mysqli_free_result($result);
					   }
					   if (mysqli_more_results($conn_i))
					   {
					   
					   }
				   } while (mysqli_next_result($conn_i));
					
					debugLog("DEBUG ::passphrase_update_in_db ::doBatchUpdates:: transaction status is:: $transaction_status \n");
				}
				if (!$transaction_status)
				{
					throw new Exception("Transaction failed from first batch","999");
				}
			}
		}
		catch(Exception $e)
		{
			debugLog("doBatchUpdates::  Message::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
			$transaction_status = FALSE;
			$return_status = 1;
		}
		/*Transaction to update the database and based on that send the response as true or false*/
		if($transaction_status)
		{
			$commit_status = mysqli_multi_query($conn_i,"COMMIT");
			debugLog("DEBUG ::passphrase_update_in_db MIGRATE_INFO ::doBatchUpdates:: Committed with status $commit_status\n");
			if ($commit_status)
			{
				$return_status =  0;
			}
		}
		else
		{
			$rollback_status = mysqli_multi_query($conn_i,"ROLLBACK");
			debugLog("doBatchUpdates:: Rolled back  with status $rollback_status\n");
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
	

function debugLog ($debugString)
{
	global $CS_INSTALLATION_PATH;
	$log_path = $CS_INSTALLATION_PATH."\\home\svsystems\\var\\";
	  $PHPDEBUG_FILE_PATH =  $log_path."passphrase_update.log";
	 
	  $fr = fopen($PHPDEBUG_FILE_PATH, 'a+');
	  if(!$fr) 
	  {
	  }
	  if (!fwrite($fr, date('m-d-Y H:i:s')." - ".$debugString . "\n")) 
	  {
	  }
	  if(!fclose($fr)) 
	  {
	  }
}

//Funciton to read the configuration server installation directory path.
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