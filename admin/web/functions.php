<?php
$functions_included = 1;
include_once( 'config.php' );
include_once( 'pair_settings.php' );

$commonfunctions_obj = new CommonFunctions();
$fx_obj = new FileProtection();

# Constants
$FRLOG_PATH = '/tmp/frlog';

# BUGBUG: If multiple hosts have the same name, this will only return the first one encountered
function get_host_id ($name)
{
  global $conn_obj;
  $name = $conn_obj->sql_escape_string($name);

  $hosts = $conn_obj->sql_query ("select id from hosts where name='$name'");
  $row = $conn_obj->sql_fetch_row($hosts);
  return $row [0];
}

function get_host_info( $id )
{
    global $conn_obj;

    $iter = $conn_obj->sql_query( "select a.id, a.name, a.sentinelEnabled, a.outpostAgentEnabled, a.filereplicationAgentEnabled from hosts a where a.id = '$id'");

    $myrow = $conn_obj->sql_fetch_row( $iter );
    $result = array( 'id' => $myrow[ 0 ], 'name' => $myrow[ 1 ], 'sentinelEnabled' => $myrow[ 2 ], 'outpostAgentEnabled' => $myrow[ 3 ], 'filereplicationAgentEnabled' => $myrow[4] );

    return( $result );
}

##
# Logs a debug string to a file.
##
function debugLog ( $debugString)
{
  /*global $PHPDEBUG_FILE;
  # Some debug
  $fr = fopen($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
    echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, $debugString . "\n")) {
    print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
    echo "Error! Couldn't close the file.";
  }*/
}


$days = array ("SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT", "WKD", "WKE", "ALL", "ONE", "MTH", "YRL");
$daysn = array_flip ($days);

$run = array ("ALLDAY", "NEVER", "DURING", "NOTDUR");
$runn = array_flip ($run);


##
# Returns an array containing the source id, source device name, destination id, and destination device name for a given ruleId
# Returns false if the ruleId is invalid

function get_pair ($ruleId)
{
  global $conn_obj;

  $result = $conn_obj->sql_query ("SELECT lh.name, lhv.devicename, rh.name, rhv.devicename, lh.id, rh.id " .
                         "FROM hosts lh, hosts rh, logicalVolumes lhv, " .
                         "logicalVolumes rhv, srcLogicalVolumeDestLogicalVolume map " .
                         "WHERE lh.id = lhv.hostId AND rh.id = rhv.hostId AND " .
                         "lh.id = map.sourceHostId AND rh.id = map.destinationHostId " .
                         "AND  lhv.deviceName = map.sourceDeviceName AND " .
                         " rhv.deviceName = map.destinationDeviceName AND " .
                         "map.ruleId=\"$ruleId\"");

  if ($conn_obj->sql_num_row ($result) != 1)
  {
    return false;
  }

  $row = $conn_obj->sql_fetch_row ($result);
  return $row;
}

##
# Debugging function that writes a line to a temporary log (/tmp/mytrace.log).
# The file is used to see the sequence of events between PHP and the TM.
# This function prepends line with "php: " and appends a linefeed.

function mylog($line)
{
    #$file = fopen( "/tmp/mytrace.log", "ab" );
    #fputs( $file, "php: " . $line . "\n" );
    #fclose( $file );
}

function is_windows ()
{
    if (preg_match('/Windows/i', php_uname())) { return true; }
    else { return false; }
}

function is_linux ()
{
    if (preg_match('/Linux/i', php_uname())) { return true; }
    else { return false; }
}

#The below function is implemented by Srinivas Patnana to construct the proper data log path before sending the data log path to the database, based on the given input and is for windows. This function is using at the time of snapshots(schedule and recovery) creation.
function verifyDatalogPath($dPath)
{
        $folderCollection = array();
        $dPathList = explode(":",$dPath);
        $count =  count($dPathList);
        #echo $dPathList[1];
        if ($count > 1)
        {
                $dPathTerms = explode("\\",$dPathList[1]);
                if (count($dPathTerms) > 1)
                {
                        #print_r($dPathTerms);
                        for($i=0;$i<count($dPathTerms);$i++)
                        {
                                if ($dPathTerms[$i]!='')
                                {
                                        $folderCollection[] = $dPathTerms[$i];
                                }
                        }
                        #print_r($folderCollection);
                        if (count($folderCollection) > 0)
                        {
                                $dPath = implode("\\",$folderCollection);
                                $dPath = $dPathList[0].':\\'.$dPath;
                        }// This is the else block if the path doesn't contain any sub folders and the path contains more slashes.
						else //If folder collection is zero
                        {
                                $dPath = $dPathList[0].':\\';
                        }

                }
        }
        #print_r($dPathTerms);
        return $dPath;
}
?>