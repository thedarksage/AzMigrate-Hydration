# This script finds the boot disk device id(s), eanbles & disables filtering on each disk device
# return values : 
# 0 - success
# 1 - failure

$path = "Install-Directory\Indskfltservice_output.txt"
function LogScriptOutput([string]$str)
{
  Add-Content -path $path -Value $str -Force
}

try
{
    
    LogScriptOutput("VerifyBootDiskFiltering - Log Path : $path")
    LogScriptOutput("VerifyBootDiskFiltering.ps1 : Enter")
     
    $gdl = & 'Install-Directory\FindDiskID.exe'
    LogScriptOutput($gdl)

    $gdl = & 'Install-Directory\FindDiskID.exe' | Select-String "DEVICE ID"
    if ($? -eq "True") {
       LogScriptOutput("VerifyBootDiskFiltering.ps1 : FindDiskID.exe pattern found $gdl")
    }
    $DevIDs = @()
    foreach ($Line in $gdl)
    {
        $DevIDs += $Line.ToString().Split(":")[1]
    }
	LogScriptOutput("VerifyBootDiskFiltering.ps1 : Extracted Device IDs are $DevIDs")
    foreach ($DevID in $DevIDs)
    {
        LogScriptOutput("VerifyBootDiskFiltering.ps1 : Verifying $DevID")  
        & 'Install-Directory\EnableDisableFiltering.bat' $DevID
        if($? -eq "True")
        {
            LogScriptOutput("VerifyBootDiskFiltering.ps1 : Successfully ran EnableDisableFiltering.bat") 
			LogScriptOutput("VerifyBootDiskFiltering.ps1 : Exit")
            exit 0
        } 
    }
    LogScriptOutput("VerifyBootDiskFiltering.ps1 : Returned failure from EnableDisableFiltering.bat for Device ID $DevID") 
	LogScriptOutput("VerifyBootDiskFiltering.ps1 : Exit")
    exit 1
}
Catch
{
    $string = "Exception caught in VerifyBootDiskFiltering.ps1 : [$_]"
    Add-Content -path $path -Value $string -Force
	LogScriptOutput("VerifyBootDiskFiltering.ps1 : Exit")
    exit 1
}