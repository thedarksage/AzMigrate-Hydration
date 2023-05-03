param(
	[Parameter(Mandatory=$True)][string] $BranchRunPrefix,
	[Parameter(Mandatory=$True)][string][ValidateSet("true","false")] $EnableMailReporting,
	[Parameter(Mandatory=$True)][string] $GQLBinPath,
	[Parameter(Mandatory=$True)][string] $OSTag,
	[Parameter(Mandatory=$True)][string] $PrimaryLocation,
	[Parameter(Mandatory=$True)][string] $RecoveryLocation,
	[Parameter(Mandatory=$True)][string] $SubscriptionId,
	[Parameter(Mandatory=$True)][string] $SubscriptionName,
	[Parameter(Mandatory=$True)][string] $TestBinRoot,
	[Parameter(Mandatory=$True)][string] $VaultRGLocation,
	[Parameter(Mandatory=$True)][string] $LabAccntUserName,
	[Parameter(Mandatory=$True)][string] $LabAccntPassword
)

if ($(Test-Path -Path $TestBinRoot)) {
	Remove-Item $TestBinRoot -Recurse
}
Copy-Item -Path $GQLBinPath -Destination $TestBinRoot -Recurse 

. $TestBinRoot\OS.ps1

Function PerformGQL()
{
	param([string] $OSName)

	$OSType = $distros[$OSName][0]

	Log "Start GQL on $OSType -> $OSName"
			
	Start-process powershell.exe -ArgumentList "-file", $TestBinRoot\WTT.ps1, $BranchRunPrefix, $EnableMailReporting, $OSName, $OSType, $PrimaryLocation, $RecoveryLocation, $SubscriptionId, `"$SubscriptionName`", $VaultRGLocation, $LabAccntUserName, $LabAccntPassword
}

$OSList = GetOsList $OSTag

if ($OSList.count -eq 0) {
	LogErrorExit "Cannot find any distro matching the tag"
} else {
	# Add individual distros as tags
	foreach ($OSName in $OSList) {
		PerformGQL $OSName
		sleep 300
	}
}
