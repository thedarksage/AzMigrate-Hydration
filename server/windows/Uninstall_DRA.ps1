$msiUninstallLog = $Env:SystemDrive + "\ProgramData\ASRSetupLogs\DRAUninstall.log"
write-output "Uninstalling the DRA Agent" >> $msiUninstallLog
$app = Get-WmiObject -Class Win32_Product | Where-Object {
	$_.Name -match "Microsoft Azure Site Recovery Provider"}
$identityNumber = $app.IdentifyingNumber
if ($identityNumber)
{
	write-output $identityNumber >> $msiUninstallLog
	$identifier = $identityNumber.Substring(1,$identityNumber.Length-2)
	MsiExec.exe /qn /x "{$identifier}" /L+*V $msiUninstallLog
}