param(
	# Script params
	[int] $LogDebug = 0
	
)	

# Check InitVars for init of these variables
[string] $global:StatusBaseDir = "$HOME\Desktop\Status"


$Scenarios = @{
    # Key = @(RowID, Columns ... )
    A2A_GQL = @("VM", "OS", "Kernel", "Agent", "CreateConfig", "CreateVM","Reboot", "ER_180", "TFO", "FO_180", "DI_180", "ER_360","FO_360","DI_360", "ER_540", "DI_540", "DisableRep", "CleanRep", "CLEANPREVIOUSRUN")
	V2A_GQL_DEPLOYMENT = @("Branch", "BuildDate", "CS", "PS", "AzurePS", "MARS", "DRPALL", "RebootSVMs", "CleanupRG", "UpdateConfig", "UninstallAgents", "UninstallMT", "RemoveVCenter", "UinstallCS", "CopyBuilds", "CopyCSScripts", "DownloadVault", "InstallCS", "AddAccount", "AddVCenter", "InstallMT")
	V2A_GQL = @("VM", "Branch", "OS", "OSType", "Kernel", "Agent", "UpdateConfig", "AddAccount", "ValidateFabric",  "ER_180", "Reboot", "TFO", "FO_180", "DI_180", "ER_360", "FO_360", "DI_360", "ER_540", "DisableRep", "DelRepPolicy")
    CVT = @("TEST", "BUILD", "PASSED", "FAILED", "UNTESTED")
	BUILD = @("Branch", "Debian7", "Debian8","OL6", "OL7","RHEL5", "RHEL6", "RHEL7", "RHEL8", "SLES11-SP3", "SLES11-SP4", "SLES12", "SLES15", "UBUNTU-14.04", "UBUNTU-16.04", "UBUNTU-18.04", "Windows", "PS.msi", "UnifiedSetup")
}

# Logging

Function Log()
{
	param (
		[string] $LogMessage
	)
	
	$LogDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
	echo "$LogDate $LogMessage" | Out-File -FilePath $LogFilePath -Append
}

Function StatusDirName()
{
	param ([hashtable] $Logger)
	
	return $Logger["Scenario"] + "_" + $Logger["Tag"]
}

Function StatusBlobName()
{
	param ([hashtable] $Logger)
	
	return $StatusDir + "/" + $Logger["Scenario"] + "_" + $Logger["Tag"] + "_status.html"
}

Function StatusLeaseBlobName()
{
	param ([hashtable] $Logger)
	
	return $StatusDir + "/" + $Logger["Scenario"] + "_" + $Logger["Tag"] + ".lease"
}

Function UpdateBlobName()
{
	return "UpdatedBlobs"
}

Function UpdateLeaseBlobName()
{
	return "UpdatedBlobs"
}

Function AppendRowValue()
{
    param (
		[string] $Id,
		[string] $Key,
		[string] $Value
	)

	[string] $RowId = $Id + "_" + "$Key" + "><b> "
    
    $Value = $Value + " </b></td>"
	
	(Get-Content $StatusFilePath) | 
	# Add a row before the end of table
    Foreach-Object {
        if ($_ -match "$RowId") {
			$_ -replace "</b></td>", $Value   
		} else {
			$_
		}
    } | Set-Content $StatusFilePath
}

Function UpdateRowValue()
{
    param (
		[string] $Id,
		[string] $Key,
		[string] $Value
	)

	[string] $RowId = $Id + "_" + "$Key" + "><b> "
	[string] $NewColumn = "<td id=" + $RowId + $Value + " </b></td>"
	
	(Get-Content $StatusFilePath) | 
	# Add a row before the end of table
    Foreach-Object {
        if ($_ -match "$RowId") {
			$NewColumn
		} else {
			$_
		}
    } | Set-Content $StatusFilePath
}

Function DeleteTableRowId()
{
	param ([string] $Id)
	
    # Add a row before the end of table
	(Get-Content $StatusFilePath) | 
    Foreach-Object {
        if (!($_ -match "$Id")) {
            $_ 
        }
    } | Set-Content $StatusFilePath	
}

Function DeleteTableRow()
{
	param ([hashtable] $Logger)
	[string] $Scenario = $Logger["Scenario"]
	[string] $Id = ""
	
	$Id = $Logger["Id"] + "_RowStart"
	
	# If the row is not present, just return
	if (!(Select-String -Path $StatusFilePath -Pattern $Id)) {
		Log "No row corresponding to $Id"
		return
	}
	
	DeleteTableRowId $Id
	
	foreach ($Key in $Scenarios[$Scenario]) {
		$Id = $Logger["Id"] + "_" + $Key
		DeleteTableRowId $Id
	}
	
	$Id = $Logger["Id"] + "_RowEnd"
	DeleteTableRowId $Id
}

