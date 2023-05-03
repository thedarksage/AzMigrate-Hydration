<?php
ob_start();
header('Content-Type: text/html; charset=utf-8');
$request_uri_string = $_SERVER['REQUEST_URI'];
$match_status = preg_match('@^/ui/@',$request_uri_string);
if ($match_status)
{
	session_start();
}
date_default_timezone_set(@date_default_timezone_get());
include_once("app/Logging.php");
include_once("app/Constants.php");
function _load_amethyst_config ()
{
  global $REPLICATION_ROOT, $AMETHYST_CONF,$AMETHYST_PATH_CONF;
  global $MAX_TMID;
  global $DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME, $DB_TYPE, $DB_ROOT_USER, $DB_ROOT_PASSWD;
  global $CXPS_CONNECTTIMEOUT,$CXPS_RESPONSETIMEOUT;
  global $CXPS_USER_WINDOWS, $CXPS_PASSWORD_WINDOWS, $CXPS_USER_LINUX, $CXPS_PASSWORD_LINUX;
  global $IN_COMPANY, $IN_VX_SENTINEL, $IN_VX_OUTPOST, $IN_CX_DRSCOUT, $IN_FX_FILEREP, $IN_APPLABEL;
  global $MAX_RESYNC_FILES_THRESHOLD,$MAX_DIFF_FILES_THRESHOLD;
  global $MIN_DISK_FREESPACE;
  global $IN_PROFILER;
  global $DEFAULT_VX_AGENT_TIMEOUT;
  global $SSL_CERT_PASSWORD, $SSL_KEY_PASSWORD, $SSL_CIPHER_LIST;
  global $INSTALLATION_DIR;
  global $CX_MODE, $CS_IP, $CS_PORT, $PS_CS_PORT;
  global $LOG_LEVEL;
  global $CLUSTER_IP, $CLUSTER_NAME;
  global $HOST_GUID;
  global $CS_IP,$PS_CS_IP, $CX_TYPE;
  global $INSTALLER_PATH,$LINUX_AGENT_INSTALL_PATH,$WINDOW_AGENT_INSTALL_PATH, $CONFIG_WEB_ROOT;
  global $CUSTOM_BUILD_TYPE, $ACTUAL_INSTALLATION_DIR;
  global $SLASH,$MYSQL,$MYSQL_PATH,$MYSQL_DUMP,$CHMOD,$FIND,$XARGS,$CP,$MV,$TAR,$LN,$CS_SECURED_COMMUNICATION,$PS_CS_SECURED_COMMUNICATION,$CLIENT_AUTHENTICATION,$PHP_PATH, $DVACP_PORT, $BATCH_SIZE,$DB_TRANSACTION_RETRY_SLEEP,$DB_TRANSACTION_RETRY_LIMIT,$ENABLE_API_PROFILE,$IP_ADDRESS_FOR_AZURE_COMPONENTS;
  global $enable_storage_monitoring, $MAX_PHP_CONNECTION_RETRY_LIMIT;
  global $FILE_OPEN_RETRY,$SLEEP_RETRY_WINDOW,$MYSQL_DATA_PATH, $IS_APPINSIGHTS_LOGGING_ENABLED, $IS_MDS_LOGGING_ENABLED;
  global $DRA_ALLOW_LOCALHOST_COMMUNICATION;

  $file =array();
	
	$cs_conf_path = normalizeWindowsPath($REPLICATION_ROOT.$AMETHYST_PATH_CONF);
	$file = FALSE;
	$retry = 0;
	while (($file == FALSE) and ($retry < $FILE_OPEN_RETRY)) 
	{
		$file = @file($cs_conf_path);
		if ($file == FALSE)
		{
			sleep($SLEEP_RETRY_WINDOW);
			$retry++;
		}
	}
	if (($retry == $FILE_OPEN_RETRY) && ($file == FALSE))
	{
		// Erases the output buffer if any.
		ob_clean();
		$GLOBALS['http_header_500'] = TRUE;
		// Send 500 response 
		header("HTTP/1.0 500 Internal server error: File open failed with retry $retry: $cs_conf_path", true);
		
		exit(0);
	}
  
  foreach ($file as $line)
  {
    $matches = array ();

    if (preg_match ('/([\w]+)[\s]*=[\s]*"(.+)"/', $line, $matches) && !preg_match('/^[\s]*#/', $line))
    {
      switch ($matches [1])
      {
      case 'MAX_TMID':
	$MAX_TMID         = $matches [2];
	break;

      case 'DB_HOST':
	$DB_HOST          = $matches [2];
	break;
      case 'DB_USER':
	$DB_USER          = $matches [2];
	break;
      case 'DB_PASSWD':
	$DB_PASSWORD      = $matches [2];
	break;
      case 'DB_NAME':
	$DB_DATABASE_NAME = $matches [2];
	break;
      case 'DB_TYPE':
	$DB_TYPE = $matches [2];
	break;
	case 'DB_ROOT_USER':
	$DB_ROOT_USER = $matches [2];
	break;
        case 'DB_ROOT_PASSWD':
	$DB_ROOT_PASSWD = $matches [2];
	break;
	case 'CXPS_USER_WINDOWS':
		$CXPS_USER_WINDOWS   = $matches [2];
	break;
      case 'CXPS_PASSWORD_WINDOWS':
	$CXPS_PASSWORD_WINDOWS     = $matches [2];
	break;
      case 'CXPS_USER_LINUX':
	$CXPS_USER_LINUX         = $matches [2];
      break;
      case 'CXPS_PASSWORD_LINUX':
	$CXPS_PASSWORD_LINUX     = $matches [2];
	break;
      case 'CXPS_CONNECTTIMEOUT':
        $CXPS_CONNECTTIMEOUT = intval($matches [2]);
        break;
      case 'CXPS_RESPONSETIMEOUT':
       $CXPS_RESPONSETIMEOUT = intval($matches[2]);
       break;
      case 'IN_COMPANY':
        $IN_COMPANY       = $matches [2];
      break;
      case 'IN_VX_SENTINEL':
        $IN_VX_SENTINEL   = $matches [2];
      break;
      case 'IN_VX_OUTPOST':
        $IN_VX_OUTPOST    = $matches [2];
      break;
      case 'IN_CX_DRSCOUT':
        $IN_CX_DRSCOUT    = $matches [2];
      break;
      case 'IN_FX_FILEREP':
        $IN_FX_FILEREP    = $matches [2];
      break;

      case 'MAX_RESYNC_FILES_THRESHOLD':
        $MAX_RESYNC_FILES_THRESHOLD = $matches [2];
      break;
      case 'MAX_DIFF_FILES_THRESHOLD':
        $MAX_DIFF_FILES_THRESHOLD = $matches [2];
      break;
      case 'MIN_DISK_FREESPACE':
        $MIN_DISK_FREESPACE = $matches [2];
      break;

      case 'IN_PROFILER':
        $IN_PROFILER      = $matches [2];
      break;

      case 'IN_APPLABEL':
        $IN_APPLABEL      = $matches [2];
      break;

      case 'DEFAULT_VX_AGENT_TIMEOUT':
        $DEFAULT_VX_AGENT_TIMEOUT  = $matches [2];
      break;

      case 'SSL_CERT_PASSWORD':
        $SSL_CERT_PASSWORD = $matches [2];
      break;

      case 'SSL_KEY_PASSWORD':
        $SSL_KEY_PASSWORD  = $matches [2];
      break;

      case 'SSL_CIPHER_LIST':
        $SSL_CIPHER_LIST   = $matches [2];
      break;

      case 'INSTALLATION_DIR':
        $INSTALLATION_DIR   = $matches [2];
      break;

      case 'CX_MODE':
        $CX_MODE   = $matches [2];
      break;
	  
      case 'CS_IP':
        $CS_IP   = $matches [2];
      break;

      case 'CS_PORT':
        $CS_PORT   = $matches [2];
      break;
	  
	case 'LOG_LEVEL':
        $LOG_LEVEL = $matches [2];
      break;

 	case 'CLUSTER_IP':
       $CLUSTER_IP = $matches [2];
      break;
	  
	case 'CLUSTER_NAME':
       $CLUSTER_NAME = $matches [2];
      break;  
	
	case 'HOST_GUID':
	    $HOST_GUID = $matches [2];
	    break;
	    
	 case 'CS_IP':
     $CS_IP = $matches [2];
     break;
   
      case 'INSTALLER_PATH':
     $INSTALLER_PATH = $matches [2];
     break;

      case 'WINDOW_AGENT_INSTALL_PATH':
     $WINDOW_AGENT_INSTALL_PATH = $matches [2];
     break;

      case 'LINUX_AGENT_INSTALL_PATH':
     $LINUX_AGENT_INSTALL_PATH = $matches [2];
     break;

      case 'WEB_ROOT':
     $CONFIG_WEB_ROOT = $matches [2];
     break;
	 
	case 'CUSTOM_BUILD_TYPE':
	$CUSTOM_BUILD_TYPE = $matches [2];
	break;
	
	case 'PS_CS_IP':
		$PS_CS_IP = $matches [2];
		break;
		
	case 'PS_CS_PORT':
		$PS_CS_PORT = $matches [2];
		break;
		
	case 'PS_CS_SECURED_COMMUNICATION':
		$PS_CS_SECURED_COMMUNICATION = $matches [2];
		break;	
	
	case 'ACTUAL_INSTALLATION_DIR':
		$ACTUAL_INSTALLATION_DIR = $matches [2];
		break;
	
	case 'MYSQL_PATH':
		if(preg_match ('/Windows/i', php_uname())) {
			$MYSQL_PATH = $matches [2].$SLASH;
			$MYSQL = '"'.$matches [2].$SLASH.'bin'.$SLASH.'mysql'.'"';
			$MYSQL_DUMP = '"'.$matches [2].$SLASH.'bin'.$SLASH.'mysqldump'.'"';
		}
		break;
		
	case 'MYSQL_DATA_PATH':
		$MYSQL_DATA_PATH = $matches [2].$SLASH;
		break;
	
	case 'CYGWIN_PATH':
		if(preg_match ('/Windows/i', php_uname())) 
		{
			$CYGWIN_PATH = $matches [2].$SLASH.'bin'.$SLASH;
			$CHMOD = $CYGWIN_PATH."chmod";
			$FIND = $CYGWIN_PATH."find";
			$XARGS = $CYGWIN_PATH."xargs";
			$CP = $CYGWIN_PATH."cp";
			$MV = $CYGWIN_PATH."mv";
			$TAR = $CYGWIN_PATH."tar";
			$LN = $CYGWIN_PATH."ln";
		}
		break;
        
	case 'CS_SECURED_COMMUNICATION':
		$CS_SECURED_COMMUNICATION = $matches [2];
		break;
        
	case 'CX_TYPE':
		$CX_TYPE = $matches [2];
		break;
        
	case 'CLIENT_AUTHENTICATION':
		$CLIENT_AUTHENTICATION = $matches [2];
		break;        

   case 'DVACP_PORT':
		$DVACP_PORT = $matches [2];
		break;    

	case 'DB_TRANSACTION_RETRY_SLEEP':
		$DB_TRANSACTION_RETRY_SLEEP = $matches [2];
		break;
		
	case 'DB_TRANSACTION_RETRY_LIMIT':
		$DB_TRANSACTION_RETRY_LIMIT = $matches [2];
		break;
		
	case 'MAX_PHP_CONNECTION_RETRY_LIMIT':
		$MAX_PHP_CONNECTION_RETRY_LIMIT = $matches [2];
		break;
		
	case 'enable_storage_monitoring':
		$enable_storage_monitoring = $matches [2];
		break;
        
	case 'PHP_PATH':
		$PHP_PATH = empty($matches [2]) ? "php.exe" : ($matches [2] . "\\php.exe");
		break;
		
	case 'BATCH_SIZE':
		$BATCH_SIZE = $matches [2];
		break;
	case 'ENABLE_API_PROFILE':
		$ENABLE_API_PROFILE = $matches [2];
	break; 
	case 'IS_APPINSIGHTS_LOGGING_ENABLED':
		$IS_APPINSIGHTS_LOGGING_ENABLED = $matches [2];
	break;
	case 'IS_MDS_LOGGING_ENABLED':
		$IS_MDS_LOGGING_ENABLED = $matches [2];
	break;
	case 'IP_ADDRESS_FOR_AZURE_COMPONENTS':
		$IP_ADDRESS_FOR_AZURE_COMPONENTS = $matches [2];
	break;
	case 'DRA_ALLOW_LOCALHOST_COMMUNICATION':
		$DRA_ALLOW_LOCALHOST_COMMUNICATION = $matches [2];
	break;
      }
    }
  }
  
  if(preg_match ('/Windows/i', php_uname()) && empty($PHP_PATH))
  {
	$PHP_PATH = "php.exe";
  }

  $WEB_ROOT=$_SERVER['DOCUMENT_ROOT'];
}

