<#
param(

	[Parameter(Mandatory=$true)]
    [ValidateSet("Release", "Daily")]
    [String] $BuildType,

    [Parameter(Mandatory=$true)]
    [String] $BuildVersion,

    [Parameter(Mandatory=$true)]
    [String[]] $Distros,
	
	[Parameter(Mandatory=$true)]
	[ValidateSet("GA", "HOTFIX")]
    [string]$DeploymentType,

    [Parameter(Mandatory=$false)]
    [String] $BuildDate = "Default",

	[Parameter(Mandatory=$false)]
    [string]$TargetDirectory = "$pwd",

    [Parameter(Mandatory=$true)]
    [string[]]$Owners,

    [Parameter(Mandatory=$true)]
    [string[]]$Signers
	
)
#>

# ########### Header ###########
# Refer common library file
. $PSScriptRoot\Utils.ps1
# #############################


#========Global Variables Start============#
$BuildType = $env:BuildType
$BuildPath = $env:BuildPath
$BuildVersion = $env:BuildVersion
$Distros = $env:Distros
$DeploymentType = $env:DeploymentType
$BuildDate = $env:BuildDate
$TargetDirectory = $env:TargetDirectory
$LogDirectory = $env:LoggingDirectory
$TestBinRoot = $env:WorkingDirectory
$Owners = $env:Owners
$Signers = $env:Signers

#========Global Variables End============#

# Uncomment below code block and modify the variables as per the requirement for local runs.
<#
$BuildType = "DEVELOP"
$BuildPath = "\\inmstagingsvr.corp.microsoft.com\DailyBuilds\Daily_Builds\9.53\HOST"
$BuildPath = "\\InmStagingSvr.fareast.corp.microsoft.com\DailyBuilds\Daily_Builds\9.53\HOST\15_Dec_2022"
$BuildVersion = "9.53.6544.1"
$Distros = "all"
$DeploymentType = "GA"
$BuildDate = "Default"
$TargetDirectory = $PSScriptRoot
$LogDirectory = "$PSScriptRoot\AgentUploadLogs"
$TestBinRoot = $PSScriptRoot	
$Owners = "sisunkar@microsoft.com"
$Signers = "sisunkar@microsoft.com"
#>

Write-Host "BuildType : $BuildType"
Write-Host "BuildPath : $BuildPath"
Write-Host "BuildVersion : $BuildVersion"
Write-Host "Distros : $Distros"
Write-Host "OSName : $OSName"
Write-Host "DeploymentType : $DeploymentType"
Write-Host "BuildDate : $BuildDate"
Write-Host "TargetDirectory : $TargetDirectory"
Write-Host "Owners : $Owners"
Write-Host "Signers : $Signers"
Write-Host "LogDirectory : $LogDirectory"
Write-Host "TestBinRoot : $TestBinRoot"

$Error.Clear()

if ($null -ne $DeploymentType)
{
    $DeploymentType = $DeploymentType.ToUpper()
}

if ($null -ne $DeploymentType -and ($DeploymentType -ne "HOTFIX" -and $DeploymentType -ne "GA"))
{
    Write-Host "Invalid argument passed for DeploymentType - $DeploymentType. Please provide DeploymentType as 'GA' or 'HOTFIX'"
    exit 1
}

if ($null -ne $BuildType)
{
    $BuildType = $BuildType.ToUpper()
}

if ($null -ne $BuildType -and ($BuildType -ne "RELEASE" -and $BuildType -ne "DEVELOP"))
{
    Write-Host "Invalid argument passed for BuildType - $BuildType. Please provide BuildType as 'RELEASE' or 'DEVELOP'"
    exit 1
}

$StartTime   = $(Get-Date -Format o)
$InputParametersFilePath = $TargetDirectory
$InputParametersFileName = Join-Path -Path $InputParametersFilePath -ChildPath "InputParameters.json"
$InputParametersFileNameForSpec = Join-Path -Path $InputParametersFilePath -ChildPath "InputParametersSpec.json"
$InputToStage2SpecFile = Join-Path -Path $TargetDirectory -ChildPath "InputToStage2.json"

