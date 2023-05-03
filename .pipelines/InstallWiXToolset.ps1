try
{
    # Set ProcessStartInfo attributes.
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false

    # Download WiX Toolset installer.
    $wixUri = 'https://github.com/wixtoolset/wix3/releases/download/wix3111rtm/wix311.exe'
    $wixInstaller = "$env:Temp\wix311.exe"
    Write-Host "Downloading WiX installer..."
    Invoke-RestMethod -Uri $wixUri -OutFile $wixInstaller -UseBasicParsing    

    # Install WiX Toolset version 3.11.
    Write-Host "Installing ${wixInstaller}..."
    $pinfo.FileName = $wixInstaller
    $pinfo.Arguments = @('/quiet', '/norestart', '/install')	
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) 
    { 
        Write-Host ('WiX installation failed with exit code:', $p.ExitCode)
        exit 1	
    } 
    else 
    {
       Write-Host ('WiX installation succeeded.')
    }

    # Below way of installing WiX VSIX didn't work due to a known issue with VSIX installer on Server Core.
    # Ref link: https://social.msdn.microsoft.com/Forums/vstudio/en-US/a8f5185f-772e-4b12-a2ed-17501a8fe9c0/vsix-installer-doesnt-work-on-server-core?forum=vssetup
    # Worked around by copying wix.ca.targets, wix.nativeca.targets and wix.targets files to 'C:\Progr+am Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Microsoft\WiX\v3.x\'
    # through the build pipeline.
    #
    # Download WiX VSIX installer.
    # $wixVsixUri = 'https://marketplace.visualstudio.com/_apis/public/gallery/publishers/WixToolset/vsextensions/WixToolsetVisualStudio2019Extension/1.0.0.4/vspackage'
    # $wixVsixInstaller = "$env:Temp\Votive2019.vsix"
    # Write-Host "Downloading WiX VSIX installer..."
    # Invoke-RestMethod -Uri $wixVsixUri -OutFile $wixVsixInstaller -UseBasicParsing
    #
    # # Install WiX VSIX installer.
    # $vixInstaller= "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\IDE\VSIXInstaller.exe"
    # Write-Host "Installing ${wixVsixInstaller}..."
    # $pinfo.FileName = $vixInstaller
    # $pinfo.Arguments = @('/q', '/a', '/f', '/sp',"`"${wixVsixInstaller}`"")
    # $p = New-Object System.Diagnostics.Process
    # $p.StartInfo = $pinfo
    # $p.Start() | Out-Null
    # $p.WaitForExit()
    # if ($p.ExitCode -ne 0)
    # {
    #     Write-Host ('WiX VSIX installation failed with exit code:', $p.ExitCode)
    # }
    # else
    # {
    #    Write-Host ('WiX VSIX installation succeeded.')
    # }
} 
catch
{
    Write-Host "Hit below excecption."	
    Write-Host $_.Exception | Format-List 
    exit 1
}