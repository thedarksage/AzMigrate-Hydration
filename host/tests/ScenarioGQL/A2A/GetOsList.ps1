# TagName/TAG
param (
    [string] $TagName = $(if ($Args -eq $null) { $null } else { $Args[0] })
)

. $PSScriptRoot\OS.ps1

if ($TagName -eq "") {
    GenTags
	Log "$($tags | Out-String)"
    Log "USAGE: <Script> <TAG NAME>"
    LogErrorExit "Enter valid TAG NAME"
}

$dlist = GetOsList $TagName
if ($dlist.count -eq 0) {
	LogErrorExit "Cannot find any distro matching the tag"
} else {
   	# Add individual distros as tags
	foreach ($OSName in $dlist) {
        $OSName
	}

}
