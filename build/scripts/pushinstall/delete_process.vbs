On Error Resume Next

Set objArgs = WScript.Arguments
strDebugLog = "delete_process.log"
'******************************************************************************
strComputer = objArgs(0)
strInstallDir = objArgs(1)
strUsername = objArgs(2)
strPassword = objArgs(3)

WriteTextFile strDebugLog, "Push Un-Install Job about to start on " & strComputer 
strFxService = "frsvc"
strVxService = "svagents"
g_strMonScript = "monitor_process.vbs"
strExeBaseName= "unins000.exe /SILENT /SUPPRESSMSGBOXES"

strCommand = strInstallDir & "\" & strExeBaseName 
strExtraCommand = " && echo success > " & "C:" & "\success.txt"

'******************************************************************************

On Error Resume Next

'Connect to WMI.
Set objSWbemLocator = CreateObject("WbemScripting.SWbemLocator")
Set objWMIService = objSWbemLocator.ConnectServer(strComputer, "root\cimv2", strUsername,strPassword)
objWMIService.Security_.ImpersonationLevel = 3
objWMIService.Security_.authenticationLevel=pktPrivacy

If Err.Number <> 0 Then
	WriteTextFile strDebugLog, "WMI connection to " & strComputer & " Failed with Error " & Err.Number
	HandleError Err, strComputer
	WScript.Quit 1
Else
	'Check if remote host already has VX/FX installed and quit if so
	Set fxService = objWMIService.Get("Win32_Service.Name='" & strFxService & "'")
	Set vxService = objWMIService.Get("Win32_Service.Name='" & strVxService & "'")
	If IsObject(fxService) or IsObject(vxService) Then
		WriteTextFile strDebugLog, "Remote host " & strComputer & " already has VX/FX agent installed. Proceeding for uninstallation..."
		intPID = CreateProcess(strCommand)
		If intPID <> -1 Then
			WriteTextFile strDebugLog, "Successfully created process on remote host with PID " & intPID
			WriteTextFile strDebugLog, "Going to start monitor script on this PID..."
			ExecMonitorScript strComputer, intPID, "C:"
			WScript.Sleep 180000
			WriteTextFile strDebugLog, "Waited for Three Minutes."
			WriteTextFile strDebugLog, "Remote host " & strComputer & " VX/FX agent uninstalled."
			WScript.Quit 0
		Else
			WriteTextFile strDebugLog, "Unable to create the process " & strCommand & " " & strExtraCommand
			HandleError Err, strComputer
			WScript.Quit 1
		End If
	End If
End If

'******************************************************************************

On Error Resume Next

Set objFSO = CreateObject("Scripting.FileSystemObject")
If Not objFSO.FileExists(strSourceFile) Then
  WriteTextFile strDebugLog, "Error: File " & strSourceFile & " not found."
  WScript.Quit 1
End If
If Not objFSO.FolderExists(strTargetFolder) Then
  objFSO.CreateFolder(strTargetFolder)
End If
If Err = 0 Then
  objFSO.CopyFile strSourceFile, strTargetFolder & "\"
  If Err = 0 Then
     WriteTextFile strDebugLog,"Copied file " & strSourceFile & " to folder " & _
     strTargetFolder
    FileCopy = 0
  Else
    WriteTextFile strDebugLog, "Unable to copy file " & strSourceFile & " to folder " & _
     strTargetFolder
    FileCopy = 2
    HandleError Err, strHost
	WScript.Quit 1
  End If
Else
  WriteTextFile strDebugLog, "Unable to create folder " & strTargetFolder
  FileCopy = 1
  HandleError Err, strHost
  WScript.Quit 1
End If


Function CreateProcess(strCL)
'Create a process.

On Error Resume Next

Set objProcess = objWMIService.Get("Win32_Process")
intReturn = objProcess.Create _
 (strCL, Null, Null, intProcessID)
If intReturn = 0 Then
  WriteTextFile strDebugLog, "Process Created." & _
   vbCrLf & "Command line: " & strCL & _
   vbCrLf & "Process ID: " & intProcessID
  CreateProcess = intProcessID
Else
  WriteTextFile strDebugLog, "Process could not be created." & _
   vbCrLf & "Command line: " & strCL & _
   vbCrLf & "Return value: " & intReturn
  CreateProcess = -1
End If

End Function

'******************************************************************************

' Runs an external program and pipes it's output to
' the StdOut and StdErr streams of the current script.
' Returns the exit code of the external program.
Function Run (ByVal cmd)

	Dim sh: Set sh = CreateObject("WScript.Shell")
	Dim wsx: Set wsx = Sh.Exec(cmd)

	If wsx.ProcessID = 0 And wsx.Status = 1 Then
		' (The Win98 version of VBScript does not detect WshShell.Exec errors)
		Err.Raise vbObjectError,,"WshShell.Exec failed."
	End If

	Do
		Dim Status: Status = wsx.Status
		WScript.StdOut.Write wsx.StdOut.ReadAll()
		WScript.StdErr.Write wsx.StdErr.ReadAll()
		If Status <> 0 Then Exit Do
		WScript.Sleep 10
	Loop

	Run = wsx.ExitCode

End Function

'******************************************************************************

Sub ExecMonitorScript(strHost, intProcessID, strRemoteDir)
'Launch second script to monitor process deletion events

On Error Resume Next

strCommandLine = "cscript " & g_strMonScript & " " & _
 strHost & " " & intProcessID & " " & strRemoteDir & " " & strUsername & " " & strPassword
strCommandLineToPrint = "cscript " & g_strMonScript & " " & _
 strHost & " " & intProcessID & " " & strRemoteDir & " " & strUsername & " ********"
 
WriteTextFile strDebugLog, "Command line for monitoring PID on remote host is: " & vbCrLf & strCommandLines & vbCrLf

WriteTextFile strDebugLog, "Running command line:" & vbCrLf & _
   strCommandLineToPrint

Run(strCommandLine)

End Sub

'******************************************************************************

Sub HandleError(Err, strHost)
'Handle errors.

strError = "Computer Name: " & strHost & VbCrLf & _
 "ERROR " & Err.Number & VbCrLf & _
 "Description: " & Err.Description & VbCrLf & _
 "Source: " & Err.Source
WriteTextFile strDebugLog, strError   

'If Err <> 0   Then
      On Error Resume   Next

      Dim strErrDesc: strErrDesc = Err.Description
      Dim ErrNum: ErrNum = Err.Number
      Dim WMIError : Set WMIError = CreateObject("WbemScripting.SwbemLastError")
      WriteTextFile strDebugLog, "Error from WMI: " 
      If ( TypeName(WMIError) = "Empty" ) Then
         WriteTextFile strDebugLog, strErrDesc & " (HRESULT: "   & Hex(ErrNum) & ")."
      Else
         WriteTextFile strDebugLog, WMIError.Description & "(HRESULT: " & Hex(ErrNum) & ")."
         Set WMIError = nothing
      End   If
'End If
Err.Clear

End Sub

'******************************************************************************

'Write or append data to text file.
Sub WriteTextFile(strFileName, strOutput)


On Error Resume Next

Const FOR_APPENDING = 8

'Open text file for output.
'WScript.Echo strOutput
Set objFSO = CreateObject("Scripting.FileSystemObject")
If objFSO.FileExists(strFileName) Then
  Set objTextStream = objFSO.OpenTextFile(strFileName, FOR_APPENDING)
Else
  Set objTextStream = objFSO.CreateTextFile(strFileName)
End If

'Write data to file.
objTextStream.WriteLine strOutput
objTextStream.WriteLine

objTextStream.Close

End Sub

