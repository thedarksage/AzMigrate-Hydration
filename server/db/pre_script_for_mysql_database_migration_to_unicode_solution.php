<?
try
{
error_reporting(E_ALL & ~E_NOTICE);
set_time_limit(0);
ini_set('memory_limit', '-1');
ini_set('display_errors', '1');

global $backup_completed;
$backup_completed = TRUE;

if (preg_match('/Windows/i', php_uname()))
{
	$mysql_conf_file_path = "C:\\Program Files (x86)\\MySQL\\MySQL Server 5.5\\my.ini";
	$mysql_conf_file_path_backup = "C:\\Program Files (x86)\\MySQL\\MySQL Server 5.5\\my.ini.backup";
}
else
{
	$mysql_conf_file_path = "/etc/my.cnf";
	$mysql_conf_file_path_backup = "/etc/my.cnf.backup";
}

 $mysql_status   = check_mysqld_status("running");

#print $mysql_status;
#exit;
print "Verifying database server configuration...\n";
if ($mysql_status == TRUE)
{
$amethyst_vars = amethyst_values();
$svsdb1_exists = is_db_server_has_svsdb1_database($amethyst_vars);
if ($svsdb1_exists == TRUE)
{
#print_r($amethyst_vars);
#exit;
$db_server_in_latin1=is_db_server_moved_to_unicode_solution($amethyst_vars);
#print $db_server_in_latin1;
#exit;
if ($db_server_in_latin1)
{
	print "Verifying database health...\n";
	 $db_health_status = db_health($amethyst_vars);
	#print "db health".$db_health_status;
	#exit;
         if ($db_health_status == TRUE)
         {
	$tman_status = check_tman_status();
	if($tman_status == TRUE)
	{
		$tman_service_status =  act_tmansvc_service("stop");

		#print "entered to stop service";
		#exit;

/*
		0 Successful execution of command

		1 command fails because of an error during expansion or redirection, the exit status is greater than zero.

		2 Incorrect command usage

		126 Command found but not executable

		127 Command not found 
*/
	}
	
	if (preg_match('/Windows/i', php_uname()))
	{
		print "scheduler service status checking..\n";
		$scheduler_service_status = 'sc query INMAGE-AppScheduler | findstr "STOPPED RUNNING"';
		$output = system($scheduler_service_status, $ret_val);
		if($ret_val != 0) throw new Exception ( "$scheduler_service_status \n$output", 100 );	
		migrationLog("Scheduler service $scheduler_service_status, status $ret_val\n\n");
		if(stristr($output, 'running'))
		{
			$scheduler_service_status = act_scheduler_service("start");
		}
	}
	
	$web_service_status = check_web_service_status("running");
	 #print $web_service_status;
       	#exit;

	if ($web_service_status == TRUE)
	{
		$web_service_status = act_web_service("stop");
		#print "entered for web service stop";
		#exit;
	}
	#exit;
		$web_service_status = check_web_service_status("stopped");	

		#print $web_service_status;
		#exit;
		if ($web_service_status)
		{
		$backup_completed = FALSE;

		print "Collecting database backup is in progress..\n";
		$svsdb1_dump_cmd = "mysqldump -u ".$amethyst_vars['DB_ROOT_USER']." -p".$amethyst_vars['DB_ROOT_PASSWD']." svsdb1  > svsdb1-dump-backup.sql";
		$output = system($svsdb1_dump_cmd, $ret_val);
		if($ret_val != 0) throw new Exception ( "$svsdb1_dump_cmd \n$output", 101 );
		sleep(3);
		
		if (!copy($mysql_conf_file_path, $mysql_conf_file_path_backup)) 
		{
			throw new Exception ( "Configuration file failed to copy from  $mysql_conf_file_path to $mysql_conf_file_path_backup", 102 );
		}
		
		$restore_svsdb1_to_tmp_db = "mysql -u ".$amethyst_vars['DB_ROOT_USER']." -p".$amethyst_vars['DB_ROOT_PASSWD']." svsdb1 < svsdb1-alter-statements.sql";
		$output = system($restore_svsdb1_to_tmp_db, $ret_val);
		if($ret_val != 0) throw new Exception ( "$restore_svsdb1_to_tmp_db \n$output", 102 );
		sleep(3);
		
		$svsdb1_dump_cmd = "mysqldump -u ".$amethyst_vars['DB_ROOT_USER']." -p".$amethyst_vars['DB_ROOT_PASSWD']." svsdb1  > svsdb1-dump.sql";
		$output = system($svsdb1_dump_cmd, $ret_val);
		if($ret_val != 0) throw new Exception ( "$svsdb1_dump_cmd \n$output", 103 );
		sleep(3);

		if (file_exists('svsdb1-dump.sql'))
		{	
			$db_def_data = file_get_contents('svsdb1-dump.sql');
			if ($db_def_data === FALSE)
			{
				throw new Exception ("svsdb1 dump data read failed.",134);
			}
		#print  $db_def_data;
		#exit;
		if (preg_match('/Windows/i', php_uname()))
		{
			$data = str_ireplace("CHARSET=latin1", "CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC ", $db_def_data);
		}
		else
		{
			$data = str_ireplace("CHARSET=latin1", "CHARSET=utf8 ", $db_def_data);
		}
		#print  $data;
		#exit;
		$charset_modification = file_put_contents("svsdb1-dump.sql", $data);
		if ($charset_modification === FALSE)
		{
			throw new Exception("Character set replacment in dump file failed.", 135);
		}
	
		$svsdb1_drop_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "drop database if exists svsdb1"';
		$output = system($svsdb1_drop_cmd, $ret_val);
		if($ret_val != 0) throw new Exception ( "$svsdb1_drop_cmd \n$output", 104 );

		$mysqld_service_status = act_mysqld_service("stop");	
		#print $mysqld_service_status;
		#exit;
		$mysql_status   = check_mysqld_status("stopped");
		#print $mysql_status;
		#exit;
		if($mysql_status == TRUE)
		{	
			update_db_server_configuration_file_with_unicode_solution_configuration($mysql_conf_file_path);
			
			$mysqld_service_status = act_mysqld_service("start");
			$mysql_status   = check_mysqld_status("running");

			if($mysql_status == TRUE)
			{				
				$svsdb1_create_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "create database if not exists svsdb1"';
				$output = system($svsdb1_create_cmd , $ret_val);
				if($ret_val != 0) throw new Exception ( "$svsdb1_create_cmd  \n$output", 105 );
				
				$restore_svsdb1_to_tmp_db = "mysql -u ".$amethyst_vars['DB_ROOT_USER']." -p".$amethyst_vars['DB_ROOT_PASSWD']." svsdb1 < svsdb1-dump.sql";
				$output = system($restore_svsdb1_to_tmp_db, $ret_val);
				if($ret_val != 0) throw new Exception ( "$restore_svsdb1_to_tmp_db \n$output", 106 );
				sleep(3);
			}
			else
			{
				print "MySql service is not starting. Please contact support.\n";
				throw new Exception ( "MySql service is not starting. Please contact support.", 107);
			}
			$backup_completed = TRUE;
			
		}
		 else
                {
                        print "Web service is not able to stop. Please contact support.\n";
			throw new Exception("Web service is not able to stop. Please contact support.",108);
                }
		}
		else
		{
			print "svsdb1 database dump file not exists. Please contact support.\n";
			throw new Exception("svsdb1 database dump file not exists. Please contact support.",133);
		}

		}
		else
		{
			print "MySql service is not able to stop. Please contact support.\n";
			throw new Exception("MySql service is not able to stop. Please contact support.",109);
		}
		}
		else
		{
			print "DB health is not ok";
			throw new Exception("Database health is no ok. Please contact support.",110);
		}
	}
	}
	else
	{
		print "svsdb1 database not present in database server. Please contact suppport.\n";
		throw new Exception("svsdb1 database not present in database server. Please contact suppport.",111);
	}
}
else
{
     	print "MySql service is already in stopped state. Please contact support.\n";
	throw new Exception("MySql service is already in stopped state. Please contact support.",112);
}
	
}
catch(Exception $e)
{
		
	$error_string = "Time : " . date("Y-m-d H:i:s");
	#$error_string = "\nVersion : " . trim($version);
	$error_string .= "\nCode : " . $e->getCode();
	$error_string .= "\nMessage : " . $error_message = $e->getMessage();
	$error_string .= "\nFile : " . $e->getFile();
	$error_string .= "\nLine : " . $e->getLine();
	migrationLog("Exception occured at instruction:: $error_string \n\n");

	$return_value = db_restore($amethyst_vars);
	if ($return_value == 1)
	{
		migrationLog("DB restore failed.\n\n");
		//Need to contact support here.
	}
	else
	{
		migrationLog("DB restore success.\n\n");
	}
	
}
$web_service_status = check_web_service_status("stopped");
if ($web_service_status == TRUE)
{
	$web_service_status = act_web_service("start");
}

