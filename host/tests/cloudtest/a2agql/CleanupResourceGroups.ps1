<#
#.SYNOPSIS
#
# This script deletes the Resource Groups matching the a2agql tag.
# Note: a2agql tag will be assigned to the resource groups created by GQL scripts.
#>

param(
    # Script params
    # Subscription ID
    [Parameter(Mandatory=$False)]
    [string] $SubscriptionId = "41b6b0c9-3e3a-4701-811b-92135df8f9e3"
)

$LogDirectory = $env:LoggingDirectory
if ($LogDirectory -eq "" -Or $LogDirectory -eq $null)
{
    $LogDirectory = $PSScriptRoot
}

$global:LogName = "$LogDirectory\RGCleanup.log"
$global:SubscriptionId = $SubscriptionId

if ($global:SubscriptionId -eq "" -Or $global:SubscriptionId -eq $null)
{
    $global:SubscriptionId = $env:SubscriptionId
    if ($global:SubscriptionId -eq "" -Or $global:SubscriptionId -eq $null)
    {
        $global:SubscriptionId = "41b6b0c9-3e3a-4701-811b-92135df8f9e3"
    }
}

# AAD App identity to login to Azure subscription
$global:DrDatapPlaneAppClientId = "7c07fb08-b554-4955-8baf-13d2f772a65b"
$global:TenantId = "72f988bf-86f1-41af-91ab-2d7cd011db47"
$global:AgentSpnCertName = "AgentSpnCert"
$StartTime   = $(Get-Date -Format o)

Write-Host "LogName : $global:LogName"
Write-Host "SubscriptionId : $global:SubscriptionId"
Write-Host "SPN : $global:AgentSpnCertName"
Write-Host "DrDatapPlaneAppClientId : $global:DrDatapPlaneAppClientId"
Write-Host "TenantId : $global:TenantId"

Add-Type -TypeDefinition @"
   public enum LogType
   {
      Error,
      Info,
      Warning
   }
"@

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

Function LogMessage {
    param(
       [string] $Message,
       [LogType] $LogType = [LogType]::Info,
       [bool] $WriteToLogFile = $true
   )

   if ($LogType -eq [LogType]::Error) {
       $logLevel = "ERROR"
       $color = "Red"
   }
   elseif ($LogType -eq [LogType]::Info) {
       $logLevel = "INFO"
       $color = "Green"
   }
   elseif ($LogType -eq [LogType]::Warning) {
       $logLevel = "WARNING"
       $color = "Yellow"
   }

   $format = "{0,-7} : {1,-8} : {2}"
   $dateTime = Get-Date -Format "MM-dd-yyyy HH.mm.ss"
   $msg = $format -f $logLevel, $dateTime, $Message
   Write-Host $msg -ForegroundColor $color

   if ($WriteToLogFile) {
       $message = $dateTime + ' : ' + $logLevel + ' : ' + $Message
       Write-output $message | out-file $LogName -Append
   }
}

Function LoginToAzureSubscription {
    try
    {
        LogMessage -Message ("Logging to Azure Subscription : {0}" -f $global:SubscriptionId) -LogType ([LogType]::Info)
	
	    $retry = 1
	    $sleep = 0
	    while ($retry -ge 0) {
		    Start-Sleep -Seconds $sleep
		    $cert = Get-ChildItem -path 'cert:\LocalMachine\My' | Where-Object { $_.Subject.Contains($global:AgentSpnCertName) }
		    #LogMessage -Message ("User: {0}, Cert: {1}" -f $env:username, ($cert | ConvertTo-json -Depth 1)) -LogType ([LogType]::Info)
		    Start-Sleep -Seconds $sleep
		    $Thumbprint = $cert.ThumbPrint
		    if (!$cert -or !$Thumbprint) {
			    if ($retry -eq 0) {
				    LogMessage -Message ("Failed to fetch the thumbprint") -LogType ([LogType]::Error)
				    return $false
			    }
			
			    LogMessage -Message ("Failed to fetch the thumbprint..retrying") -LogType ([LogType]::Info)
			    $retry = 0
			    $sleep = 60
		    } else {
			    break
		    }
        }

        ### Connect to the Azure Account
        LogMessage -Message ("ApplicationId: {0} and TenantId: {1}" -f $global:DrDatapPlaneAppClientId, $global:TenantId) -LogType ([LogType]::Info)
	    Connect-AzAccount -ApplicationId $global:DrDatapPlaneAppClientId -Tenant $global:TenantId -CertificateThumbprint $Thumbprint  -Subscription $global:SubscriptionId
	    if (!$?) {
		    $ErrorMessage = ('Unable to login to Azure account using ApplicationId: {0} and TenantId: {1}' -f $global:DrDatapPlaneAppClientId, $global:TenantId)
            LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
            throw $ErrorMessage
	    }

        LogMessage -Message ("Login to Azure succeeded...Selecting subscription : {0}" -f $global:SubscriptionId ) -LogType ([LogType]::Info)
	    Set-AzContext -SubscriptionId $global:SubscriptionId
	    if (!$?) {
		    $ErrorMessage = ('Failed to select the subscription {0}' -f $global:SubscriptionId)
            LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
            throw $ErrorMessage
	    }
    }
    catch
    {
        $ErrorMessage = ('Failed to login to Azure Subscription with error : {0}' -f $_.Exception.Message)
        LogMessage -Message ($ErrorMessage) -LogType ([LogType]::Error)
        throw $ErrorMessage
    }

	return $true
}

