<?php 
ini_set('memory_limit', '128M');
if (preg_match('/Linux/i', php_uname())) 
{
	include_once 'config.php';
}
else
{
	include_once 'config.php';
}

$commonfunctions_obj = new CommonFunctions();
$scenario_id = $argv[1];

$scenario_id_str = (string)$scenario_id ;
//If the string contains all digits, then only allow, otherwise throw 500 header.
////This call from tmansvc CS event back end call. In case data mismatch throwing 500 header and next event will trigger periodically.
if (isset($scenario_id) && $scenario_id != '' && (!preg_match('/^\d+$/',$scenario_id_str))) 
{
	$commonfunctions_obj->bad_request_response("Invalid post data plan scenario id $scenario_id in file update_secondary_scenario .");
}

$sql = "SELECT scenarioDetails FROM applicationScenario WHERE scenarioId='$scenario_id'";
$sql_result = $conn_obj->sql_query($sql);
$data = $conn_obj->sql_fetch_object($sql_result);
debugtest13 ( "sql::$sql");
$xml_data = $data->scenarioDetails;
debugtest13 ( "xml_data::$xml_data");
$cdata = $commonfunctions_obj->getArrayFromXML($xml_data);
$plan_properties = unserialize($cdata['plan']['data']['value']);
debugtest13 ( "plan_properties::".print_r($plan_properties,true));

if($plan_properties['planType'] == 'BULK')
{
	if(isset($plan_properties['primaryProtectionInfo']))
	{
		$plan_properties['Servers'] = $plan_properties['primaryProtectionInfo']['Servers'];
		$plan_properties['Cluster Servers'] = $plan_properties['primaryProtectionInfo']['Cluster Servers'];
		$plan_properties['Xen Servers']= $plan_properties['primaryProtectionInfo']['Xen Servers'];
		$plan_properties['Shared Devices'] = $plan_properties['primaryProtectionInfo']['Shared Devices'];
	}
}
unset($plan_properties['primaryProtectionInfo']);
debugtest13 ( "plan_properties::".print_r($plan_properties,true));

$cdata = serialize($plan_properties);
$str = "<plan><header><parameters>";
$str.= "<param name='name'>".$plan_properties['planName']."</param>";
$str.= "<param name='type'>Protection Plan</param>";
$str.= "<param name='version'>1.1</param>";
$str.= "</parameters></header>";
$str.= "<data><![CDATA[";
$str.= $cdata;
$str.= "]]></data></plan>";
$str = $conn_obj->sql_escape_string($str);

$sql = "UPDATE applicationScenario SET scenarioDetails='$str' WHERE scenarioId='$scenario_id'";
$sql_result = $conn_obj->sql_query($sql);
debugtest13 ( "sql::$sql");

function debugtest13 ( $debugString)
{
    global $REPLICATION_ROOT;
    if(preg_match ("/Linux/i", php_uname())) 
    {    
        $PHPDEBUG_FILE = '/home/svsystems/update_scenario.txt';
    }
    else
    {
        $PHPDEBUG_FILE = "$REPLICATION_ROOT\\home\\svsystems\\update_scenario.txt";
    }
    

  $fr = fopen($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
	echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, $debugString . "\n")) {
	print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
	echo "Error! Couldn't close the file.";
  }
}
?>