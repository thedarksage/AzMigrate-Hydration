<#
.SYNOPSIS
This wrapper script is used to update config and trigger deployment.
#>

# Populate arguments from environment variables
$SubscriptionId = $env:SubscriptionId
$SubscriptionName = $env:SubscriptionName
$LogDirectory = $env:LoggingDirectory
$WorkingDirectory = $env:WorkingDirectory
$CSName = $env:CSName
$CSIPAddress = $env:CSIPAddress
$CSUsername = $env:CSUsername
$VCenterName = $env:VCenterName
$VCenterIPAddress = $env:VCenterIPAddress
$VCenterUsername = $env:VCenterUsername
$LinuxMTVMInfo = $env:LinuxMTVMInfo
$WinMTVMInfo = $env:WinMTVMInfo
$BranchName = $env:Branch
$WindowsSourceVMInfo = $env:WindowsSourceVMInfo
$LinuxSourceVMInfo = $env:LinuxSourceVMInfo
$JBIPAddress = $env:JBIPAddress
$JBUserName = $env:JBUserName
$JBPassword = $env:JBPassword
$JBSharePath = $env:JBSharePath
$ResourceGroup = $env:ResourceGroup
$ResourceGroupId = $env:ResourceGroupId
$StorageAccountId = $env:StorageAccountId
$VnetId = $env:VnetId
$Subnet = $env:Subnet
$VaultName = $env:VaultName
$BuildLocation = $env:BuildLocation
$CSPassword = $env:CSPassword
$VCenterPassword = $env:VCenterPassword
$VMPassword = $env:VMPassword

$IsReportingEnabled = "false"
$TestBinRoot = "C:\V2AGQL"
$CSHttpsPort = "443"
$VCenterPort = "443"
$LinuxMTUAFile = "Microsoft-ASR_UA_*_UBUNTU-16.04-64_*_release.tar.gz"
$StartTime   = $(Get-Date -Format o)
$LogName = "$LogDirectory\Deployment_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")

Write-Host "VaultName : $VaultName"
Write-Host "SourceVMUserName : $SourceVMUserName"
Write-Host "SubscriptionId : $SubscriptionId"
Write-Host "SubscriptionName : $SubscriptionName"
Write-Host "LogDirectory : $LogDirectory"
Write-Host "TestBinRoot : $TestBinRoot"
Write-Host "CSName : $CSName"
Write-Host "CSIPAddress : $CSIPAddress"
Write-Host "CSHttpsPort : $CSHttpsPort"
Write-Host "CSUsername : $CSUsername"
Write-Host "VCenterName : $VCenterName"
Write-Host "VCenterIPAddress : $VCenterIPAddress"
Write-Host "VCenterPort : $VCenterPort"
Write-Host "VCenterUsername : $VCenterUsername"
Write-Host "LinuxMTVMInfo : $LinuxMTVMInfo"
Write-Host "LinuxMTUAFile : $LinuxMTUAFile"
Write-Host "CSPassword : $CSPassword"
Write-Host "VCenterPassword : $VCenterPassword"
Write-Host "VMPassword : $VMPassword"
Write-Host "Branch : $Branch"
Write-Host "WindowsSourceVMInfo : $WindowsSourceVMInfo"
Write-Host "LinuxSourceVMInfo : $LinuxSourceVMInfo"
Write-Host "ResourceGroup : $ResourceGroup"
Write-Host "ResourceGroupId : $ResourceGroupId"
Write-Host "VnetId : $VnetId"
Write-Host "Subnet : $Subnet"
Write-Host "BuildLocation : $BuildLocation"
Write-Host "IsReportingEnabled : $IsReportingEnabled"
Write-Host "LogName : $LogName"
Write-Host "JBIPAddress : $JBIPAddress"
Write-Host "JBSharePath : $JBSharePath"
Write-Host "JBUserName : $JBUserName"

#
# Include common library files
#
$Error.Clear()

$ResultFile = $PSScriptRoot + "\DefaultV2AGQLStatus.xml"

$GQLStatusFile = $PSScriptRoot + "\V2AGQLStatus.xml"