workflow Remove-ResourcegroupsInParallel {
    param(
        [Parameter(Mandatory = $false)]
        [bool]
        $RemoveLocks = $false
    )
    
    $tag = @{"a2agql" = "true"}
    $rgs = Get-AzResourceGroup -Tag $Tag
    $rgs
    foreach -parallel ($resourceGroup in $rgs) {
        try
        {
            $ResourceGroupName = $resourceGroup.ResourceGroupName
            'Current Resouces Group Name: ' + $ResoucesGroupName
            if ($RemoveLocks) {
                'Removing any Locks from resources in ' + $ResourceGroupName
                Get-AzResourceLock -ResourceGroupName $ResourceGroupName | Remove-AzResourceLock -Force                
            }
                        
            'Removing resource group ' + $ResourceGroupName
            Remove-AzResourceGroup -Id $resourceGroup.ResourceId -Force -Confirm:$false
            $status = $?
            'Status : ' + $status

            if ($status)
            {
                'Successfully deleted : ' + $resourceGroup.ResourceGroupName
            }
        }
        catch {
            'Failed to delete RG - ' + $resourceGroup.ResourceGroupName
            'ERROR Message:  ' + $_.Exception.Message
        }
    }
}

Function PopulateResultTrx
{
    param (
        [parameter(Mandatory=$True)] [string]$TestCaseResult,
        [parameter(Mandatory=$False)] [string]$ErrorMessage
    )

    try {
         $resultTrx = -join($PSScriptRoot,"\CleanupResult.trx")

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
        $TestRun.SetAttribute('name','Cleanup Deployment Run')
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
        $TestMethod.SetAttribute('codeBase',$PSScriptRoot + "\CleanupResourceGroups.ps1")
        $TestMethod.SetAttribute('className', 'Cleanup')
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
        LogMessage -Message ("Saving test results to {0}" -f $resultTrx) -LogType ([LogType]::Info)
        $TestResults.Save($resultTrx)
        Write-Output $TestResults
    }
    catch {
        LogMessage -Message ("Failed to create result to {0}" -f $resultTrx) -LogType ([LogType]::Info)
        Throw
    }
}

Function Main
{
    try {
        CreateLogDir

        LogMessage -Message ("Logging to Azure Subscription") -LogType ([LogType]::Info)
        LoginToAzureSubscription

        LogMessage -Message ("Cleanup GQL Resource Groups") -LogType ([LogType]::Info)
        Remove-ResourcegroupsInParallel -RemoveLocks $true

        $status = "Passed"
        LogMessage -Message ("Cleanup Succeeded") -LogType ([LogType]::Info)
    }
    catch
    {
        Write-Error "ERROR Message:  $_.Exception.Message"
        Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"
        $status = "Failed"
        $errorMessage = "Cleanup FAILED"
        Write-Error $errorMessage
        throw $errorMessage
    }
    finally
    {
        PopulateResultTrx -TestCaseResult $status -ErrorMessage $errorMessage
    }
}

Main