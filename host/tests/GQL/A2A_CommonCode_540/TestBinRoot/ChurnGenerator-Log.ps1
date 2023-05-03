Param
(
	[parameter(Mandatory=$true)]
	[String]$BlobUrl,
		
    [parameter(Mandatory=$true)]
	[String]$StorageKey,
	
	[Parameter(Mandatory=$false)]
    [String]$DataFolder="ChurnData",
	
	[Parameter(Mandatory=$false)]
    $Validate = $false,
	
	[parameter(Mandatory=$false)]
	[String]$ChurnSize="300",

	[Parameter(Mandatory=$false)]
    [String]$DataSize="1"
)

# Global Variables
$global:WorkingDir = $Env:SystemDrive + "\TestBinRoot"
$global:DIResultLogPath = $WorkingDir + "\DITestResults"
$global:ChurnVolumes = @()

#
# Create log directory if not exist
#
Function CreateLogDir()
{
	# Create the directory
    if(!(Test-Path $DIResultLogPath))
	{ 
		New-Item $DIResultLogPath -Type Dir >> $null
		#Write-Host "Created the directory $DIResultLogPath ..."
	}
	
	if ( !$? )
    {
        Write-Error "Error creating working directory $DIResultLogPath"
        exit 1
    }
}

#
# Rename DI status file
#
Function RenameFile()
{ 
	param (
		[parameter(Mandatory=$False)] [string]$statusFile
	)
	
	[string]$directory = [System.IO.Path]::GetDirectoryName($statusFile);
	[string]$strippedFileName = [System.IO.Path]::GetFileNameWithoutExtension($statusFile);
	[string]$extension = [System.IO.Path]::GetExtension($statusFile);
	[string]$newFileName = $strippedFileName + "-" + [DateTime]::Now.ToString("yyyyMMdd-HHmmss") + $extension; 
	[string]$newFilePath = [System.IO.Path]::Combine($WorkingDir, $newFileName);

	Move-Item -LiteralPath $statusFile -Destination $newFilePath >> $null
	if ($? -eq "True")
	{
		Logger "INFO : Renamed $statusFile to $newFilePath successfully"
	}
	else
	{
		Logger "ERROR : Failed to rename $statusFile to $newFilePath"	
		exit 1
	}
}

#
# Log messages to text file
#
Function Logger ([string]$message)
{
	$logfile = $DIResultLogPath + "\ChurnGenerator.log"
	if(!(Test-Path $logfile)) { New-Item $logfile -Type File }
        
	Write-output [$([DateTime]::Now)]$message | out-file $logfile -Append
}

