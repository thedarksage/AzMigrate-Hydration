#Declaring the date and time variables.
$Date = Get-Date -Format dd_MMM_yyyy
$Time = Get-Date -Format hh_mm_tt

$Config=$args[0]
$Product=$args[1]
$SourcePath=$args[2]
$Branch=$args[3]
$Signing=$args[4]
$FolderPath = "I:\Signing\"+$Branch
$Log_Name = "Copy_Singed_Installers_To_Build_Path"+"_"+$Config+"_"+$Date+"_"+$Time+".txt"
$Log_File = $FolderPath+"\"+$Date+"\"+$Product+"_INSTALLER"+"\"+$Log_Name

Write-Host $Config
Write-Host $Product
Write-Host $SourcePath
Write-Host $Branch
Write-Host $Signing

if ($Product -eq "UCX")
{
	# Define copy back path for installer
	$UCXCopyBackPath = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"

	# Copy installer to submission path
	Copy-Item $UCXCopyBackPath\ucx_server_setup.exe "$SourcePath\server\windows\$Config\ucx_server_setup.exe" -passthru >> $Log_File
}

if ($Product -eq "CX_TP")
{
	# Define copy back path for installer
	$CXTPCopyBackPath = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"
	
	# Copy installer to submission path
	Copy-Item $CXTPCopyBackPath\cx_thirdparty_setup.exe "$SourcePath\server\windows\$Config\cx_thirdparty_setup.exe" -passthru >> $Log_File
}

if ($Product -eq "ASRUA")
{
	# Define copy back path for installer
	$ASRUACopyBackPath = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"

	# Copy installer to submission path
	Copy-Item $ASRUACopyBackPath\MicrosoftAzureSiteRecoveryUnifiedAgent.exe "$SourcePath\host\ASRSetup\PackagerUnifiedAgent\bin\$Config\MicrosoftAzureSiteRecoveryUnifiedAgent.exe" -passthru >> $Log_File
}

if ($Product -eq "ASRSETUP")
{
	# Define copy back path for installer
	$ASRSETUPCopyBackPath = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"

	# Copy installer to submission path
	Copy-Item $ASRSETUPCopyBackPath\MicrosoftAzureSiteRecoveryUnifiedSetup.exe "$SourcePath\host\ASRSetup\Packager\bin\$Config\MicrosoftAzureSiteRecoveryUnifiedSetup.exe" -passthru >> $Log_File
}

if ($Product -eq "PSMSI")
{
	# Define copy back path for installer
	$PSMSICopyBackPath = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"

	# Copy installer to submission path
	Copy-Item $PSMSICopyBackPath\ProcessServer.msi "$SourcePath\server\windows\ProcessServerMSI\x64\bin\$Config\ProcessServer.msi" -passthru >> $Log_File
}