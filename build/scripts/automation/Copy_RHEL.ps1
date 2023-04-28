#Powershell script to copy Linux agents and push clients. It also copies DRA and MARS Agent.

#Declaring the variables for directory structure.
$BranchName=$args[0]
$BuildMachine="InMStagingSvr"
$Version=$args[1]
$Date = Get-Date -Format dd_MMM_yyyy
$Date1 = Get-Date -Format ddMMMyyyy

#Declaring the config variable and reading the value from perl module.
$Config=$args[2]

#Declaring the variables for log name.
$Log_Name = "copy_Linux_agents_pushclients_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$Log_File = "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date\$Log_Name"

Remove-Item  "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents\*.tar.gz"
Remove-Item  "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents\*.tar.gz"
mkdir "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents"
mkdir "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents"

#Copying Linux agents/push clients from InMstagingsvr.
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_RHEL6-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_RHEL7-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_RHEL8-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_OL7-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_OL8-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_SLES12-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_SLES15-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_UBUNTU-16.04-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_UBUNTU-18.04-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_UBUNTU-20.04-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_DEBIAN10-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\UnifiedAgent_Builds\$Config\Microsoft-ASR_UA_*_DEBIAN11-*_$Date1_$Config.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File

Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\RHEL5-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\RHEL6-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\RHEL7-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\RHEL8-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\OL6-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\OL7-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\OL8-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\SLES11-SP3-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\SLES11-SP4-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\SLES12-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\SLES15-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\UBUNTU-14.04-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\UBUNTU-16.04-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\UBUNTU-18.04-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\UBUNTU-20.04-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\DEBIAN7-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\DEBIAN8-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\DEBIAN9-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\DEBIAN10-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File
Copy-Item "\\$BuildMachine\DailyBuilds\Daily_Builds\$Version\HOST\$Date\PushInstallClients\DEBIAN11-*_pushinstallclient.tar.gz"  I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\Linux_Agents -passthru >> $Log_File

#Declaring the variables for log name.
$DRALog_Name = "DRABuildDetails_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$DRALog_File = "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date\$DRALog_Name"

# Copying ASR DRA build to .\host\setup\DRA\ path
$yDay = (Get-Date).AddDays(-1).ToString('yyMMdd')
$curMonth = (Get-Date).ToString('yyMM')
$prvMonth = (Get-Date).AddMonths(-1).ToString('yyMM')
$targetDirectory = "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA"

function sendMail {     
    $Attachment = "$DRALog_File"
    $Subject = "DRA from Branch - $BranchName $Config build "	
    $Body = Get-Content -Path "$DRALog_File" | Select-String -Pattern "^(?!.*.exe does not exist.)" | Select-String -Pattern "DRA Build Path"	
    Send-MailMessage -From mabldadm@microsoft.com -To inmiet@microsoft.com, manish.jain@microsoft.com -SmtpServer cloudmail.microsoft.com -Subject $Subject -Body $Body -Attachments $Attachment
}


