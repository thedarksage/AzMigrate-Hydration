<?php
$amethyst_vars = amethyst_values();

$DB_HOST = $amethyst_vars['DB_HOST'];
$DB_ROOT_USER = $amethyst_vars['DB_ROOT_USER'];
$DB_ROOT_PASSWD = $amethyst_vars['DB_ROOT_PASSWD'];
$DB_DATABASE_NAME = $amethyst_vars['DB_NAME'];
	
mysql_connect($DB_HOST, $DB_ROOT_USER, $DB_ROOT_PASSWD) or  raise_error("Connection Establishment failed with error  ".mysql_error());
mysql_select_db($DB_DATABASE_NAME) or  raise_error("Database selection failed with error  ".mysql_error());

/*
 *  Following block is to add new columns to agentInstallerDetails.
 */

$value_data = ($argv[1] == 'VPN') ? 'VPN' : 'NON VPN';
$sql = "UPDATE transbandSettings SET ValueData = '$value_data' WHERE ValueName = 'CONNECTIVITY_TYPE'";
mysql_query($sql) or raise_error("Database update failed with error ".mysql_error());

$sql = "UPDATE cfs SET connectivityType = '$value_data'";
mysql_query($sql) or raise_error("Database update failed with error ".mysql_error());


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

function raise_error($debugString)
{
	echo "$debugString";
	exit(111);
}
?>