$tman_status = check_tman_status();
if ($tman_status == FALSE)
{
	$tman_service_status =  act_tmansvc_service("start");
}

if (preg_match('/Windows/i', php_uname()))
{
	 print "scheduler service status checking..\n";
                $scheduler_service_status = 'sc query INMAGE-AppScheduler | findstr "STOPPED RUNNING"';
                $output = system($scheduler_service_status, $ret_val);
                if($ret_val != 0) throw new Exception ( "$scheduler_service_status \n$output", 113 );
		migrationLog("Scheduler service $scheduler_service_status, status $ret_val\n\n");

                if(stristr($output, 'stopped'))
		{
			$scheduler_service_status = act_scheduler_service("start");
		}
}
function check_web_service_status($service_status)
{
	 $web_service_status = FALSE;
        if (preg_match('/Windows/i', php_uname()))
        {
                print "w3svc web service status checking..\n";
                $w3svc_service_status = 'sc query w3svc | findstr "STOPPED RUNNING"';
        }
        else
        {
                print "HTTPD web service status checking..\n";
                $w3svc_service_status = 'service httpd status';
        }
        $output = system($w3svc_service_status, $ret_val);
        #print_r($output);
        $status_result = stristr($output, $service_status);
        if($status_result)
        {
                $web_service_status = TRUE;
        }
        if (preg_match('/Windows/i', php_uname()))
        {
                if($ret_val != 0) throw new Exception ( "$w3svc_service_status \n$output", 114);
        }
        else
        {
                if($ret_val != 0 and $ret_val!=3) throw new Exception ( "$w3svc_service_status \n$output", 115);
        }
	migrationLog("Checking web service status $w3svc_service_status, status $ret_val, return $web_service_status.\n\n");
	return  $web_service_status;
}

