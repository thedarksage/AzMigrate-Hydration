<?php
$amethyst_vars = amethyst_values();

$DB_HOST = $amethyst_vars['DB_HOST'];
$DB_ROOT_USER = $amethyst_vars['DB_ROOT_USER'];
$DB_ROOT_PASSWD = $amethyst_vars['DB_ROOT_PASSWD'];
$DB_DATABASE_NAME = $amethyst_vars['DB_NAME'];
	
mysql_connect($DB_HOST, $DB_ROOT_USER, $DB_ROOT_PASSWD) or  svsdb_upgrade_log("Connection Establishment failed  ".mysql_error());
mysql_select_db($DB_DATABASE_NAME) or  svsdb_upgrade_log("Database selection failed  ".mysql_error());

/*
 *  Following block is to add new columns to agentInstallerDetails.
 */
$sql = "desc agentInstallerDetails";
$agent_installer_result = mysql_query($sql) or svsdb_upgrade_log("desc agentInstallerDetails query failed  ".mysql_error());
$agent_installer_array = array('errCode' => 'false',
									'errPlaceHolders' => 'false');
while ($agent_installer_obj = mysql_fetch_object($agent_installer_result))
{	 
	if($agent_installer_obj->Field == 'errCode')
	{  
		$agent_installer_array['errCode'] = 'true';
	}		
	if($agent_installer_obj->Field == 'errPlaceHolders')
	{  
		$agent_installer_array['errPlaceHolders'] = 'true';
	}
}
if($agent_installer_array['errCode'] == "false")
{
	$sql = "alter table agentInstallerDetails add column `errCode` varchar(50) NOT NULL DEFAULT ''";
	mysql_query($sql) or svsdb_upgrade_log("alter table agentInstallerDetails add column `errCode` varchar(50) NOT NULL DEFAULT has failed ".mysql_error());
}
if($agent_installer_array['errPlaceHolders'] == "false")
{
	$sql = "alter table agentInstallerDetails add column `errPlaceHolders` text NOT NULL";
	mysql_query($sql) or svsdb_upgrade_log("alter table agentInstallerDetails add column `errPlaceHolders` text NOT NULL has failed ".mysql_error());
}

/*
 *  Following block is to add new columns to infrastructureInventory.
 */
$sql = "desc infrastructureInventory";
$infr_inv_result = mysql_query($sql) or svsdb_upgrade_log("desc infrastructureInventory query failed  ".mysql_error());
$infr_inv_array = array('errCode' => 'false',
									'errPlaceHolders' => 'false',
									'hostPort' => 'false');
while ($infr_inv_obj = mysql_fetch_object($infr_inv_result))
{	 
	if($infr_inv_obj->Field == 'errCode')
	{  
		$infr_inv_array['errCode'] = 'true';
	}		
	if($infr_inv_obj->Field == 'errPlaceHolders')
	{  
		$infr_inv_array['errPlaceHolders'] = 'true';
	}
    if($infr_inv_obj->Field == 'hostPort')
	{  
		$infr_inv_array['hostPort'] = 'true';
	}
}
if($infr_inv_array['errCode'] == "false")
{
	$sql = "alter table infrastructureInventory add column `errCode` varchar(50) NOT NULL DEFAULT ''";
	mysql_query($sql) or svsdb_upgrade_log("alter table infrastructureInventory add column `errCode` varchar(50) NOT NULL DEFAULT has failed ".mysql_error());
}
if($infr_inv_array['errPlaceHolders'] == "false")
{
	$sql = "alter table infrastructureInventory add column `errPlaceHolders` text NOT NULL";
	mysql_query($sql) or svsdb_upgrade_log("alter table infrastructureInventory add column `errPlaceHolders` text NOT NULL has failed ".mysql_error());
}
if($infr_inv_array['hostPort'] == "false")
{
	$sql = "alter table infrastructureInventory add column `hostPort` int(6) NOT NULL DEFAULT '443'";
	mysql_query($sql) or svsdb_upgrade_log("alter table infrastructureInventory add column `hostPort` int(6) NOT NULL has failed ".mysql_error());
}

/*
 *  Following block is to add new columns to infrastructureVMs.
 */
$sql = "desc infrastructureVMs";
$infr_vm_result = mysql_query($sql) or svsdb_upgrade_log("desc infrastructureVMs query failed  ".mysql_error());
$infr_vm_array = array('macIpAddressList' => 'false');
while ($infr_vm_obj = mysql_fetch_object($infr_vm_result))
{	 
	if($infr_vm_obj->Field == 'macIpAddressList')
	{  
		$infr_vm_array['macIpAddressList'] = 'true';
	}
}
if($infr_vm_array['macIpAddressList'] == "false")
{
	$sql = "alter table infrastructureVMs add column `macIpAddressList` text NOT NULL";
	mysql_query($sql) or svsdb_upgrade_log("alter table infrastructureVMs add column `macIpAddressList` text NOT NULL has failed ".mysql_error());
}

/*
 *  Following block is to add new columns to infrastructureVMs.
 */
$sql = "desc eventCodes";
$event_code_result = mysql_query($sql) or svsdb_upgrade_log("desc eventCodes query failed  ".mysql_error());
$event_code_array = array('errPossibleCauses' => 'false',
						'errorMsg' => 'false');
