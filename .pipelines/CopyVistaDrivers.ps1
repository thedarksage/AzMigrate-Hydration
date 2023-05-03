# Script that copies Vista Drivers.
Param(
[Parameter(Mandatory=$true)]
    [String]$SourceDirectory,
[Parameter(Mandatory=$true)]
    [String]$DriverDirectoryName
)

$TargetDirectoryx86 = $SourceDirectory + "\x86\VistaRelease"
$TargetDirectoryx64 = $SourceDirectory + "\x64\VistaRelease"
$SourceDirectory = $SourceDirectory + "\" + $DriverDirectoryName + "*"
$SourceDriverDirectory = (Get-ChildItem $SourceDirectory).FullName
$InDskFltx86Sys = $SourceDriverDirectory + "\x86\indskflt.sys"
$InDskFltx86Pdb = $SourceDriverDirectory + "\x86\indskflt.pdb"
$InDskFltx64Sys = $SourceDriverDirectory + "\x64\indskflt.sys"
$InDskFltx64Pdb = $SourceDriverDirectory + "\x64\indskflt.pdb"


# This function copies vista driver files.
function CopyFiles
{
    Write-Host ("Vista Drivers source Directory:", $SourceDriverDirectory)
    Write-Host ("Target x86 folder:", $TargetDirectoryx86)
    Write-Host ("Target x64 folder:", $TargetDirectoryx64)
	
	New-Item -Path $TargetDirectoryx86 -ItemType Directory
	New-Item -Path $TargetDirectoryx64 -ItemType Directory

	Copy-Item $InDskFltx86Sys $TargetDirectoryx86
	Copy-Item $InDskFltx86Pdb $TargetDirectoryx86
	Copy-Item $InDskFltx64Sys $TargetDirectoryx64
	Copy-Item $InDskFltx64Pdb $TargetDirectoryx64
}

try
{
    CopyFiles $SourceFolder $TargetFolder
}
catch
{
    Write-Host "Hit below excecption."	
    Write-Host $_.Exception | Format-List 
    exit 1
}