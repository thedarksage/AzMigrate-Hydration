<#
.SYNOPSIS
This wrapper script is used to update config, deploy a VM and run 540 on it.
#>

# Populate arguments from environment variables
$BranchRunPrefix = $env:BranchRunPrefix
$OSName = $env:OSName
$OSType = $env:OSType
$PrimaryLocation = $env:PrimaryLocation
$RecoveryLocation = $env:RecoveryLocation
$SubscriptionId = $env:SubscriptionId
$SubscriptionName = $env:SubscriptionName
$VaultRGLocation = $env:VaultRGLocation
$LogDirectory = $env:LoggingDirectory
$TestBinRoot = $env:WorkingDirectory
$SourceVMUserName = $env:USERNAME
$AgentVersion = $env:AgentVersion
$ExtensionVersion = $env:ExtensionVersion
$ExtensionType = $env:ExtensionType
$RequiredKernel = $env:RequiredKernel
$ValidationType = $env:ValidationType
$EnableMailReporting = "false";
$IsPremiumDR = $env:IsPremiumDR
$DoReboot = $env:DoReboot
$RunFSHealthCheck = $env:RunFSHealthCheck


Write-Host "SourceVMUserName : $SourceVMUserName"
Write-Host "BranchRunPrefix : $BranchRunPrefix"
Write-Host "OSType : $OSType"
Write-Host "OSName : $OSName"
Write-Host "PrimaryLocation : $PrimaryLocation"
Write-Host "RecoveryLocation : $RecoveryLocation"
Write-Host "SubscriptionId : $SubscriptionId"
Write-Host "SubscriptionName : $SubscriptionName"
Write-Host "VaultRGLocation : $VaultRGLocation"
Write-Host "LogDirectory : $LogDirectory"
Write-Host "TestBinRoot : $TestBinRoot"
Write-Host "ExtensionType : $ExtensionType"
Write-Host "ExtensionVersion : $ExtensionVersion"
Write-Host "AgentVersion : $AgentVersion"
Write-Host "RequiredKernel : $RequiredKernel"
Write-Host "ValidationType : $ValidationType"
Write-Host "IsPremiumDR : $IsPremiumDR"
Write-Host "DoReboot : $DoReboot"
Write-Host "RunFSHealthCheck : $RunFSHealthCheck"

$global:LogDir = $LogDirectory

$Error.Clear()

$host.ui.RawUI.WindowTitle = $OSName
Write-Host "Test: $TestBinRoot"

if ($null -eq $DoReboot)
{
    $DoReboot = "true"
    Write-Host "DoReboot : $DoReboot"
}

if ($null -eq $RunFSHealthCheck)
{
    $RunFSHealthCheck = "true"
    Write-Host "RunFSHealthCheck : $RunFSHealthCheck"
}

if ($null -ne $ValidationType)
{
    $ValidationType = $ValidationType.ToUpper()
}

if ($null -ne $ValidationType -and ($ValidationType -ne "TEST" -and $ValidationType -ne "PROD"))
{
    Write-Host "Invalid argument passed for ValidationType - $ValidationType. Please provide ValidationType as 'Test' or 'Prod'"
    exit 1
}

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

$TestResultsMap = @{}

# List of TestCases to populate test results trx file.
if ($ValidationType -eq "PROD")
{
    $TestResultsMap = [ordered]@{    "CONFIGUPDATE" = TestResultsMetaData
                        "CLEANPREVIOUSRUN" = TestResultsMetaData
                        "CREATEVM" = TestResultsMetaData
                        "ENABLEREPLICATION" = TestResultsMetaData
                        "DISABLEREPLICATION" = TestResultsMetaData
                        "CLEANUPRESOURCES" = TestResultsMetaData
                    }
}
else {
    $TestResultsMap = [ordered]@{    "CONFIGUPDATE" = TestResultsMetaData
                        "CLEANPREVIOUSRUN" = TestResultsMetaData
                        "CREATEVM" = TestResultsMetaData
                        "ENABLEREPLICATION" = TestResultsMetaData
                        "REBOOT" = TestResultsMetaData
                        "TESTFAILOVER" = TestResultsMetaData
                        "FAILOVER" = TestResultsMetaData
                        "SWITCHPROTECTION" = TestResultsMetaData
                        "FAILBACK" = TestResultsMetaData
                        "REVERSESWITCHPROTECTION" = TestResultsMetaData
                        "DISABLEREPLICATION" = TestResultsMetaData
                        "CLEANUPRESOURCES" = TestResultsMetaData
                    }
}

