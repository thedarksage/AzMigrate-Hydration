<?php

mysql_connect("localhost", "svsystems", "svsHillview") or  newdebugLog2("Connection Establishment failed  ".mysql_error());
mysql_select_db("svsdb1") or  newdebugLog2("Database selection failed  ".mysql_error());

$sql = "desc retLogPolicy";
$result = mysql_query($sql) or newdebugLog2("desc retLogPolicy query failed  ".mysql_error());
$flag = "false";
while ($retlogpolicy_obj = mysql_fetch_object($result))
{
	    if($retlogpolicy_obj->Field == 'logTypeStored')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN logTypeStored";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN logTypeStored  query execution failed  ".mysql_error());
			
		}
		if($retlogpolicy_obj->Field == 'ret_logupto_hrs')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN ret_logupto_hrs";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN ret_logupto_hrs query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'ret_logupto_days')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN ret_logupto_days";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN ret_logupto_days query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'type_of_policy')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN type_of_policy";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN type_of_policy query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'retSizeinbytes')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN retSizeinbytes";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN retSizeinbytes query execution failed  ".mysql_error());

			
		} 
		if($retlogpolicy_obj->Field == 'logdatadir')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN logdatadir";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN logdatadir query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'timebasedFlag')
        { 
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN timebasedFlag";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN timebasedFlag query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'retTimeInterval')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN retTimeInterval";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN retTimeInterval query execution failed  ".mysql_error());

			
		} 
		if($retlogpolicy_obj->Field == 'logsizethreshold')
        {
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN logsizethreshold";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN logsizethreshold query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'log_device_name')
        {
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN log_device_name";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN log_device_name query execution failed  ".mysql_error());

			
		} 
		if($retlogpolicy_obj->Field == 'temp_logdata_dir')
        { 
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN temp_logdata_dir";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN temp_logdata_dir query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'logPruningPolicy')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN logPruningPolicy";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN logPruningPolicy query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'moveRetentionPath')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN moveRetentionPath";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN moveRetentionPath query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'retStatus')
        {  
			$sql = "ALTER TABLE retLogPolicy DROP COLUMN retStatus";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy DROP COLUMN retStatus query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'retPolicyType')
        {  
			$sql = "ALTER TABLE retLogPolicy MODIFY retPolicyType TINYINT(1) UNSIGNED";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy MODIFY retPolicyType query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'retId')
        {  
			$sql = "ALTER TABLE retLogPolicy MODIFY retId BIGINT UNSIGNED AUTO_INCREMENT";
			mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy MODIFY retId query execution failed  ".mysql_error());

			
		}
		if($retlogpolicy_obj->Field == 'sparseEnable')
        {  
			$flag = "true";

			
		}
	

}

if($flag == "false")
{
	$sql = "ALTER TABLE retLogPolicy ADD (sparseEnable TinyInt(1) default 0)";
	mysql_query($sql) or newdebugLog2("ALTER TABLE retLogPolicy query execution failed  ".mysql_error());

}

$sql = "ALTER TABLE retentionWindow ADD CONSTRAINT retLogPolicy_retentionWindow FOREIGN KEY (retId) REFERENCES retLogPolicy (retId) ON DELETE CASCADE ON UPDATE CASCADE";
mysql_query($sql) or newdebugLog2("ALTER TABLE retentionWindow ADD CONSTRAINT retLogPolicy_retentionWindow query execution failed  ".mysql_error());

$sql = "ALTER TABLE retentionEventPolicy ADD CONSTRAINT retentionWindow_retentionEventPolicy FOREIGN KEY (Id) REFERENCES retentionWindow (Id) ON DELETE CASCADE ON UPDATE CASCADE";
mysql_query($sql) or newdebugLog2("ALTER TABLE retentionEventPolicy ADD CONSTRAINT retentionWindow_retentionEventPolicy FOREIGN KEY (Id) query execution failed  ".mysql_error());

$sql = "ALTER TABLE retentionSpacePolicy ADD CONSTRAINT retLogPolicy_retentionSpacePolicy FOREIGN KEY (retId) REFERENCES retLogPolicy (retId) ON DELETE CASCADE ON UPDATE CASCADE";
mysql_query($sql) or newdebugLog2("ALTER TABLE retentionSpacePolicy ADD CONSTRAINT retLogPolicy_retentionSpacePolicy FOREIGN KEY (retId) query execution failed  ".mysql_error());

function newdebugLog2 ($debugString)
{
  
  if (eregi('Windows', php_uname()))
  {
      $PHPDEBUG_FILE_PATH = "..\\var\\retention_schema_drop.log";
  }
  else
  { 
	  $PHPDEBUG_FILE_PATH = "/home/svsystems/var/retention_schema_drop.log";
  }
  # Some debug
  $fr = fopen($PHPDEBUG_FILE_PATH, 'a+');
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