$CLOUD_CONTAINER_REGISTRY_HIVE_FILE = "cloud_container_registry.reg";

$logging_obj = new Logging($LOG_ROOT."phpdebug.txt","phpdebug");

_load_amethyst_config ();

$LDAP_support = (function_exists('ldap_connect')) ? TRUE : FALSE;

$log_rollback_string = "";
$page_load_start_time = microtime(true);
/*
	This function will estabilish a common resource handler
	and it is storing in a global resource handler

*/

function normalizeWindowsPath($dPath)
{
	$folderCollection = array();
	$dPathList = explode(":",$dPath);
	$count =  count($dPathList);
	
	if ($count > 1)
	{
		$dPathTerms = explode("\\",$dPathList[1]);
		
		if (count($dPathTerms) > 1)
		{
				
				for($i=0;$i<count($dPathTerms);$i++)
				{
					if ($dPathTerms[$i]!='')
					{
						$folderCollection[] = $dPathTerms[$i];
					}
				}
			
				if (count($folderCollection) > 0)
				{
						$dPath = implode("\\",$folderCollection);
						$dPath = $dPathList[0].':\\'.$dPath;
				}
				/*
				 This is the else block if the path doesn't contain any
				 sub folders and the path contains more slashes.
				 If folder collection is zero
				*/
				else 
				{
					$dPath = $dPathList[0].':\\';
				}
		}
		else
		{
		   $dPath = str_replace(':', ':\\', $dPath);
		}
	}
	
	return $dPath;
}