if (!$(Test-Path -Path $GQLStatusFile)) {
    Write-Host "Copying $ResultFile to $GQLStatusFile during first run"
    Copy-Item $ResultFile $GQLStatusFile

    if (!$?)
    {
        $errorMsg = "Failed to copy $ResultFile to $GQLStatusFile"
        Write-Host "$errorMsg"
        throw $errorMsg
    }
}

$result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

Function CreateLogDir() {
    if ( !$(Test-Path -Path $LogDirectory -PathType Container) ) {
        Write-Host "Creating the directory $LogDirectory"
        New-Item -Path $LogDirectory -ItemType Directory -Force >> $null

        if ( !$? ) {
            Write-Error "Error creating log directory $LogDirectory"
            throw "Error creating log directory $LogDirectory"
        }
    }
}

Function Logger {
	param(
		[Parameter(Mandatory=$true)]
		[String] $Msg
	)
	
	Write-Output "$(Get-Date -Format "yyyy-MM-dd HH:mm:ss") $Msg" | out-file $LogName -Append
}

Function UpdateConfigFile
{
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.CONFIGUPDATE -eq "true")
        {
            Write-Host "Skipping the update config operation as it was run earlier!"
            return
        }
        else 
        {
            $ConfigFile = $PSScriptRoot + "\V2AGQLConfig.xml"
            $DefConfigFile = Join-Path -Path $PSScriptRoot -ChildPath "DefaultV2AGQLConfig.xml"
            Copy-Item -Path $DefConfigFile -Destination $ConfigFile -Force

            Logger "INFO : Updating $ConfigFile"
            $xmlDoc = [System.Xml.XmlDocument](Get-Content $ConfigFile)
            $config = $xmlDoc.V2AGQLConfig

            # Update AzureFabric Node
            $AzureElement = $config.AzureFabric
            $AzureElement.VaultName = $VaultName
            $AzureElement.SubscriptionName = $SubscriptionName
            $AzureElement.SubscriptionId = $SubscriptionId
            $AzureElement.ResourceGroup = $ResourceGroup
            $AzureElement.ResourceGroupId = $ResourceGroupId
            $AzureElement.StorageAccountId = $StorageAccountId
            $AzureElement.VnetId = $VnetId
            $AzureElement.Subnet = $Subnet

            $InMageFabric = $config.InMageFabric

            # Update Configuration Server Node
            $ConfigServer = $InMageFabric.ConfigServer
            $ConfigServer.Name = $CSName
            $ConfigServer.FQDN = $CSName + ".fareast.corp.microsoft.com"
            $ConfigServer.IPAddress = $CSIPAddress
            $ConfigServer.HttpsPort = $CSHttpsPort
            $ConfigServer.UserName = $CSUsername
            $ConfigServer.Password = $CSPassword

            # Update VCenter Node
            $VCenter = $InMageFabric.VCenter
            $VCenter.Name = $VCenterName
            $VCenter.FriendlyName = $VCenterName
            $VCenter.IpAddress = $VCenterIPAddress
            $VCenter.Port = $VCenterPort
            $VCenter.FQDN = $VCenterName + ".fareast.corp.microsoft.com"
            $VCenter.UserName = $VCenterUsername
            $VCenter.Password = $VCenterPassword

            $MTList = $InMageFabric.MasterTargetServerList
            # Update Windows MT details
            if ($null -ne $WinMTVMInfo) {
                $MTInfo = $WinMTVMInfo.split(",")
                $MTList.MasterTarget[0].FriendlyName = $MTInfo[0]
                $MTList.MasterTarget[0].OsType = "Windows"
                $MTList.MasterTarget[0].Datastore = $MTInfo[1]
                $MTList.MasterTarget[0].Retention = $MTInfo[2]
                $MTList.MasterTarget[0].UserName = $MTInfo[3]
                $MTList.MasterTarget[0].Password = $CSPassword
            }

            # Update Master Target Node for Linux
            if ($null -ne $LinuxMTVMInfo) {
                $MTInfo = $LinuxMTVMInfo.split(",")

                $MTList.MasterTarget[1].FriendlyName = $MTInfo[0]
                $MTList.MasterTarget[1].OsType = "Linux"
                $MTList.MasterTarget[1].Datastore = $MTInfo[1]
                $MTList.MasterTarget[1].Retention = $MTInfo[2]
                $MTList.MasterTarget[1].UserName = $MTInfo[3]
                $MTList.MasterTarget[1].Password = $VMPassword
            }

            # Add Source VMs Information
            $sourceServerListNode = $InMageFabric.appendChild($xmlDoc.CreateElement("SourceServerList"))
            $OSType = "Windows"
            $VMInfo = $WindowsSourceVMInfo
            do {
                if ($VMInfo) {
                    $VMList = $VMInfo.Split(":")
                    for ($i = 0; $i -lt $VMList.Length; $i++) {
                        $VM = $VMList[$i].Split(",")
                        $sourceServerNode = $sourceServerListNode.appendChild($xmlDoc.CreateElement("SourceServer"))
                        
                        $newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("Name"))
                        $newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[0]))
                        
                        $newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("IPAddress"))
                        $newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[1]))
                        
                        $newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("OsType"))
                        $newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($OSType))
                        
                        $newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("UserName"))
                        $newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VM[2]))
                        
                        $newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("Password"))
                        $newXmlNameElement.AppendChild($xmlDoc.CreateTextNode($VMPassword))

                        $newXmlNameElement = $sourceServerNode.AppendChild($xmlDoc.CreateElement("UninstallStatus"))
                        $newXmlNameElement.AppendChild($xmlDoc.CreateTextNode("Fail"))
                    }
                }
                if ($OSType -match "Linux") {
                    break
                }
                
                $OSType = "Linux"
                $VMInfo = $LinuxSourceVMInfo
            } while(1)

            # Update Deployment Node
            $Deployment = $config.Deployment
            $Deployment.LinuxMTUAFile = $LinuxMTUAFile
            $Deployment.SharedLocation = $JBSharePath
            
            $Deployment.Testbinroot = $TestBinRoot
            $Deployment.BuildLocation = $BuildLocation
            $Deployment.BuildType = $BranchName
            $Deployment.BranchName = $BranchName.ToLower()
            $Deployment.IsReportingEnabled = $IsReportingEnabled

            Logger "Saving $ConfigFile"
            $xmlDoc.Save($ConfigFile)

            Logger "INFO : Successfully updated $ConfigFile"
            $result.GQLResult.CONFIGUPDATE = "true"
            $result.Save($GQLStatusFile)
        }
    }
    catch {
        Throw
    }
}