function CopyDRA {
    $BUILDAVAILABLE = "0"
	$SharePath = "\\inmstagingsvr\OneOffRequests\dra_develop_latest\retail-amd64"
	$DRAExe = "$SharePath\AzureSiteRecoveryProvider.exe"
	$BinArray = @(				
		"$SharePath\ASRAdapterFiles\InmageDiscovery.exe",
		"$SharePath\ASRAdapterFiles\AutoMapper.dll",
		"$SharePath\ASRAdapterFiles\STSService.dll",
		"$SharePath\ASRAdapterFiles\VMware.Interfaces.dll",
		"$SharePath\ASRAdapterFiles\en\VMware.Interfaces.resources.dll",
		"$SharePath\ASRAdapterFiles\Vim25Service.dll",
		"$SharePath\ASRAdapterFiles\VMware.VSphere.Management.dll"
	)
			
     $ReleaseFileExists = Test-Path $DRAExe
     if ($ReleaseFileExists -eq $True) {
         $PKGDRAPATH = "Available Release DRA Build Path: $DRAExe "
         echo "Available Release DRA Build Path: $DRAExe " >> $DRALog_File
         echo "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------">> $DRALog_File
         New-Item $targetDirectory -Type Directory
		
		# Copy DRA to host\setup\DRA.
		Copy-Item -Path $DRAExe I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA -passthru >> $DRALog_File
         
         # Copy DRA related files/exe's/dll's to host\setup\DRA.
         <# Commented out as part of bug to handle only the download of AzureSiteRecoveryProvider.exe from the shared path
         foreach ($file in $BinArray) {
             Copy-Item -Path "$file" I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA -passthru >> $DRALog_File
         }
         #>
         echo " " >> $DRALog_File
         Get-ItemProperty -Path "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA\AzureSiteRecoveryProvider.exe" | Format-list -Property * -Force >> $DRALog_File
         $BUILDAVAILABLE = "1"
         Break
     } else {
         echo "Release DRA Build Path: $DRAExe does not exist." >> $DRALog_File
         $BUILDAVAILABLE = "0"
     }
    
    echo " " >> $DRALog_File

    if ($BUILDAVAILABLE -eq "0" ) { 
        echo "Latest release DRA build does not exist. Using existing DRA Build Path: I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA\AzureSiteRecoveryProvider.exe" >> $DRALog_File
        Get-ItemProperty -Path "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA\AzureSiteRecoveryProvider.exe" | Format-list -Property * -Force >> $DRALog_File
    }
}

# Copying MARS agent to host\setup\MARS path

$MARSLog_Name = "MARSAgentDetails_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$MARSLog_File = "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date\$MARSLog_Name"
$MARStargetFolder = "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\MARS"

# Use "git_mgmt_recoverysvcs_marsasr_develop" branch for DailyBuilds and "git_mgmt_recoverysvcs_marsasr_master" branch for ReleaseBuilds
$MARSBranch = "git_mgmt_recoverysvcs_marsasr_develop"
$MARSBuildsRootPath = "\\reddog\Builds\branches"
$LatestFolder = ((Get-ChildItem $MARSBuildsRootPath\$MARSBranch | Sort-Object -Descending {$_.LastWriteTime}) | select -f 1 | Select Name -ExpandProperty Name)
#$LatestFolder = "9166"
$MARSSourceFile = "$MARSBuildsRootPath\$MARSBranch\$LatestFolder\target\retail\amd64\release\MARSAgentInstaller.exe"

$MARSAgentExists = Test-Path $MARSSourceFile
if ($MARSAgentExists -eq $True) {
    echo "Available MARS Agent Path: $MARSSourceFile " >> $MARSLog_File
    echo "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------">> $MARSLog_File
    Remove-Item $MARStargetFolder -Force -Recurse | Out-Null
    New-Item $MARStargetFolder -Type Directory	
    Copy-Item -Path $MARSSourceFile $MARStargetFolder -passthru >> $MARSLog_File
    echo " " >> $MARSLog_File
    Get-ItemProperty -Path $MARSSourceFile | Format-list -Property * -Force >> $MARSLog_File             
} else {
    echo "Available MARS Agent Path: $MARSSourceFile does not exist." >> $MARSLog_File
}

function MARS_Details_Mail {     
    $Attachment = "$MARSLog_File"
    $Subject = "MARS Agent from $MARSBranch Branch - $BranchName $Config build"	
    $Body = Get-Content -Path "$MARSLog_File" | Select-String -Pattern "Available MARS Agent Path"	
    Send-MailMessage -From mabldadm@microsoft.com -To inmiet@microsoft.com, manish.jain@microsoft.com, rahul.rajwanshi@microsoft.com -SmtpServer cloudmail.microsoft.com -Subject $Subject -Body $Body -Attachments $Attachment
}

# Sending the mail with MARS agent details
# MARS_Details_Mail

#Disabling this method call. As latest DRA script is integrated in Copy_DRA_Binaries
#CopyDRA

# Sending mail to InMage Install Experience Team
#sendMail