Function AddTableRow()
{
	param ([string] $NewTableRow)

    # Add a row before the end of table
	(Get-Content $StatusFilePath) | 
    Foreach-Object {
        if ($_ -match "^</table>") {
			$NewTableRow
		}
		
        $_ 
    } | Set-Content $StatusFilePath
}

Function InitTableRow()
{
	param ([hashtable] $Logger)
	[string] $Scenario = $Logger["Scenario"]
    [string] $TableRow = ""
	[string] $Id = ""
	
	$Id = $Logger["Id"] + "_RowStart"
	$TableRow = "<tr id=" + $Id + ">`r`n"
	
	foreach ($Key in $Scenarios[$Scenario]) {
		$Id = $Logger["Id"] + "_" + $Key
        if ($Key -ne "Logs") {
	        $TableRow += "<td id=" + $Id + "><b> TBD </b></td>`r`n"
        } else {
            $TableRow += "<td id=" + $Id + "><b> </b></td>`r`n"
        }
	}
	
	$Id = $Logger["Id"] + "_RowEnd"
	$TableRow += "<id=" + $Id + " /tr>"

	AddTableRow $TableRow
	UpdateRowValue $Logger["Id"] $Scenarios[$Scenario][0] $Logger["Id"]
}

Function InitHtmlFile()
{
	param ([hashtable] $Logger)
    [string] $Scenario = $Logger["Scenario"]

	$BlobName = StatusBlobName $Logger

	Log "New Table: $BlobName"

	[string] $HtmlBody = ""
	$HtmlBody += "<head><body>`r`n"
	$HtmlBody += "<h3>" + $Logger["Scenario"] + ": " + $Logger["Tag"] + "</h3>`r`n"
	$HtmlBody += "<table border=1>`r`n</table>`r`n"
	$HtmlBody += "<b></b><a href=" + $AzBlobURI + "/" + $BlobName + $SASToken + "> Click Here For Current Status </a>"
    $HtmlBody += "</body></html>"
	$HtmlBody | Out-File -FilePath $StatusFilePath
		
	[string] $TableHdr = "<tr bgcolor=#C0C0C0>"
	foreach ($hdr in $Scenarios[$Scenario]) {
		$TableHdr += "<th>" + $hdr + "</th>"
	}
	$TableHdr += "</tr>"

	AddTableRow $TableHdr
}

#
# Blob REST APIs
#

Function UploadBlob()
{
	param (
		[string] $Name,
		[string] $Path,
        [string] $LeaseGuid
	)

	[string] $URI = $AzBlobURI + "/" + $Name + $SASToken

   	$Hdr = @{  
		"Content-Type" = "text/plain";
		"x-ms-blob-type" = "BlockBlob"
    }

    if ($LeaseGuid) {
        $Hdr["x-ms-lease-id"] = $LeaseGuid
    }

	if ("$Path" -ne "") {
		if (Test-Path -Path $Path) {

    		if ((Get-Item $Path).Extension -eq ".html") {
	    		$Hdr["Content-Type"] = "text/html"
		    }

		} else {

			Log "$Path not found to upload .. creating dummy"
		    echo "$Path not found ... cannot be uploaded" | Out-File -FilePath $Path
			Get-PSCallStack | Out-File -FilePath $Path -Append

        }

		Log "Upload: $Path -> $Name"
		
        Invoke-RestMethod -Method "Put" -InFile "$Path" -Uri $URI -Headers $Hdr

	} else {

		Log "Upload: $Name"
	    Invoke-RestMethod -Method "Put" -Uri $URI -Headers $Hdr 

    }
}

Function DownloadBlob()
{
	param (
		[Parameter(Mandatory=$true)][string] $Name,
		[Parameter(Mandatory=$true)][string] $Path
	)

	[string] $URI = $AzBlobURI + "/" + $Name + $SASToken
	
    Log "Download: $Name -> $Path"

    Invoke-RestMethod -Method "Get" -OutFile "$Path" -Uri $URI -Headers $Hdr
}

Function BlobAcquireLease()
{
	param ([string] $LeaseBlob)
	
	[string] $URI = $AzBlobURI + "/" + $LeaseBlob + $SASToken + "&comp=lease"
	
	$Hdr = @{
        "x-ms-lease-action" = "acquire"; 
        "x-ms-lease-duration" = "60";
		"x-ms-proposed-lease-id" = $LeaseGuid
	}

    Log "Acquire: $LeaseGuid"	

    DO {
        $SC=201
        try {	
            Invoke-RestMethod -Method "Put" -Uri $URI -Headers $Hdr -ErrorAction SilentlyContinue
        } catch {
            $SC = $_.Exception.Response.StatusCode.value__
            sleep 5
        }
    } While ($SC -ne 201)

}