$global:configFile = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLConfig.xml"
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

Function UpdateConfigFile()
{
    $TestCase = "CONFIGUPDATE"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.CONFIGUPDATE -eq "true")
        {
            Write-Host "Skipping the update config operation as it was run earlier!"
            return
        }
        else 
        {
            $Script = Join-Path -Path $PSScriptRoot -ChildPath "UpdateConfig.ps1"

            if ($ValidationType -eq "TEST" -or $ValidationType -eq "PROD")
            {
                .$Script -TestBinRoot $TestBinRoot -SubscriptionName "$SubscriptionName" -SubscriptionId $SubscriptionId -PrimaryLocation $PrimaryLocation -RecoveryLocation $RecoveryLocation `
                            -VaultRGLocation $VaultRGLocation -OSType $OSType -BranchRunPrefix $BranchRunPrefix -OSName $OSName -EnableMailReporting $EnableMailReporting -AgentVersion $AgentVersion `
                            -ExtensionVersion $ExtensionVersion -ExtensionType $ExtensionType -ValidateDeployment "true" -IsPremiumDR $IsPremiumDR -RunFSHealthCheck $RunFSHealthCheck
            }
            else {
                .$Script -TestBinRoot $TestBinRoot -SubscriptionName "$SubscriptionName" -SubscriptionId $SubscriptionId -PrimaryLocation $PrimaryLocation -RecoveryLocation $RecoveryLocation `
                            -VaultRGLocation $VaultRGLocation -OSType $OSType -BranchRunPrefix $BranchRunPrefix -OSName $OSName -ExtensionType $ExtensionType -EnableMailReporting $EnableMailReporting `
                            -IsPremiumDR $IsPremiumDR -RunFSHealthCheck $RunFSHealthCheck
            }

            $status = $?

            if ($status) {
                Write-Host "Updating update config status as success"
                $result.GQLResult.CONFIGUPDATE = "true"
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

Function UpdateTestCaseResults()
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


Function Login()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    Write-Host "Test: $PSScriptRoot"

    .$Script "LOGIN"
    
    Write-Host "Login status : $?"
}

Function SetVaultContext()
{
    $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

    Write-Host "Test: $PSScriptRoot"

    .$Script "SETVAULTCONTEXT"
    
    Write-Host "SETVAULTCONTEXT status : $?"
}

Function DisableCleanAzureResources()
{
    $TestCase = "CLEANPREVIOUSRUN"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.CLEANPREVIOUSRUN -eq "true")
        {
            Write-Host "Skipping the DisableCleanAzureResources operation as it was run earlier!"
            return
        }

        Write-Host "curr : $PSScriptRoot"
        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

        .$Script "CLEANPREVIOUSRUN"

        $status = $?
        if ($status) {
            Write-Host "Updating CLEANPREVIOUSRUN operation status as success"
            $result.GQLResult.CLEANPREVIOUSRUN = "true"
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

Function CreateVM() {
    param (
        [Parameter(Mandatory=$true)][string] $SubscriptionId,
        [Parameter(Mandatory=$true)][string] $PrimaryLocation,
        [Parameter(Mandatory=$true)][string] $BranchRunPrefix,
        [Parameter(Mandatory=$true)][string] $OSName
    )

    $TestCase = "CREATEVM"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.CREATEVM -eq "true")
        {
            Write-Host "Skipping the CreateVM operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "CreateNewVm.ps1"

        if ($ValidationType -eq "TEST" -or $ValidationType -eq "PROD")
        {
            .$Script -SubId $SubscriptionId -Location $PrimaryLocation -VmUserName $SourceVMUserName -BranchRunPrefix $BranchRunPrefix -OSName $OSName -OSType $OSType -RequiredKernel $RequiredKernel
        }
        else {
            .$Script -SubId $SubscriptionId -Location $PrimaryLocation -VmUserName $SourceVMUserName -BranchRunPrefix $BranchRunPrefix -OSName $OSName -OSType $OSType
        }

        $status = $?
        if ($status) 
        {
            Write-Host "Updating CREATEVM operation status as success"
            $result.GQLResult.CREATEVM = "true"
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

Function EnableReplication()
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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

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

Function TestFailover()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )

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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

        .$Script "TFO" $ResourceGroupType

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

Function Reboot()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )

    $TestCase = "REBOOT"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.REBOOT -eq "true")
        {
            Write-Host "Skipping the REBOOT operation as it was run earlier!"
            return
        }

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

        .$Script "REBOOT" $ResourceGroupType

        $status = $?
        if ($status) {
            Write-Host "Updating REBOOT operation status as success"
            $result.GQLResult.REBOOT = "true"
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

Function UnplannedFailover()
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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

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

Function SwitchProtection()
{
    param (
        [Parameter(Mandatory=$true)][string] $ResourceGroupType
    )
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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

        .$Script "SP" $ResourceGroupType

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

Function FailBack()
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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

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

Function ReverseSwitchProtection()
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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

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

Function DisableReplication()
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

        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

        .$Script "DR"

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

Function CleanupResources()
{
    $TestCase = "CLEANUPRESOURCES"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try 
    {
        $result = [System.Xml.XmlDocument](Get-Content $GQLStatusFile)

        if ($result.GQLResult.CR -eq "true")
        {
            Write-Host "Skipping the CLEANUPRESOURCES operation as it was run earlier!"
            return
        }

        Write-Host "curr : $PSScriptRoot"
        $Script = Join-Path -Path $PSScriptRoot -ChildPath "A2AGQLTest.ps1"

        .$Script "CR"

        $status = $?
        Write-Host "TestCase : $TestCase, Status : $status"
        if ($status) {
            Write-Host "Updating CR operation status as success"
            $result.GQLResult.CR = "true"
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


Function DeleteResultTrx
{
    param (
        [parameter(Mandatory=$True)] [string]$resultTrx
    )

    Write-Host "Checking if file $resultTrx exists or not!"

    If(Test-path $resultTrx) {
        Write-Host "Removing old $resultTrx file"
        Remove-item $resultTrx

        if (!$?)
        {
            Write-Host "Failed to remove $resultTrx"
        }
    }
}

Function PopulateResultTrx
{
    try {
        $resultTrx = -join($PSScriptRoot,"\", $OSName,".trx")
        
        DeleteResultTrx -resultTrx $resultTrx

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

        $TotalTestCasesCount = $TestResultsMap.count
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
                $TestMethod.SetAttribute('className', $OSName)
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

#
# Generate .trx result file for all the test cases, so that CloudTest will consume and populate the errors in the results dashboard.
#
Function Main
{
    try {
        CreateLogDir

        Write-Host "UpdateConfigFile"
        UpdateConfigFile
    
        Write-Host "Login"
        Login

        Write-Host "DisableCleanAzureResources"
        DisableCleanAzureResources

        Write-Host "CreateVM"
        CreateVM $SubscriptionId $PrimaryLocation $BranchRunPrefix $OSName

        Write-Host "Set Vault Context"
        SetVaultContext

        Write-Host "EnableReplication"
        EnableReplication

        if ($DoReboot -eq "true")
        {
            Write-Host "Reboot"
            Reboot "Primary"
        }

        if ($ValidationType -ne "PROD")
        {
            Write-Host "TestFailover"
            TestFailover "Recovery"

            Write-Host "UnplannedFailover"
            UnplannedFailover

            Write-Host "SwitchProtection"
            SwitchProtection "Recovery"

            Write-Host "FailBack"
            FailBack

            Write-Host "ReverseSwitchProtection"
            ReverseSwitchProtection
        }

        Write-Host "DisableReplication"
        DisableReplication

        Write-Host "CleanupResources" 
        CleanupResources

        Write-Host "Source A2A GQL Run Succeeded"
        exit 0
    }
    catch
    {
        Write-Error "ERROR Message:  $_.Exception.Message"
        Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"

        Write-Error "Source A2A GQL Run failed"

        $errorMessage = "A2A GQL Run FAILED"
        throw $errorMessage
    }
    finally
    {
        if ( $(Test-Path -Path $LogDirectory -PathType Container) ) {
            Write-Host "Copying all the xml files in $TestBinRoot to $LogDirectory directory"
            Copy-Item -path "$TestBinRoot\*.xml" -destination $LogDirectory -ErrorAction SilentlyContinue

            if (!$?)
            {
                Write-Host "Failed to copy config files to $LogDirectory"
            }
        }

        PopulateResultTrx
    }
}

Main