Function CopyFiles
{
    param (
        [Parameter(Mandatory=$true)][string[]] $Files,
        [Parameter(Mandatory=$true)][string] $DestDir,
        [parameter(Mandatory=$False)] [bool] $ThrowOnError = $true
    )

    $retCode = $true
    for ($i=0; $i -lt $Files.length; $i++) {
        $file = $JBSharePath + "\" + $Files[$i]
        if ( !(Test-Path $file)) {
            Logger "The file $file doesn't exist"
            $retCode = $false
        }

        if ($retCode)
        {
            Logger "Attempting to copy $file to $DestDir"
            Copy-Item $file -Destination $DestDir -ErrorAction stop -force
            $ret = $?
            Logger "Copy file $file return value $ret"
            if (!$ret) {
                Logger "Failed to copy the file $file from $JBSharePath. Please refer $JBSharePath\$file for deployment log"
                if ($ThrowOnError -eq $true)
                {
                    Throw "Failed to copy the file $file from $JBSharePath. Please refer $JBSharePath\$file for deployment log"
                }
            }
            else
            {
                Logger "Successfully copied file $file from $JBSharePath"
            }
        }
    }
}

Function CopyBinariesToTDM
{
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.COPYBINARIESTOTDM -eq "true")
        {
            Write-Host "Skipping the COPYBINARIESTOTDM operation as it was run earlier!"
            return
        }
        else 
        {
            # Copy the configuration file V2AGQLConfig.xml and vault credentials from shared location
            Logger "Copying the binaries to TDM"

            $DestDir = "C:\V2AGQL"

            $secpasswd = ConvertTo-SecureString $JBPassword -AsPlainText -Force
            $credential = New-Object System.Management.Automation.PSCredential($JBUserName, $secpasswd)
		
            $networkPath = "\\" + $JBIPAddress + "\c$"
            Logger "Network Path : $networkPath"
            Remove-PSDrive -Name P
            New-PSDrive -Name P -PSProvider FileSystem -Persist -Root $networkPath -Credential $credential
            if (!$?) {
                Logger "The C drive on TDM is not accessible for copying the scripts"
                Throw
            }
            $vaultFile = $VaultName + ".VaultCredentials"
            $Files = @($vaultFile, "MySQLPasswordFile.txt", "AddAccounts.exe", "csgetfingerprint.exe")
            CopyFiles -Files $Files -DestDir $WorkingDirectory
            
            $Path = "P:\V2AGQL"
            Remove-Item $Path -Recurse -Force
            New-Item $Path -Type directory
            Copy-Item $WorkingDirectory\* $Path -Exclude @("UserLogs", "V2AGQLLogs") -Recurse
            $ret = $?
            Logger "Copy file $file return value $ret"
            if (!$ret) {
                Logger "Failed to copy files from $WorkingDirectory to $DestDir on TDM"
                Throw
            }
            else
            {
                Logger "Successfully copied copy files from $PSScriptRoot to $DestDir on TDM"
            }
            $result.GQLResult.COPYBINARIESTOTDM = "true"
            $result.Save($GQLStatusFile)

            & NET USE Z: /D
        }
    }
    catch {
        Throw
    }
}