Function BlobReleaseLease()
{
	param ([string] $LeaseBlob)

	[string] $URI = $AzBlobURI + "/" + $LeaseBlob + $SASToken + "&comp=lease"
	
	$Hdr = @{
        "x-ms-lease-action" = "release"; 
		"x-ms-lease-id" = $LeaseGuid
	}

    Log "Release: $LeaseGuid"
	
	Invoke-RestMethod -Method "Put" -Uri $URI -Headers $Hdr
}

#
# UpdatedBlod list APIs


Function UpdateBlobAcquireLease()
{
	$LeaseBlob = UpdateLeaseBlobName
	BlobAcquireLease $LeaseBlob
}

Function UpdateBlobReleaseLease()
{
	$LeaseBlob = UpdateLeaseBlobName
	BlobReleaseLease $LeaseBlob
}

Function UpdateStatusBlobName()
{
    param ([string] $StatusBlobName)

	$BlobName = UpdateBlobName

    UpdateBlobAcquireLease

    DownloadBlob $BlobName $UpdateBlobPath

    # Add blobname before the end of file if not already present
	if (!(Select-String -Path $UpdateBlobPath -Pattern $StatusBlobName)) {
		#echo $StatusBlobName | Out-File -FilePath $UpdateBlobPath -Append
        Log "Appending $StatusBlobName to modified blob list"
        Add-Content -Path $UpdateBlobPath -Value $StatusBlobName
        UploadBlob $BlobName $UpdateBlobPath $LeaseGuid
	} else {
        Log "$StatusBlobName already in modified blob list"
    }

    UpdateBlobReleaseLease
}


#
# Status Blob APIs
#

Function StatusBlobAcquireLease()
{
	param ([hashtable] $Logger)

	$LeaseBlob = StatusLeaseBlobName $Logger

 	BlobAcquireLease $LeaseBlob
}

Function StatusBlobReleaseLease()
{
	param ([hashtable] $Logger)

	$LeaseBlob = StatusLeaseBlobName $Logger
	BlobReleaseLease $LeaseBlob
}

Function DownloadStatusBlob()
{
	param ([hashtable] $Logger)
   
    $BlobName = StatusBlobName $Logger

    StatusBlobAcquireLease $Logger
    DownloadBlob $BlobName $StatusFilePath
}

Function UploadStatusBlob()
{
	param ([hashtable] $Logger)

	$BlobName = StatusBlobName $Logger

	UploadBlob $BlobName $StatusFilePath
	StatusBlobReleaseLease $Logger
	
	# Update the modified blob list
	UpdateStatusBlobName $BlobName
}	

Function CreateStatusLeaseBlob()
{
	param ([hashtable] $Logger)
   
    $LeaseBlob = StatusLeaseBlobName $Logger

	UpdateBlobAcquireLease
	
	try {
		# Download the lease blob. If not present, upload new one
		# StatusFilePath should not exist at this point and is created
		DownloadBlob $LeaseBlob $StatusFilePath
	} catch {
		Log "Creating new lease file"
		UploadBlob $LeaseBlob
	}
	
	UpdateBlobReleaseLease
}

Function CreateStatusBlob()
{
	param ([hashtable] $Logger)
	
    Clear-Content -Path $LogFilePath

	if (Test-Path -Path $StatusFilePath) {
		Remove-Item -Path $StatusFilePath
	}
	
	CreateStatusLeaseBlob $Logger
	
	try { 
        DownloadStatusBlob $Logger 
    } catch {
        Log "Creating new status file"
    }
	
	if (Test-Path -Path $StatusFilePath) {
		Log "InitHtmlFile not required"
	} else {
		InitHtmlFile $Logger
	}
		
	UploadStatusBlob $Logger
}

#
# LogFile APIs
#

Function LogBlobName()
{
    param (
		[string] $Id,
        [string] $Key,
        [string] $Value,
        [string] $LogFile
	)

    $LogFile = $LogFile.Replace(" ","_")

    return $StatusDir + "/" + $Id + "_" + $Key + "_" + $Value + "_" + (Split-Path -Path $LogFile -Leaf)

}

Function UploadLogBlob()
{
    param (
        [string] $Id,
        [string] $Key,
        [string] $Value,
	    [string[]] $LogFiles
	)

    foreach ($LogFile in $LogFiles) {
        $BlobName = LogBlobName $Id $Key $Value $LogFile
		
        UploadBlob $BlobName $LogFile | Out-Null
    }
}

#
# Init
#