function act_scheduler_service($action)
{
	$scheduler_service_action  = FALSE;
	if ($action == "start")
        {
                print "Scheduler service is starting..\n";
        }
        elseif ($action == "stop")
        {
                print "Scheduler service is stopping..\n";
        }
	 $scheduler_service = "net $action INMAGE-AppScheduler";
        $output = system($scheduler_service, $ret_val);
        if($ret_val != 0) throw new Exception ( "$scheduler_service \n$output", 116);
        sleep(5);
	$scheduler_service_action = TRUE;
	migrationLog("Scheduler service $action, $scheduler_service, status $ret_val, return $scheduler_service_action.\n\n");

	return $scheduler_service_action;
}

function act_tmansvc_service($action)
{
 $tman_service_action = FALSE;
        if ($action == "start")
        {
                print "tmansvc service is starting..\n";
        }
        elseif ($action == "stop")
        {
                print "tmanagerd service is stopping..\n";
        }
        if (preg_match('/Windows/i', php_uname()))
        {
                $tmansvc_service = "net $action tmansvc";
        }
        else
        {
		if ($action == "stop")
		{
			`killall -9 volsync 2> /dev/null`;
			sleep(2);
		}
                $tmansvc_service = "service tmanagerd $action";
        }
        $output = system($tmansvc_service, $ret_val);
        if($ret_val != 0) throw new Exception ( "$tmansvc_service \n$output", 117);
        sleep(5);
        $tman_service_action = TRUE;
	migrationLog("Tmanagerd service $action, $tmansvc_service, status $ret_val, return $tman_service_action.\n\n");
        return $tman_service_action;
}

