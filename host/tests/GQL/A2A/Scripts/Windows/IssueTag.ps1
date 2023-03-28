Param
(
    [parameter(Mandatory=$true)]
	[String]$BlobUrl,
		
    [parameter(Mandatory=$true)]
	[String]$StorageKey,
	
	[parameter(Mandatory=$false)]
	[String]$TagType = "app"
)

$global:WorkingDir = $Env:SystemDrive + "\TestBinRoot"
$global:VacpLog = $WorkingDir + "\vacp.log"
$global:TimestampFile = $WorkingDir + "\taginserttimestamp.txt"

# IssueTag
[ScriptBlock] $IssueTag = {
    param($arg)
    $VacpExe = "C:\Program Files (x86)\Microsoft Azure Site Recovery\agent\vacp.exe"
    $Process = (Start-Process -Wait -Passthru -FilePath "$VacpExe" -ArgumentList "$arg" -RedirectStandardOutput $VacpLog )
    return $Process.ExitCode
}

#
# Create log directory if not exist
#
Function CreateLogDir()
{
	# Create the directory
    if(!(Test-Path $WorkingDir))
	{ 
		New-Item $WorkingDir -Type Dir >> $null
		Write-Host "Created the directory $WorkingDir ..."
	}
	
	if ( !$? )
    {
        Write-Error "Error creating working directory $WorkingDir"
        exit 1
    }
}

#
# Log messages to text file
#
Function Logger ([string]$message)
{
	$logfile = $WorkingDir + "\DITest.log"
	Write-output [$([DateTime]::Now)]$message | out-file $logfile -Append
}

#
# Issue Consistency marker 
#
Function IssueTag()
{
    try
    {
        $TagName = "CustomTag"
        Logger "INFO : Issue $TagType tag : $TagName"
        if($TagType -eq "app")
        {
            $args = "-systemlevel -t $TagName"
        }
        elseif($TagType -eq "crash")
        {
            $args = "-cc"
        }

        $log = $WorkingDir + "\vacp.log"
        $ret = (Invoke-Command -ScriptBlock $IssueTag -ArgumentList "$args")
        Logger "INFO : IssueTag : $ret"

        return $ret
    }
    catch
    {
        Logger "ERROR : IssueTag : Caught Exception : $_.Exception.Message"
        exit 1
    }
}

#
# Remove the log file, if exists
#
Function RemoveLog()
{
   param (
		[parameter(Mandatory=$True)] [string]$logFile
	)

    # Remove the existing log file if exits
    if (Test-Path $logFile)
    {
        Remove-Item -Path $logFile -Force -Confirm:$false
        if( !$? )
        {
            Logger "ERROR : Couldn't delete the existing file : $logFile"
            exit 1
        }
        Logger "INFO : Existing $logFile file has been deleted"
    }
    else
    {
        Logger "INFO : $logFile doesn't exist"
    }
}

#
# Check if log file exits and is valid
#
Function VerifyFile()
{
    param (
		[parameter(Mandatory=$True)] [string]$logFile
	)

    # check if log file if exits
    if (Test-Path $logFile)
    {
        if((Get-Item $logFile).length -gt 0kb)
        {
            Logger "INFO : $logFile is valid"
        }
        else
        {
            Logger "ERROR : $logFile size is 0"
            exit 1
        }
    }
    else
    {
        Logger "ERROR : $logFile file doesn't exist"
        exit 1
    }
}

#
# Issue Tag with retry counter
#
Function IssueMarker()
{
    $maxRetries = 3
    $retrycount = 0
    $tagIssued = $false

    try
    {
        # Removing exiting vacp log file
        RemoveLog -logFile $global:VacpLog

        while(-not $tagIssued)
        {
            $status = IssueTag
            if(-not $status)
            {
                Logger "INFO : Successfully issued $TagType tag"
                $tagIssued = $true
            }
            else
            {
                if($retryCount -ge $maxRetries)
                {
                    Logger "ERROR : Failed to issue $TagType tag for $maxRetries times"
                    exit 1
                }
                else
                {
                    Start-Sleep 300
                
                    $timestamp = (Get-Date).Ticks
                    $newlog = $global:VacpLog + $timestamp + "-" + $retryCount + ".log"
                    Rename-Item -Path $global:VacpLog -NewName $newlog -Force
                    $retryCount++
                    Logger "ERROR : Failed to issue $TagType tag, retrying $retryCount"

                }
            }
        }
    }
    catch
    {
        Logger "ERROR : IssueMarker : Caught Exception : $_.Exception.Message"
        exit 1
    }
}

#
# Parse the log file and fetch tag insert timestamp
#
Function GetTagTimeStamp
{
    try
    {
        VerifyFile -logFile $global:VacpLog

        $line = Get-Content $global:VacpLog | Where-Object { $_.Contains("TagInsertTime") }
        if($line)
        {
            $arr = $line -split ':'
            $ts = $arr[1] -replace '\s',''
            Logger "INFO : Inserted tag timestamp : $ts"

            # Removing existing timestamp file
            RemoveLog -logFile $TimestampFile

            New-Item -Itemtype File -Path $TimestampFile -ErrorAction SilentlyContinue >> $null
            if ( !$? )
            {
                Logger "ERROR : error creating file : $TimeStampFile"
                exit 1
            }
	        Write-Output $ts | Out-File $TimestampFile
            Logger "INFO : Updated $TimestampFile with $ts"
        }
        else
        {
            Logger "ERROR : Tag is not inserted"
            exit 1
        }
        
    }
    catch
    {
        Logger "ERROR : GetTagTimeStamp : Caught Exception : $_.Exception.Message"
        exit 1
    }
}

#
# Upload Tag timestamp to Blob Storage
#
function UploadFile()
{
	try
    {
        Logger "INFO : BlobUrl : $BlobUrl, StorageKey : $StorageKey"
	
        VerifyFile -logFile $global:TimestampFile

        $outputFile = Split-Path $TimeStampFile -leaf
	    Logger "INFO : Source: $TimeStampFile, Dest : $BlobUrl"
	    
	    $Command = "C:\Program Files (x86)\Microsoft SDKs\Azure\AzCopy\AzCopy.exe"
	    $Params = "/Y /Source:$WorkingDir /Dest:$BlobUrl /DestKey:$StorageKey /Pattern:$outputFile"
	    $Prms = $Params.Split(" ")
	    $output = & "$Command" $Prms
        
	    if (!$?)
	    {
		    Logger "ERROR : Failed to upload inserted tag timestamp to storage container"
		    Logger "ERROR : Output : $output"
		    exit 1
	    }
	
	    Logger "INFO : Successfully uploaded inserted tag timestamp to storage container"
	    Logger "INFO : Output : $output"
		Write-Output "Success"
	}
    catch
    {
        Logger "ERROR : UploadFiles : Caught Exception : $_.Exception.Message"
        exit 1
    }	
}

#
# Main function
#
Function Main()
{
    CreateLogDir

    IssueMarker

    GetTagTimeStamp

    UploadFile
}

Main