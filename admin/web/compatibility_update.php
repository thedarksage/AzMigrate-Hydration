<?
/* 
 *=================================================================
 * FILENAME
 *    compatibility_test.php
 *
 * DESCRIPTION
 *    This script updates the compatibility number for a host based
 *    on the post data of the hostid and compatibility number and
 *    returns true or false status.
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
*/
try
{
	$install_dir_value = read_installed_directory();
	$INSTALLATION_PATH = $install_dir_value["INSTALLATION_PATH"];

	if (preg_match('/Linux/i', php_uname())) 
	{
		$REPLICATION_ROOT="/home/svsystems/";	
		include_once "$REPLICATION_ROOT/admin/web/config.php";

	}
	else
	{
		$REPLICATION_ROOT       = "$INSTALLATION_PATH\\home\\svsystems\\";
		include_once "$REPLICATION_ROOT\\admin\\web\\config.php";
	}

	$volume_obj= new VolumeProtection();
	$commonfunctions_obj = new CommonFunctions();
	$recovery_obj = new Recovery();
	$retention_obj = new Retention();

	$return_status = FALSE;

global $HOST_GUID, $CS_IP, $logging_obj;
$cs_version_array = $commonfunctions_obj->get_inmage_version_array();
$GLOBALS['record_meta_data']	= array("CSGUID"=>$HOST_GUID,
										"CSIP"=> $CS_IP,
										"CSVERSION"=>$cs_version_array[0],
										"RecordType"=>"compatibility_update");	
	$logging_obj->appinsights_log_file = "compatibility_update";
						
	$logging_obj->my_error_handler("INFO",array("PostData"=>$_POST),Log::BOTH);
	/*
	 * This piece of code block gets the post data(host id & compatibility number)
	 * from the agent and update it into the database. If the sent compatiblity 
	 * number is not present in the defined compatibility number array or database 
	 * update fails then it sends the http 500 header as the response to the agent.
	 */
	if (isset($_POST))
	{
			$host_id = $_POST['hostid'];
			$compatibility_no = $_POST['compatibility'];
			$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id,"CompatibilityNo"=>$compatibility_no),Log::BOTH);
			/*
			 * Set the error object handler here to catch any error/debug messages
			 */
			if ($host_id && $compatibility_no)
			{
					/* 
					 * The compatibility array is defined for the product versions,
					 * and this will be used to check the compatibilty number
					 * from the agent sent data.
					 */
					$compatibility_list = array
											   (
													'4.0' => 400000, 
													'4.1' => 410000, 
													'4.2' => 420000,
													'4.3' => 430000,
													'5.1' => 510000, 
													'5.5' => 550000,
													'5.50.01.0'=> 550010,
													'6.0' => 600000,
													'6.1' => 610000,
													'6.20.1' => 630050,
													'6.20.2' => 630050,
													'7.0'=> 700000,
													'7.1'=> 710000,
													'8.0'=> 800000,
													'8.1'=> 810000,
													'8.2'=> 820000,
													'8.3'=> 830000,
													'8.4'=> 840000,
													'8.5'=> 850000,
													'9.0'=> 900000,
													'9.0.1'=> 901000,
													'9.1'=> 910000,
													'9.2'=> 920000,
													'9.3'=> 930000,
													'9.4'=> 940000,
													'9.5'=> 950000,
													'9.5.1'=> 951000,
													'9.6'=> 960000,
													'9.7'=> 970000,
													'9.8'=> 980000,
													'9.9'=> 990000,
													'9.10'=> 9100000,
													'9.11'=> 9110000,
													'9.12'=> 9120000,
													'9.13'=> 9130000,
													'9.14'=> 9140000,
													'9.15'=> 9150000,
													'9.16'=> 9160000,
													'9.17'=> 9170000,
													'9.18'=> 9180000,
													'9.19'=> 9190000,
													'9.20'=> 9200000,
													'9.21'=> 9210000,
													'9.22'=> 9220000,
													'9.23'=> 9230000,
													'9.24'=> 9240000,
													'9.25'=> 9250000,
													'9.26'=> 9260000,
													'9.27'=> 9270000,
													'9.28'=> 9280000,
													'9.29'=> 9290000,
													'9.30'=> 9300000,
													'9.31'=> 9310000,
													'9.32'=> 9320000,
													'9.33'=> 9330000,
													'9.34'=> 9340000,
													'9.35'=> 9350000,
													'9.36'=> 9360000,
													'9.37'=> 9370000,
													'9.38'=> 9380000,
													'9.39'=> 9390000,
													'9.40'=> 9400000,
													'9.41'=> 9410000,
													'9.42'=> 9420000,
													'9.43'=> 9430000,
													'9.44'=> 9440000,
													'9.45'=> 9450000,
													'9.46'=> 9460000,
													'9.47'=> 9470000,
													'9.48'=> 9480000,
													'9.49'=> 9490000,
													'9.50'=> 9500000,
													'9.51'=> 9510000,
													'9.52'=> 9520000,
													'9.53'=> 9530000,
													'9.54'=> 9540000,
													'9.55'=> 9550000
												);
					if (in_array($compatibility_no,$compatibility_list))
					{
							$query = "UPDATE
										hosts
									  SET
										compatibilityNo = ?,
										PatchHistoryVX = NULL,
										PatchHistoryFX = NULL,
										PatchHistoryAppAgent = NULL 
									  WHERE 
										id =  ?";
							$conn_obj->sql_query($query,array($compatibility_no,$host_id));
							$return_status = TRUE;
					}
					else
					{
						throw new Exception("Given compatibility number is not exists in compatibility_list array");
					}

			}
			else
			{
				throw new Exception("Received empty input parameters");
			}
	}
}
catch(Exception $e)
{
	$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id,"Reason"=>$e->getMessage()),Log::BOTH);
	$return_status = FALSE;
}
/* 
 * If the return status is false, then log it & send the 500 header as the 
 * response.
 */
if (!$return_status)
{
	$logging_obj->my_error_handler("INFO",array("Status"=>"Fail", "Result"=>500, "Reason"=>"Exception"),Log::BOTH);
	$GLOBALS['http_header_500'] = TRUE;
    header('HTTP/1.0 500 InMage method call returned an error',TRUE,500);
    header('Content-Type: text/plain');
}
if (!$GLOBALS['http_header_500'])
	$logging_obj->my_error_handler("INFO",array("Status"=>"Success"),Log::BOTH);
else
	$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
function read_installed_directory()
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
