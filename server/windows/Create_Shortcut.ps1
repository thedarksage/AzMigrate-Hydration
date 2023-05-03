$WshShell = New-Object -comObject WScript.Shell
Set-Variable CopyPath "$env:USERPROFILE"
$Shortcut = $WshShell.CreateShortcut("$CopyPath\Desktop\cspsconfigtool.lnk")
$Shortcut.TargetPath = "C:\home\svsystems\bin\cspsconfigtool.exe"
$Shortcut.Save()