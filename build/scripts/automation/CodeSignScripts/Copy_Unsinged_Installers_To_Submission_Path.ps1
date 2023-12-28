#Declaring the date and time variables.
$Date = Get-Date -Format dd_MMM_yyyy
$Time = Get-Date -Format hh_mm_tt

$Config=$args[0]
$Product=$args[1]
$SourcePath=$args[2]
$Branch=$args[3]
$Signing=$args[4]
$FolderPath = "I:\Signing\"+$Branch
$Log_Name = "Copy_Unsinged_Installers_to_Submission_Path"+"_"+$Config+"_"+$Date+"_"+$Time+".txt"
$Log_File = $FolderPath+"\"+$Date+"\"+$Product+"_INSTALLER"+"\"+$Log_Name

Write-Host $Config
Write-Host $Product
Write-Host $SourcePath
Write-Host $Branch
Write-Host $Signing

if ($Product -eq "UCX")
{
	# Remove existing contents and create directories afresh to keep unsigned installer
	$UCXTargetPathUnsigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Unsigned\$Config"
	Remove-Item $UCXTargetPathUnsigned\* -recurse -Force
	new-Item -Path $UCXTargetPathUnsigned -ItemType directory

	# Remove existing contents and create directories afresh to keep signed installer
	$UCXTargetPathSigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"
	Remove-Item $UCXTargetPathSigned\* -recurse -Force
	new-Item -Path $UCXTargetPathSigned -ItemType directory

	# Copy installer to submission path
	Copy-Item "$SourcePath\server\windows\$Config\ucx_server_setup.exe" $UCXTargetPathUnsigned -passthru >> $Log_File
}

if ($Product -eq "CX_TP")
{
	# Remove existing contents and create directories afresh to keep unsigned installer
	$CXTPTargetPathUnSigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Unsigned\$Config"
	Remove-Item $CXTPTargetPathUnSigned\* -recurse -Force
	new-Item -Path $CXTPTargetPathUnSigned -ItemType directory

	# Remove existing contents and create directories afresh to keep signed installer
	$CXTPTargetPathSigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"
	Remove-Item $CXTPTargetPathSigned\* -recurse -Force
	new-Item -Path $CXTPTargetPathSigned -ItemType directory
	
	# Copy installer to submission path
	Copy-Item "$SourcePath\server\windows\$Config\cx_thirdparty_setup.exe" $CXTPTargetPathUnSigned -passthru >> $Log_File
}

if ($Product -eq "ASRUA")
{
	# Remove existing contents and create directories afresh to keep unsigned installer
	$ASRUATargetPathUnsigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Unsigned\$Config"
	Remove-Item $ASRUATargetPathUnsigned\* -recurse -Force
	new-Item -Path $ASRUATargetPathUnsigned -ItemType directory

	# Remove existing contents and create directories afresh to keep signed installer
	$ASRUATargetPathSigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"
	Remove-Item $ASRUATargetPathSigned\* -recurse -Force
	new-Item -Path $ASRUATargetPathSigned -ItemType directory

	# Copy installer to submission path		
	Copy-Item "$SourcePath\host\ASRSetup\PackagerUnifiedAgent\bin\$Config\MicrosoftAzureSiteRecoveryUnifiedAgent.exe" $ASRUATargetPathUnsigned -passthru >> $Log_File        
}

if ($Product -eq "ASRSETUP")
{
	# Remove existing contents and create directories afresh to keep unsigned installer
	$ASRSETUPTargetPathUnsigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Unsigned\$Config"
	Remove-Item $ASRSETUPTargetPathUnsigned\* -recurse -Force
	new-Item -Path $ASRSETUPTargetPathUnsigned -ItemType directory

	# Remove existing contents and create directories afresh to keep signed installer
	$ASRSETUPTargetPathSigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"
	Remove-Item $ASRSETUPTargetPathSigned\* -recurse -Force
	new-Item -Path $ASRSETUPTargetPathSigned -ItemType directory
	
	# Copy installer to submission path		
    Copy-Item "$SourcePath\host\ASRSetup\Packager\bin\$Config\MicrosoftAzureSiteRecoveryUnifiedSetup.exe"  $ASRSETUPTargetPathUnsigned -passthru >> $Log_File        
}

if ($Product -eq "PSMSI")
{
	# Remove existing contents and create directories afresh to keep unsigned installer
	$PSMSITargetPathUnsigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Unsigned\$Config"
	Remove-Item $PSMSITargetPathUnsigned\* -recurse -Force
	new-Item -Path $PSMSITargetPathUnsigned -ItemType directory

	# Remove existing contents and create directories afresh to keep signed installer
	$PSMSITargetPathSigned = "$FolderPath\$Date\$Product"+"_INSTALLER\Signed\$Config"
	Remove-Item $PSMSITargetPathSigned\* -recurse -Force
	new-Item -Path $PSMSITargetPathSigned -ItemType directory
	
	# Copy installer to submission path		
    Copy-Item "$SourcePath\server\windows\ProcessServerMSI\x64\bin\$Config\ProcessServer.msi"  $PSMSITargetPathUnsigned -passthru >> $Log_File        
}