Function RunDeployment()
{
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.RUNDEPLOYMENT -eq "true")
        {
            Logger "Skipping the RUNDEPLOYMENT operation as it was run earlier!"
            return
        }
        else 
        {
            Set-Item wsman:\localhost\Client\TrustedHosts -value '*' -Force
            Get-Item wsman:\localhost\Client\TrustedHosts
            
            $secpasswd = ConvertTo-SecureString $JBPassword -AsPlainText -Force
            $credential = New-Object System.Management.Automation.PSCredential($JBUserName, $secpasswd)

            $session = New-PSSession -ComputerName $JBIPAddress -Credential $credential
            
            Logger "Session : $session"

            $startTime = (Get-Date)
            Invoke-Command -Session $session -Command {eventcreate /l APPLICATION /so "ASRInfratrigger" /t INFORMATION /id 22 /d "ASRInfraDeploymenttrigger"}
            do {
                Start-Sleep 30
                $output = Invoke-Command -Session $session -Command { Get-Content "C:\V2AGQL\V2AGQLLogs\RUNDEPLOYMENT.log" }
                $results = Select-String "Deployment Completed Successfully" -InputObject $output
                if ($results) {
                    Logger "V2A GQL Deployment completed successfully"
                    Write-Host "Updating RUNDEPLOYMENT status as success"
                    $result.GQLResult.RUNDEPLOYMENT = "true"
                    $result.Save($GQLStatusFile)
                    $status = "Passed"
                    break
                }
                else {
                    $results = Select-String "Deployment Failed" -InputObject $output
                    if ($results) {
                        Logger "V2A GQL Deployment failed"
                        break
                    }
                }
                
                $elapsedTime = $(get-date) - $startTime
                Logger "ElapsedTime : $elapsedTime. Waiting for deployment operation to be completed!!"
            } while ($elapsedTime.TotalHours -le 4)

            if ($elapsedTime.TotalHours -gt 4) {
                Logger "V2A GQL Deployment didn't complete within timeout"
                $status = "Failed"
            }

            $retCode = $true
            # Copy the deployment logs from TDM shared location
            Logger "Copying the binaries to $JBSharePath"
            & NET USE Z: /D
            & NET USE Z: $JBSharePath /user:$JBUserName $JBPassword
            if (!$?) {
                Logger "Failed to access the shared path $JBSharePath"
                $retCode = $false
            }

            if ($retCode)
            {
                $Files = @("RUNDEPLOYMENT.log", "deployment.log")
                CopyFiles -Files $Files -DestDir $LogDirectory -ThrowOnError $false
            }
            & NET USE Z: /D

            if ($status -ne "Passed") {
                Throw "V2A GQL deployment failed, refer RunDeployment.log for more details"
            }
        }
    }
    catch {
        Throw
    }
}