function custom_autoloader($class_name) 
{
    global $REPLICATION_ROOT;
	global $FILE_OPEN_RETRY,$SLEEP_RETRY_WINDOW;

	$class_types = array ('admin\\web\\cs\\classes\\conn\\', 'admin\\web\\cs\\classes\\data\\','admin\\web\\app\\','admin\\web\\','admin\\web\\ScoutAPI\\','system\\admin\\app\\','admin\\web\\cs\\classes\\log\\');
	
	foreach ($class_types as $class_type) 
	{
		$class_file = $REPLICATION_ROOT.$class_type.$class_name.".php";
		$class_file = normalizeWindowsPath($class_file);
		if (file_exists ($class_file)) 
		{
			$done = false;
			$retry = 0;
			while (!$done and ($retry < $FILE_OPEN_RETRY)) 
			{
				$done = @include_once $class_file;
				if (!$done)
				{
					sleep($SLEEP_RETRY_WINDOW);
					$retry++;
				}
			}
			
			if (($retry == $FILE_OPEN_RETRY) && !$done)
			{
				// Erases the output buffer if any.
				ob_clean();
				$GLOBALS['http_header_500'] = TRUE;
				// Send 500 response as no database connection created
				header("HTTP/1.0 500 Internal server error: {$retry} retries for include failure of {$class_file}", true);
				
				exit(0);
			}
			break;
		}	
   }
}

spl_autoload_register('custom_autoloader');

$commonfunctions_obj = new CommonFunctions();

if (get_magic_quotes_gpc()==1){
 # Ensuring that if magic quotes is on magic_quotes_runtime and magic_quotes_sybase should be turn off
 # bacuse it will over ride magic_quotes_gp setting , in normal case both of these entries are Off
 ini_set("magic_quotes_runtime","Off");
 ini_set("magic_quotes_sybase","Off");
}
extract($_POST, EXTR_OVERWRITE);
extract($_GET, EXTR_OVERWRITE);

if($CX_TYPE != 2)
{
	include_once("cs/classes/conn/Connection.php");
	$conn_obj = new Connection();
}

//print_r($_SESSION['lcode']);
if($CUSTOM_BUILD_TYPE != 3) include_once("ui/lang_nls.php");
$lan_code = $_SESSION['lcode'];
?>
