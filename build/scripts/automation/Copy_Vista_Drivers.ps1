# Script to copy vista drivers from staging server to build server.
$BranchName=$args[0]
$BuildVersion=$args[1]
$Config=$args[2]
$BuildMachine="InMStagingSvr"
$Date = Get-Date -Format dd_MMM_yyyy
$Log_Name = "Copy_vista_drivers_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$Log_File = "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date\$Log_Name"

# Remove existing x86 and x64 indskflt.sys and indskflt.pdb files
Remove-Item -Recurse "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x86\Vista$Config"
Remove-Item -Recurse "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x64\Vista$Config"
mkdir "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x86\Vista$Config"
mkdir "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x64\Vista$Config"

# Copy x86 and x64 indskflt.sys and indskflt.pdb files from InMstagingsvr.
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$BuildVersion\HOST\$Date\UnifiedAgent_Builds\$Config\InDskFlt\VistaRelease\x86\indskflt.sys" "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x86\Vista$Config" >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$BuildVersion\HOST\$Date\UnifiedAgent_Builds\$Config\InDskFlt\VistaRelease\x86\indskflt.pdb" "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x86\Vista$Config" >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$BuildVersion\HOST\$Date\UnifiedAgent_Builds\$Config\InDskFlt\VistaRelease\x64\indskflt.sys" "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x64\Vista$Config" >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$BuildVersion\HOST\$Date\UnifiedAgent_Builds\$Config\InDskFlt\VistaRelease\x64\indskflt.pdb" "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\drivers\InVolFlt\windows\DiskFlt\x64\Vista$Config" >> $Log_File
