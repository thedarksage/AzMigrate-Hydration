try
{
    # Set ProcessStartInfo attributes.
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false

    # Download InnoSetup installer.
    $innoSetupUri = 'https://files.jrsoftware.org/is/5/innosetup-5.5.9-unicode.exe'
    $innoSetupInstaller = "$env:Temp\innosetup-5.5.9-unicode.exe"
    Write-Host "Downloading InnoSetup installer..."
    Invoke-RestMethod -Uri $innoSetupUri -OutFile $innoSetupInstaller -UseBasicParsing    

    # Install InnoSetup version 5.5.9.
    Write-Host "Installing ${innoSetupInstaller}..."
    $pinfo.FileName = $innoSetupInstaller
    $pinfo.Arguments = @("/VERYSILENT", "/NORESTART", "/ALLUSERS", "/LOG=$env:Temp\InnoSetup-Install.log")	
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) 
    { 
        Write-Host ('InnoSetup installation failed with exit code:', $p.ExitCode)
        exit 1	
    } 
    else 
    {
       Write-Host ('InnoSetup installation succeeded.')
    }
} 
catch
{
    Write-Host "Hit below excecption."	
    Write-Host $_.Exception | Format-List 
    exit 1
}