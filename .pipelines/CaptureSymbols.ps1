Param(
[Parameter(Mandatory=$true)]
    [String]$SymbolsFilesList,
[Parameter(Mandatory=$true)]
    [String]$SourceFolder,
[Parameter(Mandatory=$true)]
    [String]$TargetFolder
)

# This function captures symbol files.
function CaptureSymbolFiles
{
    Write-Host ("Symbols file list:", $SymbolsFilesList)
    Write-Host ("Source folder:", $SourceFolder)
    Write-Host ("Target folder:", $TargetFolder)

    foreach($SymbolsFile in [System.IO.File]::ReadLines($SymbolsFilesList))
    {
        $SourceFile = $SourceFolder + "\" + $SymbolsFile
        $TargetFile = $TargetFolder + "\" + $SymbolsFile
        Write-Host ("`nSource file:", $SourceFile)
        Write-Host ("Target file:", $TargetFile)

        if (!(Test-Path $SourceFile -PathType Leaf)) {
            Write-Host ("This source file doesn't exist:", $SourceFile)
            exit 1
        }

        New-Item -path $TargetFile -ItemType File -Force
        Copy-Item $SourceFile -Destination $TargetFile -PassThru -Recurse -ErrorAction stop
    }
}

try
{
    # Capture symbol files.
    CaptureSymbolFiles $SymbolsFilesList $SourceFolder $TargetFolder
}
catch
{
    Write-Host "Hit below excecption."	
    Write-Host $_.Exception | Format-List 
    exit 1
}