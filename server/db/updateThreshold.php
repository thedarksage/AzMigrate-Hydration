<?PHP
if (preg_match('/Windows/i', php_uname()))
{
  $constants_file = "..\\admin\\web\\app\\Constants.php";
}
else
{
  $constants_file = "/home/svsystems/admin/web/app/Constants.php";
}
include_once $constants_file;
$config_file = $REPLICATION_ROOT.'admin'.$SLASH.'web'.$SLASH.'config.php';
include_once $config_file;

$conn = mysql_connect($DB_HOST,$DB_USER,$DB_PASSWORD);
mysql_select_db($DB_DATABASE_NAME);
$sql = "select sourceHostId, sourceDeviceName, destinationHostId, destinationDeviceName from srcLogicalVolumeDestLogicalVolume";
$res = mysql_query($sql);
$count = mysql_num_rows($res);
if($count > 0)
{
	while($row = mysql_fetch_object($res))
	{
		$sourceHostId = $row->sourceHostId;
		$sourceDeviceName = $row->sourceDeviceName;
		$destinationHostId = $row->destinationHostId;
		$destinationDeviceName = $row->destinationDeviceName;
		
		$maxResyncFilesThreshold = 17179869184; #16*1024*1024*1024 = 17179869184
		$maxDiffFilesThreshold = 68719476736; #64*1024*1024*1024 = 68719476736
	$updateSrc = "update srcLogicalVolumeDestLogicalVolume set maxResyncFilesThreshold = '$maxResyncFilesThreshold' where sourceHostId='$sourceHostId' and sourceDeviceName = '$sourceDeviceName' and destinationHostId='$destinationHostId' and destinationDeviceName='$destinationDeviceName'";
		$resSrc = mysql_query($updateSrc);

	$updateLgl = "update logicalVolumes set maxDiffFilesThreshold = '$maxDiffFilesThreshold' where hostId='$sourceHostId' and deviceName = '$sourceDeviceName'";
        $resLgl = mysql_query($updateLgl);	

	}


}


?>
