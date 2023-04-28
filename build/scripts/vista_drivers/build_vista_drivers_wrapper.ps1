$Date = Get-Date -Format dd_MMM_yyyy_hh_mm_tt
$BranchName = "release"
$LogName = "GitCheckout_$Date.txt"

new-Item -Path "I:\BuildLogs\$BranchName" -ItemType directory
new-Item -Path "I:\BuildLogs\$BranchName\$LogName" -ItemType file

& C:\Scripts\release\git_checkout.ps1 $BranchName >> "I:\BuildLogs\$BranchName\$LogName" 2>&1
Copy-Item "C:\Scripts\release\vista_driverbld.cmd" -Destination  "I:\Source_Code\release\InMage-Azure-SiteRecovery\build\scripts\automation"
& C:\Scripts\release\vista_driver_build.ps1 $BranchName release \\inmstagingsvr\DailyBuilds\Daily_Builds >> "I:\BuildLogs\$BranchName\vistabld_$Date.log" 2>&1