function act_web_service($action)
{
	$web_service_action = FALSE;

		
                if (preg_match('/Windows/i', php_uname()))
                {
                        #print "it is running";
                        $w3svc_service = "net $action w3svc";
			 if ($action == "start")
        		{
                		print "w3svc service is starting..\n";
        		}
        		elseif ($action == "stop")
        		{
                		print "w3svc service is stopping..\n";
        		}

                }
                else
                {
                        $w3svc_service = "service httpd $action";
			if ($action == "start")
        		{
                		print "HTTPD service is starting..\n";
        		}
        		elseif ($action == "stop")
        		{
                		print "HTTPD service is stopping..\n";
        		}
                }
                $output = system($w3svc_service, $ret_val);
                if($ret_val != 0) throw new Exception ( "$w3svc_service \n$output", 118);
                sleep(5);
	$web_service_action = TRUE;
	migrationLog("Web service $action, $w3svc_service, status $ret_val, return $web_service_action.\n\n");
	return $web_service_action;
}

function update_db_server_configuration_file_with_unicode_solution_configuration($mysql_conf_file_path)
{
	$file_data_array = array();
	 $file_data_array = file($mysql_conf_file_path);
	if ($file_data_array != FALSE)
	{
                        if (preg_match('/Windows/i', php_uname()))
                        {
                                $file_data_array[] = "[mysql]";
                                $file_data_array[] = "default-character-set = utf8mb4";
                        }
                        else
                        {
                                $file_data_array[] = "[client]";
                                $file_data_array[] = "default-character-set = utf8";
                        }

                        foreach($file_data_array as $key=>$value)
                        {
                                        $value = str_replace(array("\r\n","\r","\n"),"",$value);
                                        $final_data_array[] = $value;
                                        if ($value == "[client]")
                                        {
                                                if (preg_match('/Windows/i', php_uname()))
                                                {

                                                        $final_data_array[] = "default-character-set = utf8mb4";
                                                }
                                                else
                                                {
                                                        $final_data_array[] = "default-character-set = utf8";
                                                }
                                        }
                                        if ($value == "[mysql]")
                                        {
                                                if (preg_match('/Windows/i', php_uname()))
                                                {
                                                        $final_data_array[] = "default-character-set = utf8mb4";
                                                }
                                                else
                                                {
                                                         $final_data_array[] = "default-character-set = utf8";
                                                }
                                        }
					 if ($value == "[mysqld]")
                                        {

                                                if (preg_match('/Windows/i', php_uname()))
                                                {
                                                        $final_data_array[] = "character-set-client-handshake=FALSE";
                                                        $final_data_array[] = "character-set-server=utf8mb4";
                                                        $final_data_array[] = "collation-server=utf8mb4_unicode_ci";
                                                }
                                                else
                                                {
                                                        $final_data_array[] = "character-set-client-handshake=FALSE";
                                                        $final_data_array[] = "character-set-server=utf8";
                                                        $final_data_array[] = "collation-server=utf8_general_ci";
                                                }
                                        }
                                        if ($value == "[mysql.server]")
                                        {

                                                if (preg_match('/Windows/i', php_uname()))
                                                {

                                                        $final_data_array[] = "default-character-set = utf8mb4";
                                                }
                                                else
                                                {
                                                        $final_data_array[] = "default-character-set = utf8";
                                                }
                                        }
                                        if ($value == "[safe_mysqld]")
                                        {
                                                if (preg_match('/Windows/i', php_uname()))
                                                {

                                                        $final_data_array[] = "default-character-set = utf8mb4";
                                                }
                                                else
                                                {
                                                        $final_data_array[] = "default-character-set = utf8";
                                                }
                                        }

                        }

                        $final_data_array = array_values($final_data_array);
                        #print_r($final_data_array);
                        #exit;

                        $file_write= file_put_contents($mysql_conf_file_path,implode("\n",$final_data_array));
                        #print "================================\n";

			if ($file_write === FALSE)
			{
				migrationLog("MySql configuration file update failed. Error code 119.\n\n");
				throw new Exception ( "MySql configuration file update failed.", 119);
			}
			else
			{
	 			migrationLog("Updating database server configuration with unicode solution configuration.\n\n");
			}
	}
	else
	{
		migrationLog("MySql configuration file read failed. Error code 120.\n\n");
		throw new Exception ( "MySql configuration file read failed.", 120);
	}
}


