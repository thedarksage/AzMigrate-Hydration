Param
(
  [parameter(Mandatory=$true)]
  [String]
  $DIToolPath                                      
)

#Copy the DITestBinaries into AzureVm(Source)
$scriptDir = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Unzip
{
    param([string]$zipfile, [string]$outpath)

    [System.IO.Compression.ZipFile]::ExtractToDirectory($zipfile, $outpath)
}

$output = Unzip "$scriptDir\DITestBinRoot.zip" $DIToolPath
Write-Output $output

Start-Sleep(5)