try
{
    # Set ProcessStartInfo attributes.
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false

    # Download WDK installer.
    $wdkUri = 'https://go.microsoft.com/fwlink/?linkid=2128854'
    $wdkInstaller = "$env:Temp\wdksetup.exe"
    Write-Host "Downloading WDK installer..."
    Invoke-RestMethod -Uri $wdkUri -OutFile $wdkInstaller -UseBasicParsing    

    # Install WDK version 10.0.19041.685.
    Write-Host "Installing ${wdkInstaller}..."
    $pinfo.FileName = $wdkInstaller
    $pinfo.Arguments = @('/quiet', '/norestart', '/features', 'OptionId.WindowsDriverKitComplete', 'OptionId.WDKVisualStudioDev16')	
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) 
    { 
        Write-Host ('WDK installation failed with exit code:', $p.ExitCode)
        exit 1	
    } 
    else 
    {
       Write-Host ('WDK installation succeeded.')
    }

    # Below way of installing WDK.vsix didn't work due to a known issue with VSIX installer on Server Core.
    # Ref link: https://social.msdn.microsoft.com/Forums/vstudio/en-US/a8f5185f-772e-4b12-a2ed-17501a8fe9c0/vsix-installer-doesnt-work-on-server-core?forum=vssetup
    # $vixInstaller= "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\IDE\VSIXInstaller.exe"
    # $vixFile = "C:\Program Files (x86)\Windows Kits\10\Vsix\VS2019\WDK.vsix"
    #
    # Write-Host "Installing ${vixFile}..."
    # $pinfo.FileName = $vixInstaller
    # $pinfo.Arguments = @('/q', '/a', '/f', '/sp',"`"${vixFile}`"")
    # $p = New-Object System.Diagnostics.Process
    # $p.StartInfo = $pinfo
    # $p.Start() | Out-Null
    # $p.WaitForExit()
    # if ($p.ExitCode -ne 0)
    # {
    #     Write-Host ('WDK.vxis installation failed with exit code:', $p.ExitCode)
    # }
    # else
    # {
    #    Write-Host ('WDK.vxis installation succeeded.')
    # }

    # Install WDK.vsix by copying extracted contents.
    $vixFile = "C:\Program Files (x86)\Windows Kits\10\Vsix\VS2019\WDK.vsix"
    Write-Host "Installing ${vixFile}..."
    mkdir "$env:Temp\wdkvsix"
    copy "$vixFile" "$env:Temp\wdkvsix.zip"
    Expand-Archive -Force "$env:Temp\wdkvsix.zip" -DestinationPath "$env:Temp\wdkvsix"

    # Copy VC files.
    robocopy /e $env:Temp\wdkvsix\`$MSBuild\Microsoft\VC\v160 "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Microsoft\VC\v160"
    If ($lastExitCode -ge "8") {
        Write-Host ("Failed to copy VC files. Exit code:", $lastExitCode)
        exit 1
    }
    else
    {
        Write-Host ("Copied VC files successfully. Exit code:", $lastExitCode)
    }

    # Copy wdkvsix folder.
    robocopy /e $env:Temp\wdkvsix "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\IDE\Extensions\fndsdtm4.ff7"
    If ($lastExitCode -ge "8") {
        Write-Host ("Failed to copy wdkvsix directory. Exit code:", $lastExitCode)
        exit 1
    }
    else
    {
        Write-Host ("Copied wdkvsix folder successfully. Exit code:", $lastExitCode)
    }
    Write-Host 'WDK.vxis installation succeeded.'
    exit 0
} 
catch
{
    Write-Host "Hit below excecption."	
    Write-Host $_.Exception | Format-List 
    exit 1
}