function db_restore($amethyst_vars)
{
	try
	{
	global $backup_completed, $mysql_conf_file_path_backup, $mysql_conf_file_path;

	$return_value = 1;
	#var_dump($backup_completed);
	#exit;	

	//Restore applies only exception occured after db config and backup done. That means migration not able to complete successfully.
	if ($backup_completed == FALSE)
	{

                $mysql_status   = check_mysqld_status("running");

		  #$mysqld_service_status = act_mysqld_service("start");
                   #     $mysql_status   = check_mysqld_status("running");

		#print $mysql_status;
		#exit;
		if ($mysql_status == TRUE)	
		{
			$mysqld_service_status = act_mysqld_service("stop");
		}

			$mysql_status   = check_mysqld_status("stopped");

		#print $mysql_status;
		#exit;
		if ($mysql_status == TRUE)
                {
			if (file_exists($mysql_conf_file_path_backup) and file_exists($mysql_conf_file_path))
    			{
			if (!copy($mysql_conf_file_path_backup,$mysql_conf_file_path)) 
			{
				print  "Config file failed to copy back from $mysql_conf_file_path_backup to $mysql_conf_file_path...\n";
				migrationLog("Config file failed to copy back from $mysql_conf_file_path_backup to $mysql_conf_file_path...\n\n");
				throw new Exception ( "Config file failed to copy back from $mysql_conf_file_path_backup to $mysql_conf_file_path.", 121);
				return 0;
			}
		
			$mysqld_service_status = act_mysqld_service("start");
			$mysql_status   = check_mysqld_status("running");

                		if ($mysql_status == TRUE)
                		{
				#print "yes it is ruuning";
				#exit;
				$svsdb1_create_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "create database if not exists svsdb1"';
				$output = system($svsdb1_create_cmd , $ret_val);
				if($ret_val != 0) 
				{
					print "Database name verification failed at restore. Please contact support";
					migrationLog("Database name verification failed at restore. Please contact support.\n\n");
					throw new Exception ( "Database name verification failed at restore. Please contact support.", 122);
					return 0;
				}
		
				$restore_svsdb1_to_tmp_db = "mysql -u ".$amethyst_vars['DB_ROOT_USER']." -p".$amethyst_vars['DB_ROOT_PASSWD']." svsdb1 < svsdb1-dump-backup.sql";
				$output = system($restore_svsdb1_to_tmp_db, $ret_val);
				if($ret_val != 0)
				{
					print "Database restore failed. Please contact support";
					migrationLog("Database restore failed. Please contact support.\n\n");
					throw new Exception("Database restore failed. Please contact support.",123);
					return 0;
				}
				}
			}
			else
			{
			 	print "MySql configuration files not found. Please contact support";
				migrationLog("MySql configuration files not found. Please contact support.\n\n");
				throw new Exception("MySql configuration files not found. Please contact support.",124);
				return 0;	
			}
		}
		else
		{
			print "MySql service is not able to stop. Please contact suppport";
			migrationLog("MySql service is not able to stop. Please contact suppport.\n\n");
			throw new Exception("MySql service is not able to stop. Please contact suppport.",125);
                       	return 0;
		}
	}
	return $return_value;
	}
	catch(Exception $e)
	{
        $error_string = "Time : " . date("Y-m-d H:i:s");
        #$error_string = "\nVersion : " . trim($version);
        $error_string .= "\nCode : " . $e->getCode();
        $error_string .= "\nMessage : " . $error_message = $e->getMessage();
        $error_string .= "\nFile : " . $e->getFile();
        $error_string .= "\nLine : " . $e->getLine();
        migrationLog("Exception occured at instruction:: $error_string \n\n");
	return 0;
	}
}

//get amethyst values
function amethyst_values()
{
    if (preg_match('/Windows/i', php_uname()))
    {
      $amethyst_conf = "..\\etc\\amethyst.conf";
    }
    else
    {
      $amethyst_conf = "/home/svsystems/etc/amethyst.conf";
    }
    
    if (file_exists($amethyst_conf))
    {
        $file = fopen ($amethyst_conf, 'r');
        $conf = fread ($file, filesize($amethyst_conf));
        fclose ($file);
        $conf_array = explode ("\n", $conf);
        foreach ($conf_array as $line)
        {
            if (!preg_match ("/^#/", $line))
            {
                list ($param, $value) = explode ("=", $line);
                $param = trim($param);
                $value = trim($value);
                $result[$param] = str_replace('"', '', $value);
            }
        }
    }
    migrationLog("Reading amethyst file content from $amethyst_conf file.\n");
    migrationLog(print_r($result,TRUE)."\n\n");
    return $result;
}