while ($event_code_obj = mysql_fetch_object($event_code_result))
{	 
	if($event_code_obj->Field == 'errPossibleCauses')
	{  
		$event_code_array['errPossibleCauses'] = 'true';
	}
	if($event_code_obj->Field == 'errorMsg')
	{  
		$event_code_array['errorMsg'] = 'true';
	}
}
if($event_code_array['errorMsg'] == "false")
{
	$sql = "alter table eventCodes add column `errorMsg` text NOT NULL after errType";
	mysql_query($sql) or svsdb_upgrade_log("alter table eventCodes add column `errorMsg` text NOT NULL has failed ".mysql_error());
}
if($event_code_array['errPossibleCauses'] == "false")
{
	$sql = "alter table eventCodes add column `errPossibleCauses` text NOT NULL after errorMsg";
	mysql_query($sql) or svsdb_upgrade_log("alter table eventCodes add column `errPossibleCauses` text NOT NULL has failed ".mysql_error());
}

$sql = "Select * from eventCodes where errCode IN ('AA0305','EC0146','EP0851','EP0852','EP0853','EP0854','EP0855','EP0856','EP0857','EP0858','EP0859','EP0860','AP0800','AP0801','EC0142','EC0143')";
$result = mysql_query($sql) or svsdb_upgrade_log("select * from eventCodes failed  ".mysql_error());

if(!mysql_num_rows($result))
{
	 $sql = "insert into eventCodes (errCode, errType, errCorrectiveAction, errComponent, errManagementFlag, errPossibleCauses, errorMsg) VALUES 
		('AA0305','VM health','Please extend the retention drive to increase its capacity or free up the space by deleting unwanted files or stale retention files, if any. Otherwise, re-protect the source server with lower retention window or to a different Master Target server.','MT','3','',''),
		('EC0146','VM health','Ensure that Application Agents are running on all the protected VMs in the plan. If this issue persists, contact Support.','Agent','3','',''),
        ('EP0851','Agent health','Ensure that Process server and configuration server are connected to the network and all pre-requisites are enabled. See http://go.microsoft.com/fwlink/?LinkId=525131 for detailed guidance on Push install pre-requisites.','Agent','3','Process server <ps ip> could not fetch job details from configuration server{cs ip:cs port}.',''),
        ('EP0852','Agent health','Ensure that Process server and configuration server are connected to the network and all pre-requisites are enabled. See http://go.microsoft.com/fwlink/?LinkId=525131 for detailed guidance on Push install pre-requisites.','Agent','3','Process server <ps ip> could not download software from configuration server{cs ip:cs port}.',''),
        ('EP0853','Agent health','Ensure that the process server has internet connectivity. Retry the job again after enabling all pre-requisites. See http://go.microsoft.com/fwlink/?LinkId=525131 for guidance.','Agent','3','Process server <ps ip> could not verify signature of Mobility service installation package.',''),
        ('EP0854','Agent health','Ensure WMI service(Windows) or the SSH service(Linux) on source machine is enabled. Retry the job again after enabling all pre-requisites. See http://go.microsoft.com/fwlink/?LinkId=525131 for guidance on configuring the source machine for Push install.','Agent','3','Process server <ps ip> could not execute job on {source machine ip} using {username}.',''),
        ('EP0855','Agent health','Ensure File & Printer services (Windows) or the SFTP service (Linux) on the source machine is enabled. Retry the job again after enabling all pre-requisites. See http://go.microsoft.com/fwlink/?LinkId=525131 for guidance on configuring the source machine for Push install.','Agent','3','Process server <ps ip> could not copy Mobility service to {source machine ip} using {username}.',''),
        ('EP0856','Agent health','See http://go.microsoft.com/fwlink/?LinkId=525131 for guidance on configuring the user accounts for Push install.','Agent','3','Connection to source machine{machine ip} using {username} failed due to either incorrect username or password.',''),
        ('EP0857','Agent health','Please ensure that the source machine\'s Linux variant is supported and all pre-requisites are enabled. See http://go.microsoft.com/fwlink/?LinkId=525131 for guidance on configuring the source machine for Push install.','Agent','3','Process server <ps ip> has failed to identify the linux variant of the {machine ip} using {username}.',''),
        ('EP0858','Agent health','The job log is located at <pushLogPath>. Please resolve issues in the job log and retry the operation.','Agent','3','Installation of the Mobility service has failed on the source machine {machine ip} using {username}.',''),
        ('EP0859','Agent health','Please ensure that the source machine has a valid ip address and is reachable from the Process server. See http://go.microsoft.com/fwlink/?LinkId=525131 for guidance on configuring the source machine for Push install.','Agent','3','',''),
        ('EP0860','Agent health','The job log is located at <pushLogPath>. Please resolve issues in the job log and retry the operation.','Agent','3','',''),
        ('AP0800','Agent health','Mobility service is already installed on <sourceIp>. No action required.','Agent','3','Mobility service is already installed.',''),
        ('AP0801','Agent health','Manually uninstall the older version of Mobility service and retry the operation.','Agent','3','A different version of Mobility service is already installed on source machine {machine ip}',''),
        ('EC0142','VM health','1. Ensure that the vCenter server is connected to the network and is accessible from the Process server. 2. Retry the operation with a valid set of login credentials.','Agent','3','Unable to connect to the vCenter server <hostip>:<port> from Process server <psname>. This can happen if either the vCenter server is not running or authentication failed due to invalid login credentials.',''),
        ('EC0143','VM health','Please install vCLI 5.1 on the Process server, restart the process server and retry the operation.','Agent','3','vCli not installed on the Process server <psname>','')";
	 mysql_query($sql) or svsdb_upgrade_log("INSERT TABLE  eventCodes VALUES execution failed  ".mysql_error());
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
    return $result;
}

function svsdb_upgrade_log($debugString)
{
	$update_log_path = "C:\\home\\svsystems\\var\\update1.log";
	
	# Some debug
	$fr = fopen($update_log_path, 'a+');
	if(!$fr) {
	#echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, $debugString . "\n")) {
	#print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) {
	#echo "Error! Couldn't close the file.";
	}
	exit(111);
}
?>