#
# Get the Volume Info from the Server
#
Function GetChurnVolumes()
{
    try
    {
        # Initialize the Volume Sting and Counts
        $volString = ""
        $server = $env:COMPUTERNAME
        
        Logger "INFO : Started getting the Volume and Churn Details for Server $server"
       
        # Check the OS Type of the Server
        $osName = ((Get-WmiObject Win32_OperatingSystem).Name).split(' ')[3]
        if ($osName -eq "2008")
        {
	        # Get the Volume Info
	        $volInfo = Get-WmiObject -Class Win32_LogicalDisk

	        # Loop over all the Volumes
	        foreach ($vol in $volInfo)
	        {
                # Check if this Volume is a Fixed Volume and it has a Drive Letter and it is not the System Drive
                if (($vol.DriveType -eq "3") -and ($vol.DeviceID -ne '') -and `
                ($vol.VolumeName -ne "Temporary Storage") -and ($vol.DeviceID -ne "C:"))
                {
                    # Update the Volume String
                    if ($volString -eq "")
                    {
                        $volString = $vol.DeviceID.split(':')[0]
                    }
                    else
                    {
                        $volString = $volString + "," + $vol.DeviceID.split(':')[0]
                    }

                    # Calculate the free size
                    $freeSize = $vol.FreeSpace/(1024*1024)
                    if($freeSize -le $ChurnSize)
                    {
                        Logger "ERROR : Vol : $vol freespace : $freeSize is less than ChurnSize : $ChurnSize"
                        exit 1
                    }
                }
	        }
        }
        else
        {
	        # OS Type is not 2008
            # Get Volume Details
            $volInfo = Get-Volume
        
            # Loop over all the Volumes
            foreach ($vol in $volInfo)
            {
                # Check if this Volume is a Fixed Volume and it has a Drive Letter and it is not the System Drive
                if (($vol.FileSystemLabel -ne "System Reserved") -and ($vol.DriveType -eq "Fixed") -and 
                    ($vol.DriveLetter.GetHashCode() -ne 0) -and ($vol.FileSystemLabel -ne 'Temporary Storage') -and ($vol.DriveLetter -ne "C"))
                {
                    
                    # Update the Volume String
                    if ($volString -eq "")
                    {
                        $volString = $vol.DriveLetter
                    }
                    else
                    {
                        $volString = $volString + "," + $vol.DriveLetter
                    }

                    # Calculate the free size
                    $freeSize = $vol.SizeRemaining/(1024*1024)
                    if($freeSize -le $ChurnSize)
                    {
                        Logger "ERROR : Vol : $vol freespace : $freeSize is less than ChurnSize : $ChurnSize"
						Write-Output "Failure"
                        exit 1
                    }

                }
            }
        }

        Logger "INFO : Completed getting the Volume details : $volString for Server $server"

        if (([string]$volString).Contains(' '))
        {
            $churnVolumes = ([string]$volString).Split(' ')[1].Split(';')[0]
        }
        else
        {
            $churnVolumes = ([string]$volString).Split(';')[0]
        }

        Logger "INFO : Retrieved Volume Labels [$churnVolumes] on Server $server"

        $global:ChurnVolumes = ([string]$ChurnVolumes).Split(',')
    }
    catch
    {
        Logger "ERROR : GetChurnVolumes : Caught Exception : $_.Exception.Message"
		$line = $_.InvocationInfo.ScriptLineNumber
		Logger "ERROR : Caught Exception : $line"
        exit 1
    }
}

#
# GenerateChurn
#
Function GenerateChurn()
{
    try
    {      
		$ChurnBinary = $WorkingDir + "\GenerateTestTree.exe"
		foreach ($vol in $global:ChurnVolumes)
        {
            # Multi Threading logic here...Create the Mutext
            Logger "INFO : Attempting to grap mutext"
            $mtx = New-Object System.Threading.Mutex($false, "DI-Mutex")

            # Wait on the Mutex - blocking call
            $mtx.WaitOne() >> $null
            Logger "INFO : Received mutext"
			
			$churndir = $vol + ":\" + $DataFolder
			$seed = $dataSize + "-" + $vol
				
			# Check whether dir exists and delete it
			if ( $(Test-Path -Path "$churndir" -PathType Container) )
			{
				Logger "INFO : Directory $churndir already exist. Deleting it ..."
				Remove-Item $churndir -Recurse -Force >> $null
			}

			# Create the directory
			Logger "INFO : Creating the directory $churndir ..."
			New-Item -Path "$churndir" -ItemType Directory -Force >> $null
			if ( !$? )
			{
				Write-Error "Error creating churn directory $churndir"
				exit 1
			}
				
			$size = 0
			while($size -le $ChurnSize)
			{
				$datetime = [DateTime]::Now.ToString("yyyyMMddHHmmss")
				$srcdir = $churndir + "\" + $dataSize + "_" + $datetime 
				
				# Generate churn
				$Params = "0 $dataSize $seed $srcdir"
				$Prms = $Params.Split(" ")
				
				#Logger "INFO : Initiating $dataSize churn on $vol with params : $Params"
				$output = & "$ChurnBinary" $Prms
				
				if (!$?)
				{
					Logger "ERROR : output : $output"		        
					Logger "ERROR : Data generation failed on $vol"
					exit 1
				}
				
				#Logger "INFO : Successfully generated data on $vol : Output : $output"
				
				<#
				$Params1 = "1 $dataSize $seed $srcdir"
				$Prms1 = $Params1.Split(" ")
				Logger "INFO : Validating churn on $vol with Params : $Params1"
				$output1 = & "$ChurnBinary" $Prms1
				
				if (!$?)
				{
					Logger "ERROR : output : $output1"		        
					Logger "ERROR : Data validation failed on $vol"
					exit 1
				}
				
				Logger "INFO : Successfully validated data on $vol : Output : $output1"
				#>
				
				$size++
				Start-Sleep -seconds 1
			}
			
            # Release the Mutex
            [void]$mtx.ReleaseMutex()
            $mtx.Dispose()
            Logger "INFO : Released mutext"

            DIChecker -churnFolder $churndir
			
			Logger "INFO : Generated Checksums for $vol"
        }
    }
    catch
    {
        Logger "ERROR : GenerateChurn : Caught Exception : $_.Exception.Message"
		$line = $_.InvocationInfo.ScriptLineNumber
		Logger "ERROR : Caught Exception : $line"
        exit 1
    }
}

#
# Update churn details in config file
#
Function UpdateConfig()
{
    try
    {
        $ConfigFile = $WorkingDir + "\ChurnConfig.xml"
		if (Test-Path $ConfigFile) { RenameFile $ConfigFile }
		
		$volumes = $global:ChurnVolumes -join ";"
		$ChurnSize = $ChurnSize.Trim()
		# Create The Document
		$XmlWriter = New-Object System.XMl.XmlTextWriter($ConfigFile,$Null)

		# Set The Formatting
		$xmlWriter.Formatting = "Indented"
		$xmlWriter.Indentation = "4"
		 
		# Write the XML Decleration
		$xmlWriter.WriteStartDocument()
		 
		# Write Root Element
		$xmlWriter.WriteStartElement("ChurnDriver")
		 
		# Write the Document
		$xmlWriter.WriteElementString("ChurnVolumes", $volumes)
		$xmlWriter.WriteElementString("ChurnSize", $ChurnSize)
		$xmlWriter.WriteElementString("DataFolder", $DataFolder)
		 
		# Write Close Tag for Root Element
		$xmlWriter.WriteEndElement >> $null
		 
		# End the XML Document
		$xmlWriter.WriteEndDocument()

		# Finish The Document
		$xmlWriter.Finalize
		$xmlWriter.Flush >> $null
		$xmlWriter.Close()
    }
    catch
    {
        Logger "ERROR : UpdateConfig : Caught Exception : $_.Exception.Message"
		$line = $_.InvocationInfo.ScriptLineNumber
		Logger "ERROR : Caught Exception : $line"
        exit 1
    }
}


#
# Function to Get the Hash Value of a File
#
Function GetFileHash() 
{
    param (
		[parameter(Mandatory=$True)] [string]$FileName,
		[parameter(Mandatory=$True)] [string]$HashName
	)

	# Hash Algorithm
    $hash = [Security.Cryptography.HashAlgorithm]::Create($HashName)

    # Compute the Hash Value of the File Stream
    $stream = ([IO.StreamReader]$FileName).BaseStream
    $val = -join ($hash.ComputeHash($stream) | ForEach { "{0:x2}" -f $_ })
    $stream.Close()
	
    # Return the Hash Value
    return $val 
}

#
# For data generation, Calculates md5 hash for all the data files and writes to .md5 files
# For data validation, calculates md5 hash of the data files and compares it with the .md5 file content
# and writes the result to volume level test files.
#
Function DIChecker($churnFolder)
{ 
    # Remote Log and Status Files
    $vol = $churnFolder.Split(":")[0]
    $RemoteLogFile = $DIResultLogPath + "\DataIntegrity-Vol"  + $vol + ".log"
    $RemoteStatusFile = $DIResultLogPath + "\DataIntegrity-Vol" + $vol + "-Status.log"
    Write-Output "INFO : RemoteStatusFile : $RemoteStatusFile" | Out-File $RemoteLogFile -Append
    
    # Flag to denote overall Success / Failure Status
    $failureStatus = $false

    # Check if the Volume was mounted successfully
    if ((Get-PSDrive $vol).Name -eq $vol)
    {
        # Volume was mounted successfully
        Write-Output "INFO : Volume $vol was mounted successfully" | Out-File $RemoteLogFile -Append

        # Check if the Data Folder Path exists
        if (Test-Path $churnFolder)
        {
            # Data Folder for this Volume exists
            Write-Output "INFO : Data Folder $churnFolder exists" | Out-File $RemoteLogFile -Append
            Write-Output "" | Out-File $RemoteLogFile -Append

            # Find all the .DAT files at the given path
            $files = Get-Childitem -Path $churnFolder -Include "*.DAT" -Recurse

            # Loop over all the files in the above list
            foreach ($file in $files)
            {
                # Multi Threading logic here...
                # Create the Mutext
                Write-Output "INFO : Attempting to grab mutext" | Out-File $RemoteLogFile -Append
                $mtx = New-Object System.Threading.Mutex($false, "DI-Mutex")

                # Wait on the Mutex - blocking call
                $mtx.WaitOne() >> $null
                Write-Output "INFO : Received mutext" | Out-File $RemoteLogFile -Append

                # Find the MD5 Hash of the file
                $hash = GetFileHash -FileName $file.fullName -HashName "MD5"
                $fileName = $file.name
                Write-Output "INFO : File=$fileName" | Out-File $RemoteLogFile -Append
                Write-Output "INFO : Current MD5 Hash=$hash" | Out-File $RemoteLogFile -Append

                # Name of the .md5 file where this Hash is stored
                $md5FileName = $file.fullname + '.md5'

                # Check if the Opertaion is Generation or Validation
                if ($Validate -eq $false)
                {
                    # Generate the .md5 file
                    # Remove the existing .md5 file if any
                    if (Test-Path $md5FileName)
                    {
                        Remove-Item -Path $md5FileName -Force -Confirm:$false
                        Write-Output "INFO : Existing .md5 file has been deleted" | Out-File $RemoteLogFile -Append
                    }
					
                    # Save the MD5 Hash in a new .md5 file
                    Add-Content -Path $md5FileName -Value $hash
                    Write-Output "INFO : New .md5 file has been created" | Out-File $RemoteLogFile -Append
                }
                else
                {
                    if (Test-Path $RemoteStatusFile) { RenameFile $RemoteStatusFile }
					
					# Validate the .md5 file
                    # First check if there is any existing .md5 file
                    if (!(Test-Path $md5FileName))
                    {
                        # .md5 file has not been found - Validaton cannot proceed
                        Write-Output "ERROR : .md5 file is missing - Validation Failed" | Out-File $RemoteLogFile -Append
                        $failureStatus = $true
                    }
                    else
                    {
                        # Get the Content of the existing .md5 file
                        $oldHash = Get-Content -Path $md5FileName
                        Write-Output "INFO : Previous MD5 Hash=$oldHash" | Out-File $RemoteLogFile -Append

                        # Check if the Hash values match
                        if ($hash -eq $oldHash)
                        {
                            # Hash Values have matched - Validation passed
                            Write-Output "INFO : MD5 Hash Values are in sync - Validation passed" | Out-File $RemoteLogFile -Append
                        }
                        else
                        {
                            # Hash Values have not matched - Validation failed
                            Write-Output "ERROR :  MD5 Hash Values are not in sync - Validation failed" | Out-File $RemoteLogFile -Append
                            $failureStatus = $true
                        }
                    }
                }

                # Release the Mutex
                [void]$mtx.ReleaseMutex()
                $mtx.Dispose()
                Write-Output "INFO : Released mutext" | Out-File $RemoteLogFile -Append            
                Write-Output "" | Out-File $RemoteLogFile -Append
            }
        }
        else
        {
            # Data Folder for this Volume does not exist
            Write-Output "WARN : Churn Folder $churnFolder does not exist" | Out-File $RemoteLogFile -Append
            exit 1
        }
    }
    else
    {
        # Volume was not mounted successfully
        Write-Output "ERROR :  Volume $vol was not mounted successfully" | Out-File $RemoteLogFile -Append
        $failureStatus = $true
    }

    # Now check the overall Status in case the Operation was Validation
    if ($Validate -eq $true)
    {
        New-Item -Itemtype File -Path $RemoteStatusFile -ErrorAction SilentlyContinue >> $null
	    # Operation was Validation
        if ($failureStatus -eq $true)
        {
            # Failure
            Write-Output "FAILED" | Out-File $RemoteStatusFile
        }
        else
        {
            # Success
            Write-Output "PASSED" | Out-File $RemoteStatusFile
        }
    }
}

#
# DataIntegrity validator
#
Function ValidateChurn()
{
	try
	{
		$DIResultFile = $DIResultLogPath + "\DITestResult.txt"
		if(Test-Path $DIResultFile){ Remove-Item $DIResultFile -Force -Confirm:$false >> $null}

		# Churn Driver Config XML
        $churnConfig = $WorkingDir + "\ChurnConfig.xml"
        [xml]$churnConfigXml = Get-Content $churnConfig
		
		# Parse the Churn Config Details
        $ChurnVolumes = $churnConfigXml.ChurnDriver.ChurnVolumes
        $DataFolder = $churnConfigXml.ChurnDriver.DataFolder
		
        $volumes = $ChurnVolumes.Split(';')
        $volStatus = @{}
        foreach ($vol in $volumes)
        {
	        $churnFolder = $vol + ":\" + $DataFolder
			
			DIChecker -churnFolder $churnFolder
			
			# Get the Volume-specific DI Result File
            $churnFile = $DIResultLogPath + "\DataIntegrity-Vol"  + $vol + "-Status.log"
	        Logger "INFO : Get the volume specific DI Result File : $churnFile"
	
	        if (!(Test-Path $churnFile))
	        {
		        # churnFile file has not been found - Validaton cannot proceed
		        Logger "ERROR :  $churnFile file is missing - Validation Failed"
		        exit 1
	        }
	
	        $res = Get-Content $churnFile  | ? {$_.trim() -ne "" } 
	
	        Logger "INFO : $churnFile content : $res"
	        $volStatus.Add($vol, $res)
        }

        New-Item -Itemtype File -Path $DIResultFile -ErrorAction SilentlyContinue >> $null

        # Operation was Validation
        if ($volStatus.ContainsValue("FAILED"))
        {
	        # Failure
	        Write-Output "FAILED" | Out-File $DIResultFile
        }
        else
        {
	        # Success
	        Write-Output "PASSED" | Out-File $DIResultFile
        }
	}
	catch
    {
        Logger "ERROR : ValidateChurn : Caught Exception : $_.Exception.Message"
		$line = $_.InvocationInfo.ScriptLineNumber
		Logger "ERROR : Caught Exception : $line"
        exit 1
    }
}

#
# Check if log file exits and is valid
#
Function VerifyFile()
{
    param (
		[parameter(Mandatory=$True)] [string]$file
	)

    # check if log file if exits
    if (Test-Path $file)
    {
        if((Get-Item $file).length -gt 0kb)
        {
            Logger "INFO : $file is valid"
        }
        else
        {
            Logger "ERROR : $file size is 0"
            exit 1
        }
    }
    else
    {
        Logger "ERROR : $file file doesn't exist"
        exit 1
    }
}

#
# Upload DI Test Status to Blob Storage
#
Function UploadDIStatus()
{
	try
    {
        $resultFile = "DITestResult.txt"
		Logger "INFO : BlobUrl : $BlobUrl, StorageKey : $StorageKey, resultFile : $resultFile"
		
		$logFile = $DIResultLogPath + '\DITestResult.txt'
        VerifyFile -file $logFile
	    
	    $Command = "C:\Program Files (x86)\Microsoft SDKs\Azure\AzCopy\AzCopy.exe"
	    $Params = "/Y /Source:$DIResultLogPath /Dest:$BlobUrl /DestKey:$StorageKey /Pattern:$resultFile"
	    $Prms = $Params.Split(" ")
	    $output = & "$Command" $Prms
        
	    if (!$?)
		{
			Logger "ERROR : Failed to upload DI Test Result to storage container"
			Logger "ERROR : Output : $output"
			exit 1
		}
		
		Logger "INFO : Successfully uploaded DI Test result to storage container"
		Logger "INFO : Output : $output"
	}
    catch
    {
        Logger "ERROR : UploadDIStatus : Caught Exception : $_.Exception.Message"
		$line = $_.InvocationInfo.ScriptLineNumber
		Logger "ERROR : Caught Exception : $line"
        exit 1
    }	
}

#
# Main
#
Function Main()
{
	if($Validate -eq $false)
	{
		CreateLogDir
		GetChurnVolumes
		GenerateChurn
		UpdateConfig
	}
	else
	{
		ValidateChurn
		UploadDIStatus
	}
}

Main
Write-Output "Success"