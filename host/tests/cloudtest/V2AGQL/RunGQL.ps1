<#
.SYNOPSIS
This wrapper script is used to trigger 540 on source machine.
#>

# Populate arguments from environment variables
$LogDirectory = $env:LoggingDirectory
$WorkingDirectory = $env:WorkingDirectory
$OSType = $env:OSType
$VMName = $env:VMName
$IPAddress = $env:IPAddress
$Username = $env:Username
$Password = $env:Password
$TestBinPath = $env:TestBinPath 
$JBIPAddress = $env:JBIPAddress
$JBUserName = $env:JBUserName
$JBSharePath = $env:JBSharePath
$JBPassword = $env:JBPassword

$LogName = "$LogDirectory\RunGQL_{0}.log" -f (Get-Date -Format "MM-dd-yyyy HH.mm.ss")

Write-Host "VMName : $VMName"
Write-Host "IPAddress : $IPAddress"
Write-Host "Username : $Username"
Write-Host "OSType : $OSType"
Write-Host "LogDirectory : $LogDirectory"
Write-Host "WorkingDirectory : $WorkingDirectory"
Write-Host "Password : $Password"
Write-Host "LogName : $LogName"

$global:LogDir = $LogDirectory
$resultTrx = -join($WorkingDirectory,"\", $VMName, ".trx")

#
# Include common library files
#
$Error.Clear()

Function TestResultsMetaData
{
    $dataset = @{
        Status      = "NotExecuted"
        StartTime   = $(Get-Date -Format o)
        EndTime     = $(Get-Date -Format o)
        ErrMessage  = ""
    }
<#
    $dataset.keys | ForEach-Object{
        $message = '{0} = {1}' -f $_, $property[$_]
        Write-Output $message
    }
#>
    return $dataset
}

$TestResultsMap = [ordered]@{   "COPYBINARIES" = TestResultsMetaData
                                "UPPDATESOURCEVMINFO" = TestResultsMetaData
                                "VALIDATE" = TestResultsMetaData
                                "ENABLEREPLICATION" = TestResultsMetaData
                                "TESTFAILOVER" = TestResultsMetaData
                                "FAILOVER" = TestResultsMetaData
                                "SWITCHPROTECTION" = TestResultsMetaData
                                "FAILBACK" = TestResultsMetaData
                                "REVERSESWITCHPROTECTION" = TestResultsMetaData
                                "DISABLEREPLICATION" = TestResultsMetaData
                                "DRP" = TestResultsMetaData
                            }

$ResultFile = $PSScriptRoot + "\DefaultGQLResult.xml"

$GQLStatusFile = $PSScriptRoot + "\GQLStatus.xml"

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

Function CopyBinaries
{
    $TestCase = "COPYBINARIES"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase    
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.COPYBINARIES -eq "true")
        {
            Write-Host "Skipping the COPYBINARIES operation as it was run earlier!"
            return
        }
        else 
        {
            # Copy the configuration file V2AGQLConfig.xml and vault credentials from shared location
            Logger "Copying the binaries to $JBSharePath"
            
            & NET USE Z: /D
            & NET USE Z: $JBSharePath /user:$JBUserName $JBPassword
            if (!$?) {
                Logger "Failed to access the shared path $JBSharePath"
                Throw "Failed to access the shared path $JBSharePath"
            }

            $Files = @("V2AGQLConfig.xml", ".\*.VaultCredentials")
            
            for ($i=0; $i -lt $Files.length; $i++) {
                $file = $JBSharePath + "\" + $Files[$i]
                if ( !(Test-Path $file)) {
                    Logger "The file $file doesn't exist"
                    Throw "The file $file doesn't exist"
                }
                
                Copy-Item $file -Destination $PSScriptRoot -ErrorAction stop -force
                $ret = $?
                Logger "Copy file $file return value $ret"
                if (!$ret) {
                    Logger "Failed to copy the file $file"
                    Throw "Failed to copy the file $file"
                }
                else
                {
                    Logger "Successfully copied file $file"
                }
            }
            $result.GQLResult.COPYBINARIES = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"

            & NET USE Z: /D
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function UpdateSourceVMInfo
{
    $TestCase = "UPPDATESOURCEVMINFO"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.UPPDATESOURCEVMINFO -eq "true")
        {
            Logger "Skipping the UPPDATESOURCEVMINFO operation as it was run earlier!"
            return
        }
        else 
        {
            $SrcUsername = $Username

            if ($OSType -match "Windows") {
                $SrcUsername = $VMName + "\" + $Username
            }

            Logger "Updating source vm $SrcUsername details in the config file"
            $Script = Join-Path -Path $PSScriptRoot -ChildPath "UpdateSourceInfo.ps1"
            .$Script -VMName $VMName -IPAddress $IPAddress -OSMode $OSType -User "$SrcUsername" -Passwd $Password -LogDirectory $LogDirectory

            $cmdstatus = $?

            if ($cmdstatus) {
                Write-Host "Updating UPPDATESOURCEVMINFO status as success"
                $result.GQLResult.UPPDATESOURCEVMINFO = "true"
                $result.Save($GQLStatusFile)
                $status = "Passed"
            }
            else {
                $status = "Failed"
                Throw
            }
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function Validate
{
    $TestCase = "VALIDATE"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)
        
        if ($result.GQLResult.VALIDATE -eq "true")
        {
            Write-Host "Skipping the VALIDATE operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "VALIDATE"
        $status = $?

        if ($status) {
            Write-Host "Updating VALIDATE operation status as success"
            $result.GQLResult.VALIDATE = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function EnableReplication
{
    $TestCase = "ENABLEREPLICATION"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)
        
        if ($result.GQLResult.ER -eq "true")
        {
            Write-Host "Skipping the EnableReplication operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "ER"
        
        $status = $?
        if ($status) {
            Write-Host "Updating ER operation status as success"
            $result.GQLResult.ER = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function TestFailover
{
    $TestCase = "TESTFAILOVER"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)
        if ($result.GQLResult.TFO -eq "true")
        {
            Write-Host "Skipping the TestFailover operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "TFO"

        $status = $?
        if ($status) {
            Write-Host "Updating TFO operation status as success"
            $result.GQLResult.TFO = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function UnplannedFailover
{
    $TestCase = "FAILOVER"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.UFO -eq "true")
        {
            Write-Host "Skipping the UnplannedFailover operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "UFO"

        $status = $?
        if ($status) {
            Write-Host "Updating UFO operation status as success"
            $result.GQLResult.UFO = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function SwitchProtection
{
    $TestCase = "SWITCHPROTECTION"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.SP -eq "true")
        {
            Write-Host "Skipping the SwitchProtection operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "SP"

        $status = $?
        if ($status) {
            Write-Host "Updating SP operation status as success"
            $result.GQLResult.SP = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function FailBack
{
    $TestCase = "FAILBACK"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.FB -eq "true")
        {
            Write-Host "Skipping the FailBack operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "FB"

        $status = $?
        if ($status) {
            Write-Host "Updating FB operation status as success"
            $result.GQLResult.FB = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function ReverseSwitchProtection
{
    $TestCase = "REVERSESWITCHPROTECTION"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.RSP -eq "true")
        {
            Write-Host "Skipping the ReverseSwitchProtection operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "RSP"

        $status = $?
        if ($status) {
            Write-Host "Updating RSP operation status as success"
            $result.GQLResult.RSP = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function DisableReplication
{
    $TestCase = "DISABLEREPLICATION"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.DR -eq "true")
        {
            Write-Host "Skipping the DisableReplication operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "DISABLE"

        $status = $?
        if ($status) {
            Write-Host "Updating DR operation status as success"
            $result.GQLResult.DR = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function DisableReplicationPolicy
{
    $TestCase = "DRP"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.DRP -eq "true")
        {
            Write-Host "Skipping the DRP operation as it was run earlier!"
            return
        }

        Write-Host "curr : $PSScriptRoot"
        $Script = Join-Path -Path $PSScriptRoot -ChildPath "V2AGQLTest.ps1"

        .$Script "DRP"

        $status = $?
        Write-Host "TestCase : $TestCase, Status : $status"
        if ($status) {
            Write-Host "Updating DRP operation status as success"
            $result.GQLResult.DRP = "true"
            $result.Save($GQLStatusFile)
            $status = "Passed"
        }
        else {
            $status = "Failed"
            Throw
        }
    }
    catch {
        $testCaseData.ErrMessage = $Error
        $status = "Failed"
        Throw
    }
    finally
    {
        $TestCaseResults = UpdateTestCaseResults -TestCaseData $testCaseData -StartTime $startTime -Status $status -TestCase $TestCase
        $TestResultsMap.$TestCase = $TestCaseResults
    }
}

Function UpdateTestCaseResults
{
    param (
        [Parameter(Mandatory=$true)][object] $TestCaseData,
        [Parameter(Mandatory=$true)][string] $Status,
        [Parameter(Mandatory=$true)][string] $TestCase,
        [parameter(Mandatory=$True)] [PSObject]$StartTime
    )

    $testCaseData.Keys | ForEach-Object{
        $message = '{0} - {1} - {2}' -f $Testcase, $_, $testCaseData[$_] 
        Write-Output $message
    }

    $tmp = $testCaseData.StartTime
    Write-Host "Modifying $TestCase starttime from $tmp to $StartTime"
    $testCaseData.Status = $Status
    $testCaseData.StartTime = $StartTime
    $testCaseData.EndTime = $(Get-Date -Format o)

    return $testCaseData
}

Function DeleteFile
{
    param (
        [parameter(Mandatory=$True)] [string]$fileName
    )

    Write-Host "Checking if file $fileName exists or not!"

    If(Test-path $fileName) {
        Write-Host "Removing old $fileName file"
        Remove-item $fileName

        if (!$?)
        {
            Write-Host "Failed to remove $fileName"
        }
    }
}

Function PopulateResultTrx
{
    try {
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
        $TestRun.SetAttribute('name','GQL Tests')
        $TestRun.SetAttribute('xmlns','http://microsoft.com/schemas/VisualStudio/TeamTest/2010')
        $testrunid = [guid]::NewGuid() -replace '{}',''
        $TestRun.SetAttribute('id',$testrunid)
        $TestDefinitions=$TestResults.SelectSingleNode('/TestRun/TestDefinitions')
        $TestEntries=$TestResults.SelectSingleNode('/TestRun/TestEntries')
        $Results = $TestResults.SelectSingleNode('/TestRun/Results')
        $ResultsSummary = $TestResults.SelectSingleNode('/TestRun/ResultSummary')                                              
        $Times = $TestResults.SelectNodes('/TestRun/Times')

        $TotalTestCasesCount = $TestResultsMap.count - 1
        Write-Host "Total Test Cases : $TotalTestCasesCount"
        if ($null -ne $TestResultsMap.keys -and $TestResultsMap.keys.length -gt 0)
        {
            $TestResultsMap.keys | ForEach-Object {
                $TestCase = $_
                $message = '{0} status {1}' -f $_, $TestResultsMap[$_]
            
                #Write-Output $message
                $TestCaseData = $TestResultsMap[$_]

                $TestCaseResult = $TestCaseData.Status
                $StartTime = $TestCaseData.StartTime
                $EndTime = $TestCaseData.EndTime
                $ErrorMessage = $TestCaseData.ErrMessage

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
                $TestMethod.SetAttribute('codeBase',$PSScriptRoot + "\RunGQL.ps1")
                $TestMethod.SetAttribute('className', $VMName)
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
            }
        }
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
        
        DeleteFile -fileName $resultTrx

        Write-Host "CopyBinaries"
        CopyBinaries

        Write-Host "UpdateSourceVMInfo"
        UpdateSourceVMInfo

        Write-Host "ValidateFabric"
        Validate

        Write-Host "EnableReplication"
        EnableReplication

        Write-Host "TestFailover"
        TestFailover

        Write-Host "UnplannedFailover"
        UnplannedFailover

        Write-Host "SwitchProtection"
        SwitchProtection

        Write-Host "FailBack"
        FailBack

        Write-Host "ReverseSwitchProtection"
        ReverseSwitchProtection

        Write-Host "DisableReplication"
        DisableReplication

        Write-Host "DisableReplicationPolicy" 
        DisableReplicationPolicy

        Write-Host "Source V2A GQL Run Succeeded"
        exit 0
    }
    catch
    {
        Write-Error "ERROR Message:  $_.Exception.Message"
        Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"

        Write-Error "Source V2A GQL failed"

        $errorMessage = "V2A GQL FAILED"
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

        PopulateResultTrx
    }
}

Main