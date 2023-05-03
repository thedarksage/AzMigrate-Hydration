Write-Output "************ START InmSrcLogCollector.ps1 ************"
Write-Output "START AgentHostInfo"
$SerialNumber = (Get-WmiObject -Class Win32_Bios).SerialNumber
Write-Output "SerialNumber=$SerialNumber"

$UUID = (Get-WmiObject Win32_ComputerSystemProduct).UUID 
Write-Output "UUID=$UUID"

$os = Get-WmiObject win32_operatingsystem
$lastbootuptime = ($os.ConvertToDateTime($os.lastbootuptime))
$uptime = (Get-Date) - $lastbootuptime
$uptimeSec = ($uptime.Ticks)/10000000 
Write-Output "SystemUpTime=$uptimesec sec, LastBootupTime=$lastbootuptime"

$svagentspid = (Get-Process -Name svagents).Id
$svagentsstarttime = (Get-Process -Name svagents).starttime
Write-Output "SvagentPid=$svagentspid, StartTime=$svagentsstarttime"

$colItems = get-wmiobject -class "Win32_NetworkAdapterConfiguration" | Where{$_.IpEnabled -Match "True"}
foreach ($objItem in $colItems){
	$DHCPEnabled=$objItem.DHCPEnabled
	$IPAddress=$objItem.IPAddress
	$MACAddress=$objItem.MACAddress
	
	Write-Output "MACAddress=$MACAddress, DHCPEnabled=$DHCPEnabled, IPAddress=$IPAddress"
}
Write-Output "END AgentHostInfo"
Write-Output "************ END InmSrcLogCollector.ps1 ************"
exit 0