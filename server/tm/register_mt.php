<?php
##
#
# register_mt.php: It's main purpose in life is to invoke itself and verify the 
#                   registration of MT to CS
#              
##
date_default_timezone_set(@date_default_timezone_get());

$deploy_conf_file = "/home/svsystems/var/deploy_config_file.txt";
$pid_file = "/home/svsystems/var/register_mt.pid";

#Verifying if already running
if(file_exists($pid_file))
{
    # Read the PID from the exisitng file		
    $pid = file_get_contents($pid_file);
            
    # Trim the PID from the file
    $pid = trim($pid);
    
    # Verify if the PID is running
    $running = check_pid_existence($pid);
    if($running)
    {
        log_message("EXCEPTION","An instance of this thread is already running.\n");
        exit(1);
    }
}
    
$system_jobs_interval = "60";

file_put_contents($pid_file, getmypid());

while(1)
{		
    $check_exit = updateConfig();
    if($check_exit) exit(0);
    sleep($system_jobs_interval);
}	

function check_pid_existence($process_id)
{    
    if (preg_match('/Windows/i', php_uname()))    
    {
        $process = shell_exec("wmic path Win32_PerfFormattedData_PerfProc_Process where IDProcess=\"".$process_id."\" get IDProcess");
        return ((substr($process , 0, 2)) === "No") ? 0 : 1;
    }
    else
    {
        return file_exists( "/proc/$process_id" );
    }
}

function get_deploy_config()
{
    global $deploy_conf_file;
    
	if(!file_exists($deploy_conf_file))
	{
		log_message("INFO","get_deploy_config : $deploy_conf_file not found");
		return;
	}
    
    $contents = explode("\n",file_get_contents($deploy_conf_file));    
    foreach($contents as $line)
    {
		$values = explode("=",$line);
        $key = trim($values[0]);
        if($key != "") $conf_hash[$key] = trim($values[1]);        
    }
    
    return $conf_hash;
}

function updateConfig()
{
	global $deploy_conf_file;
    
	if(!file_exists($deploy_conf_file))
	{
		log_message("INFO","updateConfig : $deploy_conf_file not found");
		return;
	}
	$conf_hash = get_deploy_config();
	
    if(!$conf_hash)
    {
        log_message("INFO","updateConfig : No Config Params\n");
		return;
    }	
	log_message("INFO","updateConfig : ".print_r($conf_hash,TRUE));
        
    $component = $conf_hash['COMPONENT'];
    if($component != "MT")
    {
        log_message("EXCEPTION","updateConfig : Invalid component type. Component = '".$component."'");
        return; 
    }
	
    $command = get_config_command($conf_hash);
    if(!$command)
    {
        log_message("EXCEPTION","updateConfig : Couldn't get the command : $command");
        return; 
    }	
	
	log_message("INFO","updateConfig : Executing the command : $command\n");
	exec($command,$output,$return_status);
	if($return_status == 0)
	{
		log_message("INFO","updateConfig : Successfully executed the command\n");
		unlink($deploy_conf_file);
        return 1;
	}
    else
    {
        log_message("EXCEPTION","updateConfig : Command Failed. Command : ".$command);
    }
	log_message("INFO","updateConfig : Exiting\n");
    return;
}

function get_config_command($conf_hash)
{
    $install_dir_file_path = "/home/svsystems/var/VX_Install_Path.txt";
    if(!file_exists($install_dir_file_path))
    {
        log_message("EXCEPTION","get_config_command : $install_dir_file_path not found");
        return;
    }
    $install_dir = trim(file_get_contents($install_dir_file_path));
    
    if (preg_match('/Windows/i', php_uname()))
	{
        $exe_path = $install_dir."\\Cloud\\PostDeployConfigMT.exe";
        if(!$exe_path || (! file_exists($exe_path)))
        {
            $logging_obj->log_message("EXCEPTION","get_config_command : Cannot find the executable at \"$exe_path\"");
            return;
        }
        $command = '"'.$exe_path.'"'." /verysilent /suppressmsgboxes /i ".$conf_hash['CS_IP_ADDRESS']." /p ".$conf_hash['PORT']." /a ".$conf_hash['IP_ADDRESS']." /g ".$conf_hash['HOST_GUID']." /h ".$conf_hash['HOSTNAME'];
    }
    else
    {                
        $exe_path = $install_dir."/Cloud/PostDeployConfigMT.sh";
        if(!$exe_path || (! file_exists($exe_path)))
        {
            log_message("EXCEPTION","get_config_command : Cannot find the executable at \"$exe_path\"");
            return;
        }
        $command = $exe_path." -i ".$conf_hash['CS_IP_ADDRESS']." -p ".$conf_hash['PORT']." -a ".$conf_hash['IP_ADDRESS']." -g ".$conf_hash['HOST_GUID'];
        $command .= (isset($conf_hash['HOSTNAME'])) ? " -H ".$conf_hash['HOSTNAME'] : "";
    }
    return $command;
}

function log_message($error_level,$error_message)
{
    $log_file_name = "/home/svsystems/var/register_mt.log";
    
    $date_time = date('Y-m-d H:i:s (T)');
    
    $log_data_string = $date_time." : ".$error_level." : "
		.$error_message."    ". __FILE__ ."(". __LINE__ .")\n\n";
        
    error_log($log_data_string,3,$log_file_name);
}

?>
