# Declare the date and time variables.
$Date = Get-Date -Format dd_MMM_yyyy
$Time = Get-Date -Format hh_mm_tt

# Read parameters
$BranchName=$args[0]
$Incremental=$args[1]
$SrcPath=$args[2]
$Config=$args[3]

# Declare the variables for directory structure and log name.
$FolderPath = "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date"
$Log_Name = $BranchName+"_mainlog_daily_"+$Config+"_build_"+$Date+"_"+$Time+".txt"

# Services needed for build
$DisabledServices = @("Spooler", "PeerDistSvc", "WinRM")
$RequiredServices= @("Apache2.2")

# Create the directory structure if does not exist.
if (!(Test-Path H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date ))
{
	foreach ($Path in "","UnifiedAgent_Builds")
	{
		foreach ($Config_Type in "release","debug")
		{
			New-Item -Path $FolderPath\$Path -name $Config_Type -ItemType directory
		}
	}
}

# Disable and stop services that use Port 80
foreach ($service in $DisabledServices) {
	"Disabling and Stopping service: "+$service
	Set-Service -Name $service -StartupType Disabled
	Stop-Service -Name $service
}

# Enable and start apache service
foreach ($service in $RequiredServices) {
	"Enabling and Starting service: "+$service
	Set-Service -Name $service -StartupType Automatic
	Start-Service -Name $service
}
		
# Take backup of the bcf file as .orig.
Copy-Item BuildConfig.bcf BuildConfig.bcf.orig

# Replace BRANCH_NAME, INC_VALUE, SRCPATH_VALUE and CONFIG_VALUE place holders with parameters.
( Get-Content BuildConfig.bcf ) | Foreach-Object {$_ -replace "BRANCH_NAME",$BranchName} | set-content BuildConfig.bcf
( Get-Content BuildConfig.bcf ) | Foreach-Object {$_ -replace "INC_VALUE",$Incremental} | set-content BuildConfig.bcf
( Get-Content BuildConfig.bcf ) | Foreach-Object {$_ -replace "SRCPATH_VALUE",$SrcPath} | set-content BuildConfig.bcf
if ($Config -eq "release")
{
	( Get-Content BuildConfig.bcf ) | Foreach-Object {$_ -replace "CONFIG_VALUE","release"} | set-content BuildConfig.bcf
}
if ($Config -eq "debug")
{ 
	( Get-Content BuildConfig.bcf ) | Foreach-Object {$_ -replace "CONFIG_VALUE","debug"}  | set-content BuildConfig.bcf
} 

#Triggering the build.
cmd.exe /C 'perl "winbld.pl" "file=BuildConfig.bcf"' > "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date\$Log_Name" 2>&1

#Copying back the backup file(.orig) to original file(.bcf).
Copy-Item BuildConfig.bcf.orig BuildConfig.bcf