if ($DeploymentType -eq "GA") {
    $SpvFilePath = Join-Path -Path $TargetDirectory -ChildPath "spv.json"
    if (! (Test-Path -Path $SpvFilePath)) {
        Write-Error "spv.json file is missing in : " $TargetDirectory
        exit 1
    }
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
$TestResultsMap = [ordered]@{    "GENSPEC1" = TestResultsMetaData
                        "CREATEAGENTJSON" = TestResultsMetaData
                        "CREATEAGENTDOWNLOAD" = TestResultsMetaData
                        "UPDATESTAGE2SPEC" = TestResultsMetaData
                        "GENSPEC2" = TestResultsMetaData
                        "VALIDATESPEC" = TestResultsMetaData
                        "CREATESPEC" = TestResultsMetaData
                        "CREATESPECDOWNLOAD" = TestResultsMetaData
                    }

$ResultFile = $PSScriptRoot + "\DefaultUploadResult.xml"

$StatusFile = $PSScriptRoot + "\UploadStatus.xml"

if (!$(Test-Path -Path $StatusFile)) {
    Write-Host "Copying $ResultFile to $StatusFile during first run"
    Copy-Item $ResultFile $StatusFile

    if (!$?)
    {
        $errorMsg = "Failed to copy $ResultFile to $StatusFile"
        Write-Host "$errorMsg"
        throw $errorMsg
    }
}

$result = [System.Xml.XmlDocument](Get-Content $StatusFile)

class UploadInputParameters {
	[string]$DownloadName
	[string]$Owners
	[string]$SystemRequirements
	[string]$Instructions
	[string]$OperatingSystem
    [string]$Version
    [string]$ContainThirdPartyBits
    [System.Collections.ArrayList]$Signers = @()
    [System.Collections.ArrayList]$FilesPath = @()
}

Function GetSignersList
{
    [System.Collections.ArrayList]$signerList = @()
    Write-Host "Signers : $Signers"

    if ($Signers.Contains(","))
    {
        $signersList = $Signers.Split(",")
        $signersList | Foreach-Object {
            $currSigner = $_
            $FilePath.replace('\','\\')
            $signerList += @($currSigner)
        }
    }
    else {
        $signerList += @($Signers)
    }

    return $signerList
}

Function GenerateSpec1
{
    $TestCase = "GENSPEC1"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.GENSPEC1 -eq "true")
        {
            Write-Host "Skipping the GENSPEC1 operation as it was run earlier!"
            return
        }
        else
        {
            & "$PSScriptRoot\GenSpec1.ps1" -BuildPath $BuildPath -BuildVersion $BuildVersion -Distros $Distros -DeploymentType $DeploymentType -TargetDirectory $TargetDirectory

            $status = $?
            if ($status) {
                Write-Host "Updating GENSPEC1 operation status as success"
                $result.Result.GENSPEC1 = "true"
                $result.Save($StatusFile)
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

Function Get-BuildsPath {
	$remoteBuildPath = "\\InmStagingSvr.fareast.corp.microsoft.com\"

    if ($BuildType -eq "Release") {
        $remoteBuildPath += "ReleaseBuilds\"
    }
    else {
        $remoteBuildPath += "DailyBuilds\Daily_Builds\"
    }

    $remoteBuildPath += $BuildVersion

    if ($BuildType -eq "Daily") {

        $remoteBuildPath += "\Host\"
    }
    else {
        $remoteBuildPath += "\"

    }

    if ($BuildDate -eq "Default") {
        $remoteBuildPath += (Date -Format dd_MMM_yyyy)
    }
	else {
        $remoteBuildPath += "30_Jul_2022"
    }

    return $remoteBuildPath
}

Function GetAgentVersion {
    $agentBuildsPath = $BuildPath + "\UnifiedAgent_Builds\release\"

    if ( !$(Test-Path -Path $agentBuildsPath -PathType Container) ) {
        Write-Error "agentBuildsPath $agentBuildsPath doesn't exist"
        Throw
    }

    $agentVersionFile = $agentBuildsPath + "AgentVersion.txt"
    Write-Host "agentVersionFile : $agentVersionFile"

    Write-Host "agentVersionFile : $agentVersionFile"
    if (!$(Test-Path -Path $agentVersionFile -PathType Leaf))
    {
        Write-Error "agentVersionFile $agentVersionFile doesn't exist"
        Throw
    }

    $version = Get-Content -Path $agentVersionFile

    if ($Version -eq '' -or $null -eq $Version)
    {
        Write-Error "Agent Version $Version is not valid"
        exit 1
    }

    Write-Host "Agent Version : $Version"
    return $version
}

Function GetRandom
{
	$rand = Get-Random -minimum 1 -maximum 1000
	return $rand
}
Function CreateJsonForAgentFilesUpload
{
    $TestCase = "CREATEAGENTJSON"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.CREATEAGENTJSON -eq "true")
        {
            Write-Host "Skipping the CREATEAGENTJSON operation as it was run earlier!"
            return
        }
        else
        {
            $Version = GetAgentVersion

            Get-ChildItem $Version |
            Foreach-Object {
                $FilePath = $_.FullName
                $FilePath.replace('\','\\')
                $FilesPath += @($FilePath)
            }

            $OwnersList = $Owners -join ";"

            $rand = GetRandom

            $SpecObj = @{}

            $GlobalInfoObj = New-Object UploadInputParameters
            $GlobalInfoObj.DownloadName = "ASRMobSvc_" + $rand + "_" + $Version
            $GlobalInfoObj.Owners = $OwnersList
            $GlobalInfoObj.SystemRequirements = "Windows Server 2016"
            $GlobalInfoObj.Instructions = "Follow wizard instructions"
            $GlobalInfoObj.OperatingSystem = "Windows Server 2016"
            $GlobalInfoObj.Version = $Version
            $GlobalInfoObj.ContainThirdPartyBits = "true"
            $GlobalInfoObj.Signers = @(GetSignersList)
            $GlobalInfoObj.FilesPath = $FilesPath

            $SpecObj = $GlobalInfoObj

            $SpecObj | ConvertTo-Json | Out-File -FilePath $InputParametersFileName

            $status = $?
            if ($status) {
                Write-Host "Updating CREATEAGENTJSON operation status as success"
                $result.Result.CREATEAGENTJSON = "true"
                $result.Save($StatusFile)
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

Function CreateAgentDownload
{
    $TestCase = "CREATEAGENTDOWNLOAD"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.CREATEAGENTDOWNLOAD -eq "true")
        {
            Write-Host "Skipping the CREATEAGENTDOWNLOAD operation as it was run earlier!"
            return
        }
        else
        {
            & "$PSScriptRoot\DMSUploader.exe" /Operation createdownload /DMSRequiredParametersFile `"$InputParametersFileName`"

            $status = $?
            if ($status) {
                Write-Host "Updating CREATEAGENTDOWNLOAD operation status as success"
                $result.Result.CREATEAGENTDOWNLOAD = "true"
                $result.Save($StatusFile)
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

Function GetDownloadFilePath
{
    $Version = GetAgentVersion
    $DownloadLinksFileName = -join("DownloadFilePath_ASRMobSvc_", $Version, ".txt");
    $downloadFilePath = Join-Path -Path $PSScriptRoot -ChildPath $DownloadLinksFileName

    return $downloadFilePath
}

Function GetUploadPath {
[CmdletBinding()]
    param (
        [Parameter(Mandatory=$true, Position=1)]
        [String] $downloadFilePath
    )

    $UploadPath = Get-Content $downloadFilePath -First 1
    $UploadLink = $UploadPath.Substring(0, $UploadPath.lastIndexOf('/'))
    return $UploadLink
}

Function UpdateInputToStage2SpecFile {
    $TestCase = "UPDATESTAGE2SPEC"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.UPDATESTAGE2SPEC -eq "true")
        {
            Write-Host "Skipping the UPDATESTAGE2SPEC operation as it was run earlier!"
            return
        }
        else
        {
            $JsonObj = (Get-Content $InputToStage2SpecFile) | convertfrom-json
            $downloadFilePath = GetDownloadFilePath
            $UploadPath = GetUploadPath -downloadFilePath $downloadFilePath
            $JsonObj.CommonReleaseInfo.PartLink = $UploadPath
            $JsonString = $JsonObj | ConvertTo-Json
            $JsonString | Set-Content -Path $InputToStage2SpecFile -Force

            $status = $?
            if ($status) {
                Write-Host "Updating UPDATESTAGE2SPEC operation status as success"
                $result.Result.UPDATESTAGE2SPEC = "true"
                $result.Save($StatusFile)
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

Function GenerateSpec2
{
    $TestCase = "GENSPEC2"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.GENSPEC2 -eq "true")
        {
            Write-Host "Skipping the GENSPEC2 operation as it was run earlier!"
            return
        }
        else
        {
            & "$PSScriptRoot\GenSpec2.ps1" -TargetDirectory $TargetDirectory

            $status = $?
            if ($status) {
                Write-Host "Updating GENSPEC2 operation status as success"
                $result.Result.GENSPEC2 = "true"
                $result.Save($StatusFile)
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

Function ValidateSpecFile
{
    $TestCase = "VALIDATESPEC"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.VALIDATESPEC -eq "true")
        {
            Write-Host "Skipping the VALIDATESPEC operation as it was run earlier!"
            return
        }
        else
        {
            $Version = GetAgentVersion
            # TODO : This parser binary needs to be fetched from RCM Repo Artifacts.
            $MobilityAgentSpecParser = "$TargetDirectory\MobilityAgentSpecParser\MobilityAgentSpecParser.exe"
            $CanSpecfile = "ASRMobSvcSpecCan_" + $Version + ".json"
            $ProdSpecFile = "ASRMobSvcSpecProd_" + $Version + ".json"

            & $MobilityAgentSpecParser $CanSpecfile

            if ($LASTEXITCODE -eq 0)
            {
                Write-Host "Successfully validated $CanSpecfile Spec File"
                & $MobilityAgentSpecParser $ProdSpecFile

                if($LASTEXITCODE -ne 0)
                {
                    Write-Error "Spec $ProdSpecFile validation failed with $LASTEXITCODE"
                    $status = "Failed"
                    Throw
                }
                Write-Host "Successfully validated $ProdSpecFile Spec File"
            }
            else {
                Write-Error "Spec $CanSpecfile validation failed with $LASTEXITCODE"
                $status = "Failed"
                Throw
            }

            Write-Host "Updating VALIDATESPEC operation status as success"
            $result.Result.VALIDATESPEC = "true"
            $result.Save($StatusFile)
            $status = "Passed"

            Write-Host "Successfully validated spec files"
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

Function CreateJsonForSpecFilesUpload
{
    $TestCase = "CREATESPEC"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.CREATESPEC -eq "true")
        {
            Write-Host "Skipping the CREATESPEC operation as it was run earlier!"
            return
        }
        else
        {
            $Version = GetAgentVersion
            $CanSpecFile = -join("ASRMobSvcSpecCan_" + $Version + ".json");
            $CanSpecFilePath = Join-Path -Path $TargetDirectory -ChildPath $CanSpecFile;

            $ProdSpecFile = -join("ASRMobSvcSpecProd_" + $Version + ".json");
            $ProdSpecFilePath = Join-Path -Path $TargetDirectory -ChildPath $ProdSpecFile;

            $FilesPath = @()
            $FilesPath += $CanSpecFilePath
            $FilesPath += $ProdSpecFilePath

            $rand = GetRandom

            $SpecObj = @{}

            $GlobalInfoObj = New-Object UploadInputParameters
            $GlobalInfoObj.DownloadName = "ASRMobSvcSpec_" + $rand + "_" + $Version
            $GlobalInfoObj.Owners = $Owners
            $GlobalInfoObj.SystemRequirements = "Windows Server 2016"
            $GlobalInfoObj.Instructions = "Windows Server 2016"
            $GlobalInfoObj.OperatingSystem = "Windows Server 2016"
            $GlobalInfoObj.Version = $Version
            $GlobalInfoObj.ContainThirdPartyBits = "true"
            $GlobalInfoObj.Signers = @(GetSignersList)
            $GlobalInfoObj.FilesPath = $FilesPath

            $SpecObj = $GlobalInfoObj

            $SpecObj | ConvertTo-Json | Out-File -FilePath $InputParametersFileNameForSpec

            $status = $?
            if ($status) {
                Write-Host "Updating CREATESPEC operation status as success"
                $result.Result.CREATESPEC = "true"
                $result.Save($StatusFile)
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

Function CREATESPECDOWNLOAD
{
    $TestCase = "CREATESPECDOWNLOAD"
    $status = "NotExecuted"
    $startTime = $(Get-Date -Format o)
    $testCaseData = $TestResultsMap.$TestCase
    try {
        $result = [System.Xml.XmlDocument](Get-Content $StatusFile)

        if ($result.Result.CREATESPECDOWNLOAD -eq "true")
        {
            Write-Host "Skipping the CREATESPECDOWNLOAD operation as it was run earlier!"
            return
        }
        else
        {
            & "$PSScriptRoot\DMSUploader.exe" /Operation createdownload /DMSRequiredParametersFile `"$InputParametersFileNameForSpec`"

            $status = $?
            if ($status) {
                Write-Host "Updating CREATESPECDOWNLOAD operation status as success"
                $result.Result.CREATESPECDOWNLOAD = "true"
                $result.Save($StatusFile)
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

Function PopulateResultTrx
{
    param (
        [parameter(Mandatory=$True)] [string]$TestCaseResult,
        [parameter(Mandatory=$False)] [string]$ErrorMessage
    )

    try {
         $resultTrx = -join($PSScriptRoot,"\UploadAgentResult.trx")

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
        $TestRun.SetAttribute('name','Agent Upload Run')
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
        $TestMethod.SetAttribute('codeBase',$PSScriptRoot + "\UploadAgent.ps1")
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

Function UploadAgentFiles
{
    try
    {
        Write-Host "Creating Logging Directory"
        CreateLogDir

        Write-Host "GetAgentVersion"
        GetAgentVersion

        Write-Host "Running GenerateSpec1"
        GenerateSpec1

        Write-Host "Running CreateJsonForAgentFilesUpload"
        CreateJsonForAgentFilesUpload

        Write-Host "Running CreateAgentDownload"
        CreateAgentDownload

        Write-Host "Running UpdateInputToStage2SpecFile"
        UpdateInputToStage2SpecFile

        Write-Host "Running GenerateSpec2"
        GenerateSpec2

        Write-Host "Running ValidateSpecFile"
        ValidateSpecFile

        Write-Host "Running CreateJsonForSpecFilesUpload"
        CreateJsonForSpecFilesUpload

        Write-Host "Running CREATESPECDOWNLOAD"
        CREATESPECDOWNLOAD

        Write-Host "Successfully uploaded agent bits to DLC"
        $status = "Passed"
    }
    catch
    {
        Write-Error "ERROR Message:  $_.Exception.Message"
        Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"

        $status = "Failed"
        throw "Agent Upload to DLC failed"
    }

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

# UploadAgent.ps1 -BuildType DEVELOP -BuildPath "\\InmStagingSvr.fareast.corp.microsoft.com\DailyBuilds\Daily_Builds\9.53\HOST\15_Dec_2022" -BuildVersion 9.51 -Distros DEBIAN7-64,DEBIAN8-64,DEBIAN9-64,DEBIAN10-64,OL6-64,OL7-64,OL8-64,RHEL5-64,RHEL6-64,RHEL7-64,RHEL8-64,SLES11-SP3-64,SLES11-SP4-64,SLES12-64,SLES15-64,UBUNTU-14.04-64,UBUNTU-16.04-64,UBUNTU-18.04-64,UBUNTU-20.04-64,Windows -DeploymentType GA -Owners xxx@microsoft.com -Signers xxx@microsoft.com

UploadAgentFiles