function db_health($amethyst_vars)
{
	$mysql_health_status_file = "mysql_health_status.txt";
	$check_cmd = "mysqlcheck -u ".$amethyst_vars['DB_ROOT_USER']." -p".$amethyst_vars['DB_ROOT_PASSWD']." svsdb1 > ".$mysql_health_status_file;
	#print $check_cmd;
	#exit;
	$cmd_op = system($check_cmd,$ret_val);
	$health_flag = 1;
	$conf_array = array();
	if(file_exists($mysql_health_status_file))
	{
		if (is_readable($mysql_health_status_file)) 
		{
		$file_stat = stat($mysql_health_status_file);
		#print_r($file_stat);
		#print $file_stat['size'];
		#clearstatcache();
		if ($file_stat['size'] > 0)
		{
			$file = fopen ($mysql_health_status_file, 'r');
			$status_file = fread ($file, $file_stat['size']);
			#print_r($status_file);
			fclose ($file);
			$conf_array = explode ("\n", $status_file);
			#print_r($conf_array);
			foreach ($conf_array as $line)
			{
				if (!preg_match ("/^#/", $line))
				{
					if(isset($line) && ($line != ""))
					{
						if(substr(trim($line),-2) != "OK")
						{
							$health_flag = 0;
							break;
						}
					}
				}
			}
		}
		}
	}
	migrationLog("Checking svsdb1 health and status is $health_flag.\n");

	return $health_flag;
}


function is_db_server_moved_to_unicode_solution($amethyst_vars)
{
	$db_server_in_latin1 = TRUE;
	 if (preg_match('/Windows/i', php_uname()))
        {
		$svsdb1_create_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "show variables like \'%character_set_server%\'"';
	}
	else
	{
		$svsdb1_create_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "show variables like \'%character_set_server%\'"';
	}
       	$output = system($svsdb1_create_cmd , $ret_val);
	if($ret_val != 0) throw new Exception ( "$svsdb1_create_cmd \n$output", 126);
#print $output;
 	if(stristr($output, 'utf8') || stristr($output, 'utf8mb4') )
	{
        	$db_server_in_latin1 = FALSE;
	}
	migrationLog("Database server character set verificaiton unicode support status $db_server_in_latin1.\n\n");
	return $db_server_in_latin1;
}

function is_db_server_has_svsdb1_database($amethyst_vars)
{
	 $db_server_has_svsdb1 = FALSE;
         if (preg_match('/Windows/i', php_uname()))
        {
                $svsdb1_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "show databases like \'svsdb1\'"';
        }
        else
        {
                $svsdb1_cmd = 'mysql -u '.$amethyst_vars['DB_ROOT_USER'].' -p'.$amethyst_vars['DB_ROOT_PASSWD'].' -e "show databases like \'svsdb1\'"';
        }
        $output = system($svsdb1_cmd , $ret_val);

        if($ret_val != 0) throw new Exception ( "$svsdb1_cmd \n$output", 127);

#print $output;
        if(stristr($output, 'svsdb1'))
        {
                $db_server_has_svsdb1 = TRUE;
        }
        migrationLog("Database server  $svsdb1_cmd, svsdb1 status $ret_val, return $db_server_has_svsdb1.\n\n");
        return $db_server_has_svsdb1;
}

function check_mysqld_status($service_status)
{
 	$mysql_status = FALSE;
        print "MySql service status checking..\n";
        if (preg_match('/Windows/i', php_uname()))
        {
                $mysql_service_status = 'sc query mysql | findstr "STOPPED RUNNING"';
        }
        else
        {
                $mysql_service_status = 'service mysqld status';
        }
        $output = system($mysql_service_status, $ret_val);
	$status_result = stristr($output, $service_status);
        if($status_result)
        {
                        $mysql_status = TRUE;
        }

        if (preg_match('/Windows/i', php_uname()))
        {
                        if($ret_val != 0) throw new Exception ( "$mysql_status \n$output", 128);
        }
        else
        {
                if($ret_val != 0 and $ret_val!=3) throw new Exception ( "$mysqld_status \n$output", 129);
        }

	migrationLog("MySql service status checking for $service_status, $mysql_service_status with exit status $ret_val, return value $mysql_status\n\n");
	return $mysql_status;
}