Function InitVars()
{
	param ([hashtable] $Logger)
		
	$global:StatusDir = StatusDirName $Logger
	
	$global:StatusFilePath = StatusBlobName $Logger
	$global:StatusFilePath = $global:StatusFilePath.Replace("/","\")
	$global:StatusFilePath = $StatusBaseDir + "\" + $StatusFilePath

	$global:StatusDirPath = Split-Path -Path $StatusFilePath -Parent
	# Create Directory for status file and log
    New-Item -ItemType Directory -Path $StatusDirPath -Force | Out-Null
    
    $global:UpdateBlobPath = UpdateBlobName
	$global:UpdateBlobPath = $StatusDirPath + "\" + $UpdateBlobPath
	
    $global:LogFilePath = $StatusDirPath + "\status_" + $Logger["Id"] + ".log"

	$global:StatusSAccount = "srctestlogs"
	$global:StatusContainer = "testlogs"
	
	### TODO:
	# Git doesn't allow code with auth hash/creds, fetch the secrets from key vault
	$global:SASToken =""

	$global:AzBlobURI = "https://" + $StatusSAccount + ".blob.core.windows.net/"
	$global:AzBlobURI += $StatusContainer
	#$global:AzBlobURI += $Logger["Scenario"] + "_" + $Logger["Tag"]

    $URL = $AzBlobURI + "/" + $Logger["Scenario"] + "_" + $Logger["Tag"] + "/" + $Logger["Scenario"] + "_" + $Logger["Tag"] + "_status.html" + $global:SASToken
	Log "BlobURL: $URL" 
	
	$global:LeaseGuid = [guid]::Newguid()
	Log "Lease Guid = $LeaseGuid"
}



#
# Exported APIs - To be called
#

Function TestStatusLog()
{
    param (
		[Parameter(Mandatory=$true)][hashtable] $Logger,
		[Parameter(Mandatory=$true)][string] $Key,
		[Parameter(Mandatory=$true)][string] $Value,
        [string[]] $LogFiles
	)

    try {
	    #InitVars $Logger

	    if (!($Scenarios[$Logger["Scenario"]] -contains $Key)) {
		    Write-Host "Invalid Key $Key"
		    return
	    }

	    Log "Updating $Key = $Value"
	
        if ($Logfiles.Count -ne 0) {
            $BlobURIs = UploadLogBlob $Logger["Id"] $Key $Value $Logfiles
        }

        $ColValue = $Value

        if ($Key -eq "PASSED" -or $ColValue -match "PASSED") {
		    $ColValue = "<font color=green> " + $ColValue + " </color>"
	    }

        if ($Key -match "FAIL" -or $ColValue -match "FAIL") {
		    $ColValue = "<font color=red> " + $ColValue + " </color>" 
	    }

	    DownloadStatusBlob $Logger
	
        UpdateRowValue $Logger["Id"] $Key $ColValue

        if ($Logfiles.Count -ne 0) {
            foreach ($LogFile in $LogFiles) {
			    Log "Attaching $LogFile"
                $BlobName = LogBlobName $Logger["Id"] $Key $Value $LogFile
                $BlobURI = $AzBlobURI + "/" + $BlobName + $SASToken
                $BlobName = Split-Path -Path $BlobName -Leaf
                $ColValue = "<a href=" + $BlobURI + "> " + $BlobName + " <br/></a>"
                AppendRowValue $Logger["Id"] "Logs" $ColValue
            }
        }

	    UploadStatusBlob $Logger

    } catch {
        Log "TestStatusLog() Failed"
		($Error | ConvertTo-json -Depth 1)
        Get-PSCallStack | Out-File -FilePath $LogFilePath -Append
    }

}

Function TestStatusInit()
{
	param ([hashtable] $Logger)
	$Scenario = $Logger["Scenario"]

    try {
        #InitVars $Logger

        if ($Scenarios[$Scenario] -contains "Logs") {
            Log "Not adding logs"
        } else {
            $Scenarios[$Scenario] += "Logs"
        }

	    CreateStatusBlob $Logger
	    DownloadStatusBlob $Logger
	    DeleteTableRow $Logger
	    InitTableRow $Logger
	    UploadStatusBlob $Logger
    } catch {
        Log "TestStatusInit() Failed"
		($Error | ConvertTo-json -Depth 1)
        Get-PSCallStack | Out-File -FilePath $LogFilePath -Append
    }
}

Function TestStatusGetContext()
{
	param (
		[Parameter(Mandatory=$true)][string] $Id,
		[Parameter(Mandatory=$true)][string] $Scenario,
		[Parameter(Mandatory=$true)][string] $Tag	
	)

    $Id = $Id.Replace(" ","_")
    $Scenario = $Scenario.Replace(" ","_")
    $Tag = $Tag.Replace(" ","_")

    $Logger = @{Id = $Id; Scenario = $Scenario; Tag = $Tag}
	
	InitVars $Logger | Out-Null

	Log "Logger: $Id $Scenario $Tag" | Out-Null

	return $Logger
}