Function PopulateResultTrx
{
    param (
        [parameter(Mandatory=$True)] [string]$TestCaseResult,
        [parameter(Mandatory=$False)] [string]$ErrorMessage
    )

    try {
         $resultTrx = -join($PSScriptRoot,"\DeploymentResult.trx")

         If(Test-path $resultTrx) {
            Write-Host "Removing old $resultTrx file"
            Remove-item $resultTrx 
         }

         $TestResults = [xml]('<?xml version="1.0" encoding="utf-8" standalone="no"?>'+
            '<TestRun>'+
            '<ResultSummary outcome="Passed">'+
            '<Counters total="0" executed="0" passed="0" error="0" failed="0" timeout="0" aborted="0" inconclusive="0" passedButRunAborted="0" notRunnable="0" notExecuted="0" disconnected="0" warning="0" completed="0" inProgress="0" pending="0" />'+
            '</ResultSummary>'+
            "<Times creation=`"$(Get-Date -Format o)`" queuing=`"$(Get-Date -Format o)`" start=`"`" finish=`"`" />"+
            '<TestSettings id="010e155f-ff0f-44f5-a83e-5093c2e8dcc4" name="Settings">'+
            '</TestSettings>'+
            '<TestDefinitions></TestDefinitions>'+
            '<TestLists>'+
            '<TestList name="Results Not in a List" id="8c84fa94-04c1-424b-9868-57a2d4851a1d" />'+
            '<TestList name="All Loaded Results" id="19431567-8539-422a-85d7-44ee4e166bda" />'+
            '</TestLists>'+
            '<TestEntries></TestEntries>'+
            '<Results></Results>'+
        '</TestRun>')

        $TestRun = $TestResults.SelectSingleNode('/TestRun')
        $TestRun.SetAttribute('name','Deployment Run')
        $TestRun.SetAttribute('xmlns','http://microsoft.com/schemas/VisualStudio/TeamTest/2010')
        $testrunid = [guid]::NewGuid() -replace '{}',''
        $TestRun.SetAttribute('id',$testrunid)
        $TestDefinitions=$TestResults.SelectSingleNode('/TestRun/TestDefinitions')
        $TestEntries=$TestResults.SelectSingleNode('/TestRun/TestEntries')
        $Results = $TestResults.SelectSingleNode('/TestRun/Results')
        $ResultsSummary = $TestResults.SelectSingleNode('/TestRun/ResultSummary')                                              
        $Times = $TestResults.SelectNodes('/TestRun/Times')

        $TestCase = "Deployment"
        $message = '{0} status {1}' -f $TestCase, $TestCaseResult
    
        #Write-Output $message
        $EndTime = $(Get-Date -Format o)

        $duration = New-TimeSpan -Start $StartTime -End $EndTime
        if ($duration -lt 0) 
        {
            $duration = -$duration
            Write-Host "duration : $duration"
        }
        Write-Host "TestCase : $TestCase, TestCaseResult : $TestCaseResult, startTime : $startTime, endTime: $endTime, duration : $duration"

        $TestDefinition = $TestResults.CreateElement('UnitTest')
        $null = $TestDefinitions.AppendChild($TestDefinition)
        $id = [guid]::NewGuid() -replace '{}',''
        $TestDefinition.SetAttribute('name',$TestCase)
        $TestDefinition.SetAttribute('id',$id)
        $TestDefinition.SetAttribute('storage',"$resultTrx")
        $executionid= [guid]::NewGuid() -replace '{}',''
        $Execution = $TestResults.CreateElement('Execution')
        $null = $TestDefinition.AppendChild($Execution)
        $Execution.SetAttribute('id',$executionid)
        $TestMethod = $TestResults.CreateElement('TestMethod')
        $null = $TestDefinition.AppendChild($TestMethod)
        $TestMethod.SetAttribute('codeBase',$PSScriptRoot + "\RunDeployment.ps1")
        $TestMethod.SetAttribute('className', 'Deployment')
        $TestMethod.SetAttribute('name',$TestCase)
        $TestEntry = $TestResults.CreateElement('TestEntry')
        $null = $TestEntries.AppendChild($TestEntry)
        $TestEntry.SetAttribute('testId',$id)
        $TestEntry.SetAttribute('executionId',$executionid)
        $TestEntry.SetAttribute('testListId','8c84fa94-04c1-424b-9868-57a2d4851a1d')
        $Result = $TestResults.CreateElement('UnitTestResult')
        $null = $Results.AppendChild($Result)
        $Result.SetAttribute('executionId',$executionid)
        $Result.SetAttribute('testId',$id)
        $Result.SetAttribute('testName',$TestCase)
        $Result.SetAttribute('computerName',$env:COMPUTERNAME)
        $Result.SetAttribute('duration',$duration)
        $Result.SetAttribute('startTime',$StartTime)
        $Result.SetAttribute('endTime',$EndTime)
        if ($TestCaseResult -ne 'NotExecuted')
        {
            $ResultsSummary.Counters.executed = (1+$ResultsSummary.Counters.executed).ToString()
        }
        $ResultsSummary.Counters.total = (1+$ResultsSummary.Counters.total).ToString()
        if ($Times.GetAttribute('start') -eq '') {
            $Times.SetAttribute('start',$StartTime)
        }
        $Times.SetAttribute('finish',$EndTime)
        
        if ($TestCaseResult -eq 'Passed') 
        {
            $TestResult = 'Passed'
            $ResultsSummary.Counters.completed = (1+$ResultsSummary.Counters.completed).ToString()
            $ResultsSummary.Counters.passed = (1+$ResultsSummary.Counters.passed).ToString()
            $ResultsSummary.SetAttribute('outcome','Passed')
        }
        elseif ($TestCaseResult -eq 'NotExecuted') 
        {
            $TestResult = 'NotExecuted'
            $ResultsSummary.Counters.notExecuted = (1+$ResultsSummary.Counters.notExecuted).ToString()
            $ResultsSummary.SetAttribute('outcome','NotExecuted')
        }
        else 
        {
            $TestResult = 'Failed'
            $ResultsSummary.Counters.completed = (1+$ResultsSummary.Counters.completed).ToString()
            $ResultsSummary.Counters.failed = (1+$ResultsSummary.Counters.failed).ToString()
            $Output = $TestResults.CreateElement('Output')
            $null = $Result.AppendChild($Output)
            $ErrorInfo = $TestResults.CreateElement('ErrorInfo')
            $null = $Output.AppendChild($ErrorInfo)
            $Message = $TestResults.CreateElement('Message')
            $null = $ErrorInfo.AppendChild($Message)
            $Message.InnerText = $ErrorMessage
            $ResultsSummary.SetAttribute('outcome','Failed')
        }
        $Result.SetAttribute('outcome',$TestResult)
        $Result.SetAttribute('testListId','8c84fa94-04c1-424b-9868-57a2d4851a1d')
        $Result.SetAttribute('testType','13cdc9d9-ddb5-4fa4-a97d-d965ccfc6d4b')
        Write-Host "Saving test results to $resultTrx (current working folder is $(Get-Location))"
        $TestResults.Save($resultTrx)
        Write-Output $TestResults
    }
    catch {
        Write-Host "Failed to create result $resultTrx file"
        Throw
    }
}

Function Main
{
    try {
        CreateLogDir
        
        Write-Host "UpdateConfigFile"
        UpdateConfigFile

        Write-Host "CopyBinariesToTDM"
        CopyBinariesToTDM
        
        Write-Host "RunDeployment"
        RunDeployment

        $status = "Passed"
        Write-Host "Source V2A GQL Deployment Succeeded"
    }
    catch
    {
        Write-Error "ERROR Message:  $_.Exception.Message"
        Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"
        $status = "Failed"
        $errorMessage = "V2A GQL Deployment FAILED"
        Write-Error $errorMessage
        throw $errorMessage
    }
    finally
    {
        if ( $(Test-Path -Path $LogDirectory -PathType Container) ) {
            Write-Host "Copying all the xml files in $PSScriptRoot to $LogDirectory directory"
            Copy-Item -path "$PSScriptRoot\*.xml" -destination $LogDirectory -ErrorAction SilentlyContinue

            if (!$?)
            {
                Write-Host "Failed to copy config files to $LogDirectory"
            }
        }
        PopulateResultTrx -TestCaseResult $status -ErrorMessage $errorMessage
    }
}

Main