function act_mysqld_service($action)
{
        $mysqld_service_action = FALSE;
	if ($action == "start")
	{
		print "MySql service is starting..\n";
	}
	elseif ($action == "stop")
	{
	 	print "MySql service is stopping..\n";
	}	
        if (preg_match('/Windows/i', php_uname()))
      	{
        	$mysql_service = "net $action mysql";
    	}
      	else
     	{
       		$mysql_service = "service mysqld $action";
       	}
     	$output = system($mysql_service, $ret_val);
      	if($ret_val != 0) throw new Exception ( "$mysql_status \n$output", 130);
      	sleep(5);
	$mysqld_service_action = TRUE;
	migrationLog("MySql service $action, $mysql_service, status $ret_val, return $mysqld_service_action.\n\n");
	return $mysqld_service_action;
}

function check_tman_status()
{
	$tman_status = FALSE;
        if (preg_match('/Windows/i', php_uname()))
        {
                        print "tmansvc service status checking..\n";
                        $tmansvc_service_status = 'sc query tmansvc | findstr "STOPPED RUNNING"';
                        $output = system($tmansvc_service_status, $ret_val);
                        if(stristr($output, 'running'))
                        {
                                        $tman_status = TRUE;
                        }
                        if($ret_val != 0) throw new Exception ( "$tmansvc_service_status \n$output", 131);
			migrationLog("Checking tmanagerd service $tmansvc_service_status, status $ret_val, return value $tman_status");
        }
        else
        {
                        print "tmanagerd service status checking..\n";
                        $eventmanager_service_status = 'pgrep eventmanager >/dev/null';
                        $output = system($eventmanager_service_status, $eventmanager_ret_val);

                        $volsync_service_status = 'pgrep volsync >/dev/null';
                        $output = system($volsync_service_status, $volsync_ret_val);

                        $scheduler_service_status = 'pgrep scheduler >/dev/null';
                        $output = system($scheduler_service_status, $scheduler_ret_val);

                        $bpm_service_status = 'pgrep bpm >/dev/null';
                        $output = system($scheduler_service_status, $bpm_ret_val);

                        $pushinstalld_service_status = 'pgrep pushinstalld >/dev/null';
                        $output = system($scheduler_service_status, $puhsinstall_ret_val);

                        /*
                        pgrep exit status codes are as below..
                        0.One or more processes matched the criteria.
                        1.No processes matched.
                        2.Syntax error in the command line.
                        3.Fatal error: out of memory etc.
                        */
                        if (($eventmanager_ret_val > 1) or ($volsync_ret_val > 1) || ($scheduler_ret_val > 1) || ($bpm_ret_val > 1) or ($puhsinstall_ret_val > 1))
                        {
                                throw new Exception ( "$tmansvc_service_status \n$output", 132);
                        }
                        else
                        if (($eventmanager_ret_val == 0) or ($volsync_ret_val == 0) || ($scheduler_ret_val == 0) || ($bpm_ret_val == 0) or ($puhsinstall_ret_val == 0))
                        {
                                        $tman_status = TRUE;
                        }

			#print "Checking tmanagerd service $tmansvc_service_status , status eventmanager:$eventmanager_ret_val --volsync:$volsync_ret_val --scheduler: $scheduler_ret_val --bpm:$bpm_ret_val --pushinstall:$puhsinstall_ret_val, return value $tman_status";
		       	migrationLog("Checking tmanagerd service $tmansvc_service_status , status eventmanager:$eventmanager_ret_val --volsync:$volsync_ret_val --scheduler: $scheduler_ret_val --bpm:$bpm_ret_val --pushinstall:$puhsinstall_ret_val, return value $tman_status");

        }
	return $tman_status;
}

function migrationLog ($debugString)
{
	$debugString = gmdate("Y")."-".gmdate("m")."-".gmdate("d")." ".gmdate("H").":".gmdate("i").":".gmdate("s").":".$debugString;
	if (preg_match('/Windows/i', php_uname()))
	{
	  $log_path = "c:\\home\\svsystems\\var\\";
	}
	else
	{ 
	  $log_path = "/home/svsystems/var/";
	}

	$LOG_FILE_PATH =  $log_path."database_server_migration.log";
	# Some debug
	$fr = fopen($LOG_FILE_PATH, 'a+');
	if(!$fr) {
	#echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, $debugString . "\n")) {
	#print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) {
	#echo "Error! Couldn't close the file.";
	}

}
?>
