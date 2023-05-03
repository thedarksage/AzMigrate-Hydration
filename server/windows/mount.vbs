On Error Resume Next

Set objArgs = WScript.Arguments

strsc = objArgs(0)
strsrc = Replace(strsc,"home","")

strComputer = "."
Set objWMIService = GetObject("winmgmts:\\" & strComputer & "\root\cimv2")
Set colItems = objWMIService.ExecQuery _
    ("Select * From Win32_Volume Where Name ='" & strsrc & "\'")
For Each objItem in colItems
    objItem.AddMountPoint("C:\\home")
Next