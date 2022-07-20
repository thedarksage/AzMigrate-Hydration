#Powershell script to copy DRA related binaries to local path.

#Declaring the variables for directory structure.
$BranchName=$args[0]
$BuildMachine="InMStagingSvr"
$Date = Get-Date -Format dd_MMM_yyyy
$Date1 = Get-Date -Format ddMMMyyyy

#Declaring the config variable and reading the value from perl module.
$Config=$args[1]

#Declaring the variables for log name.
$DRALog_Name = "Copy_DRA_Binaries_"+$(Get-Date -Format dd_MMM_yyyy_hh_mm_tt)+".txt"
$DRALog_File = "H:\BUILDS\Daily_Builds\$BranchName\HOST\$Date\$DRALog_Name"

# Copying DRA binaries to .\host\setup\DRA\ path
$yDay = (Get-Date).AddDays(-1).ToString('yyMMdd')
$curMonth = (Get-Date).ToString('yyMM')
$prvMonth = (Get-Date).AddMonths(-1).ToString('yyMM')
$targetDirectory = "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA"

function sendMail {     
    $Attachment = "$DRALog_File"
    $Subject = "DRA binaries from Branch - $BranchName $Config build "	
    $Body = Get-Content -Path "$DRALog_File" | Select-String -Pattern "^(?!.*.exe does not exist.)" | Select-String -Pattern "DRA binaries Path"	
    Send-MailMessage -From mabldadm@microsoft.com -To inmiet@microsoft.com, manish.jain@microsoft.com -SmtpServer cloudmail.microsoft.com -Subject $Subject -Body $Body -Attachments $Attachment
}

function CopyDRA {	
    $BUILDAVAILABLE = "0"
    $SharePath = "\\inmstagingsvr\OneOffRequests\dra_develop_latest\retail-amd64"
	$dllArray = @(
	"$SharePath\ASRAdapterFiles\SetupFramework.dll",
	"$SharePath\ASRAdapterFiles\DRResources.dll",
	"$SharePath\ASRAdapterFiles\IntegrityCheck.dll",
	"$SharePath\ASRAdapterFiles\Newtonsoft.Json.dll",
	"$SharePath\ASRAdapterFiles\EndpointsConfig.xml",
	"$SharePath\ASRAdapterFiles\AccessControl2.S2S.dll",
	"$SharePath\ASRAdapterFiles\AsyncInterface.dll",
	"$SharePath\ASRAdapterFiles\CatalogCommon.dll",
	"$SharePath\ASRAdapterFiles\CloudCommonInterface.dll",
	"$SharePath\ASRAdapterFiles\CloudSharedInfra.dll",
	"$SharePath\ASRAdapterFiles\ErrorCodeUtils.dll",
	"$SharePath\ASRAdapterFiles\IdMgmtApiClientLib.dll",
	"$SharePath\ASRAdapterFiles\IdMgmtInterface.dll",
	"$SharePath\InMageFabricExtension\Microsoft.IdentityModel.dll",
	"$SharePath\ASRAdapterFiles\SrsRestApiClientLib.dll",
	"$SharePath\ASRAdapterFiles\TelemetryInterface.dll",
	"$SharePath\ASRAdapterFiles\Microsoft.Identity.Client.dll",
	"$SharePath\ASRAdapterFiles\Microsoft.ApplicationInsights.dll",
	"$SharePath\ASRAdapterFiles\Polly.dll",
	"$SharePath\AzureSiteRecoveryConfigurationManager.msi"
	)
	            
    $ReleaseFileExists = Test-Path $dllArray[0]
    if ($ReleaseFileExists -eq $True) {
        echo "Available Release DRA binaries path: $($dllArray[0])" >> $DRALog_File
        echo "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------">> $DRALog_File
        New-Item $targetDirectory -Type Directory	
        
        # Copying DRA related files/exe's/dll's to host\setup\DRA path.
        foreach ($file in $dllArray) {
            Copy-Item -Path "$file" I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA -passthru >> $DRALog_File
        }
        
        echo " " >> $DRALog_File
        $BUILDAVAILABLE = "1"
        Break
    } else {
        echo "DRA binaries Path: $($dllArray[0]) does not exist." >> $DRALog_File
        $BUILDAVAILABLE = "0"
    }

    echo " " >> $DRALog_File

    if ($BUILDAVAILABLE -eq "0" ) { 
        echo "Latest release DRA binaries does not exist." >> $DRALog_File
    }
}

function DownloadDRABinaries {
    echo "Calling Download_DRA_Binaries.ps1 script." >> $DRALog_File
    .\Download_DRA_Binaries.ps1 -targetPath "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host\setup\DRA" -branch "$BranchName"
    if ($LASTEXITCODE -eq 1) {
        echo "Download_DRA_Binaries.ps1 script execution has failed." >> $DRALog_File
        exit $LASTEXITCODE
    }
}

# Temporary work around until code is added to download NuGet packages automatically.
#Copy-Item -Recurse I:\Share\$BranchName\packages I:\SRC\$BranchName\InMage-Azure-SiteRecovery\host  -passthru >> $DRALog_File
#Copy-Item -Recurse I:\Share\$BranchName\packages I:\SRC\$BranchName\InMage-Azure-SiteRecovery\server  -passthru >> $DRALog_File
$OSDetailsPath = "I:\SRC\$BranchName\InMage-Azure-SiteRecovery\server\windows\Unix_LE_OS_details.sh"
Copy-Item I:\SRC\$BranchName\InMage-Azure-SiteRecovery\build\scripts\general\Unix_LE_OS_details.sh $OSDetailsPath -passthru >> $DRALog_File
#Convert to Unix Line endings
[IO.File]::WriteAllText($OSDetailsPath, $([IO.File]::ReadAllText($OSDetailsPath) -replace "`r`n", "`n"))

#Disabling the method call CopyDRA. As latest DRA script is integrated in Download_DRA_Binaries.ps1
#CopyDRA
DownloadDRABinaries

# Sending mail to InMage Install Experience Team
sendMail
