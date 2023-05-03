<?php
include_once("app/Constants.php");
include_once("config.php");

#upload_log("File upload request\n".print_r($_SERVER,true)."\n");
#upload_log("File upload request");

# Error structure
$file_errors = array(            
            1 => "The uploaded file exceeds the upload_max_filesize directive (".ini_get("upload_max_filesize").") in php.ini",
            2 => "The uploaded file exceeds the MAX_FILE_SIZE directive that is specified in the HTML form.",
            3 => "The file upload failed, as there is no space left on the server's temporary directory.",
            4 => "No file is uploaded.",
            6 => "Missing the temporary directory.");
			
# HTTP error reponses			
$msg_array = array(
			1 => "An unknown file upload error occured.",
			2 => "Not a valid upload directory.",
			3 => "File Checksum not matching.",
			4 => "Failed to create destination path",
			5 => "Error in input parameters.",
			6 => "Not a valid file extension.");

# For allowed directories getting the base directory			
global $REPLICATION_ROOT, $CS_INSTALLATION_PATH;
$SLASH = "/";

$file_data = array();
#upload_log("FILES:: \n".print_r($_FILES, TRUE));
#upload_log("POST:: \n".print_r($_POST, TRUE));
foreach($_FILES as $key => $file)
{
	list($str, $index) = explode('_', $key);
	
	$arr['file'] = $file;
	if($index != '')
	{
		$arr['file_name'] = ($_POST['file_name_'.$index]) ? $_POST['file_name_'.$index] : basename($_POST['file_path_'.$index]);
		$arr['file_path'] = $_POST['file_path_'.$index];
		$arr['file_checksum'] = $_POST['file_checksum_'.$index];
	}
	else
	{
		$arr['file_name'] = ($_POST['file_name']) ? $_POST['file_name'] : basename($_POST['file_path']);
		$arr['file_path'] = $_POST['file_path'];		
		$arr['file_checksum'] = $_POST['file_checksum'];
	}	
	
	if($arr['file_path'])
	{
		$file_data[] = $arr;
	}
}

#upload_log("file_data IS ".print_r($file_data, TRUE));

$error_flag = 0;
#upload_log("File upload request file_data ".print_r($file_data,true)."\n allowed_dirs::".print_r($allowed_dirs,true));
if(count($file_data) > 0)
{
	for($i=0; $i<count($file_data); $i++)
	{		
		$file_info = $file_data[$i];
		$file_path = $file_info['file_path'];
		$file_path = trim($file_path);
		
        # Validating with allowed directories
		$is_valid_file_path_to_process = $commonfunctions_obj->is_valid_file_path($CS_INSTALLATION_PATH.$file_path);
        #upload_log("File upload request is_valid_file_path_to_process for file $file_path: ".$is_valid_file_path_to_process."\n");
        
		# File upload error
		if($file_info['file']['error'])
		{
			$error_flag = '1';
			
			$msg = $file_errors[$file_info['file']['error']];
			if($msg) $msg_array[$error_flag] = $msg;			
		}
		# Not a valid allowed directory
		elseif($is_valid_file_path_to_process == false) 
		{
			$error_flag = '2';
		}
		#Not a valid file extension
		elseif (strtolower(pathinfo($file_info['file_name'])['extension']) != "txt")
		{
			$error_flag = '6';
		}
		# Checksum verification failed
		elseif ($file_info['file_checksum'] && ($file_info['file_checksum'] != md5_file($file_info['file']['tmp_name']))) 
		{
			#upload_log("Checksum : ".md5_file($file_info['file']['tmp_name']));
			$error_flag = '3';
		}
		# Verify file path
		else
		{
			#upload_log("Entered for file path to check \n ".$file_path);
			if(strtolower($file_path[0]) != 'c') 
			{
				$file_path = $CS_INSTALLATION_PATH.$file_path;
			}
			if(!file_exists($file_path))
			{	
				#upload_log("File path not exists \n ".$file_path);
				mkdir($file_path, 0777, true);
				if(!is_dir($file_path))
				{
					$error_flag = '4';
				}
			}
		}
		clearstatcache();
		if($error_flag) break;
	}
}
else
{
	$error_flag = '5';
}
#upload_log("File upload request error_flag:$error_flag ");
if(!$error_flag)
{	
	$error_not_raised = 1;
	foreach($file_data as $key => $file_info)
	{
		$uploadfile = $CS_INSTALLATION_PATH.$file_info['file_path'].$SLASH. $file_info['file']['name'];		
		$status = move_uploaded_file($file_info['file']['tmp_name'], $uploadfile);
		if($status == FALSE)
		{
			tmp_file_cleanup($file_data);
			upload_log("ERROR : Failed to move the uploaded file".$uploadfile);
			$GLOBALS['http_header_500'] = TRUE;
			header('HTTP/1.1 500 Failed to move the uploaded file');
			$error_not_raised = 0;
			break;
		}		
	}
	if($error_not_raised)
	{
		#upload_log("Successfully uploaded file(s)");
		header('HTTP/1.1 200 Succssfully uploaded');
	}
}
else
{
	tmp_file_cleanup($file_data);
	$error_msg = $msg_array[$error_flag];
	#upload_log("File upload request file_data ".print_r($file_data,true)."\n");
	upload_log("ERROR : $error_msg, File upload request error_flag:$error_flag");
	$GLOBALS['http_header_500'] = TRUE;	
	header('HTTP/1.1 500 '.$error_msg);	
}

function upload_log ( $debugString)
{
	global $REPLICATION_ROOT;
	
	include_once("C:/ProgramData/ASR/home/svsystems/admin/web/app/CommonFunctions.php");
	$PHPDEBUG_FILE = "$REPLICATION_ROOT/var/file_upload.log";
	$commonfunctions_obj = new CommonFunctions();
	$fr = $commonfunctions_obj->file_open_handle($PHPDEBUG_FILE, 'a+');
	
	$date_time = date('Y-m-d H:i:s (T)');
	if(!$fr) 
	{
        // return if file handle not available.
        return;
	  #echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, $date_time." : ".$_SERVER['REMOTE_ADDR']." : ".$debugString . "\n")) 
	{
	  #print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) {
	  #echo "Error! Couldn't close the file.";
	}
}

function tmp_file_cleanup($file_data)
{
	global $FILE_OPEN_RETRY, $SLEEP_RETRY_WINDOW;
	
	foreach($file_data as $key => $file_info)
	{
		if (file_exists ($file_info['file']['tmp_name'])) 
		{
			$done = false;
			$retry = 0;
			while (!$done and ($retry < $FILE_OPEN_RETRY)) 
			{
				#upload_log("Loop: Deleting tmp file, retry $retry , retrymax $FILE_OPEN_RETRY \n".$file_info['file']['tmp_name']);
				$done = @unlink($file_info['file']['tmp_name']);
				if (!$done)
				{
					sleep($SLEEP_RETRY_WINDOW);
					$retry++;
				}
			}
		}
	}
}
?>