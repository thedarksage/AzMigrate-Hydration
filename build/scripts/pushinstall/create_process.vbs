On Error Resume Next

Set objArgs = WScript.Arguments

strDebugLog = "create_process.log"
WriteTextFile strDebugLog,"**************************************************************************************************************************************************************************************"
WriteTextFile strDebugLog,FormatDateTime(now)

Dim retrycount
retrycount = 1
If (objArgs.Count < 14) Or (objArgs.Count > 17) Then

	strArgError = "Arguments required and their exact order is : <remote-computer-name-or-ip> <cx-ip> <cx-port> " & _
		"<complete-path-of-exe> <UA Build Path> <username-for-vx or fx-service> <password-for-vx or fx-service-user> " & _
		"<Encrypted password-for-vx or fx-service-user> <restart-flag-Y-or-N> <domain-name-for-fx or Vx-service> <JobId> <UA Install Path> <OS Name> <IsHttpsCommunication> <PassPhraseFilePath> <DisableCodeSignVerifyCheck (0 or 1) <SymmetricCryptoKeyFilePath> >"
	WriteTextFile strDebugLog, strArgError
	WScript.Quit 1

End If

'******************************************************************************
strComputer = objArgs(0)
strCxIp = objArgs(1)
strCxPort = objArgs(2)
strExe = objArgs(3)
strBuild = objArgs(4)
strUsername = objArgs(5)
strPassword = objArgs(6)
strEncryPassword = objArgs(7)
strRestart = objArgs(8)
If objArgs.Count > 9 Then
	strDomain = objArgs(9)
	If (strDomain = "EMPTY") or (strDomain = ".") or (strDomain = "\") or (strDomain = ".\") Then
		WriteTextFile strDebugLog, "DOMAIN NAME OR WORKGROUP NAME IS NOT PROVIDED"
		strUsername = "WORKGROUP\" & strUsername				
	Else		
		strUsername = strDomain & "\" & strUsername
	End If			
End If	


strJobId=objArgs(10)
strInstallPath=objArgs(11)
strOsName=objArgs(12)
strHttps=objArgs(13)
strPassPhraseFilePath=objArgs(14)
strToPassPhraseFilePath = "C:\remoteinstall\connection.passphrase"
If objArgs.Count > 15 Then
    strDisableCodeSignatureVerification=objArgs(15)
    strSymCryptoKeyFilePath=objArgs(16)
Else
    strDisableCodeSignatureVerification="0"
    strSymCryptoKeyFilePath=""
End If

Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objFile = objFSO.GetFile(strExe)
strExeBaseName = objFile.Name

If strRestart = "N" Then 
	strRestartFlag = " /norestart "
Else
	strRestartFlag = " "
End If
WriteTextFile strDebugLog, "Push Install Job about to start on " & strComputer 
WriteTextFile strDebugLog, "Job Details "
WriteTextFile strDebugLog, " User Name : " & strUsername 
WriteTextFile strDebugLog, " Job Id : " & strJobId 
WriteTextFile strDebugLog, " Install Path : " & strInstallPath 

g_strMonScript = "monitor_process.vbs"
blnCopyFile = True 'Set to True if exe must be copied to remote machines.

strRemoteDir = "\c$\remoteinstall"
strRemoteDirSpecial = Replace(strRemoteDir,"\c$","c:")
'wbemAuthenticationLevelPktPrivacy = 6

strCommand         = strRemoteDirSpecial & "\" & strExeBaseName & " " & strCxIp & " " & strCxPort & " " & strUsername & " " & strEncryPassword & " " & strRestart & " " & strJobId & " " & Chr(34) & strInstallPath & Chr(34) & " " & strOsName & " " & strHttps & " " & Chr(34) & strPassPhraseFilePath & Chr(34) & " " & strDisableCodeSignatureVerification & " " & strSymCryptoKeyFilePath
strCommandForPrint = strRemoteDirSpecial & "\" & strExeBaseName & " " & strCxIp & " " & strCxPort & " " & strUsername & " " & "**************" & " " & strRestart & " " & strJobId & " " & Chr(34) & strInstallPath & Chr(34) & " " & strOsName & " " & strHttps & " " & Chr(34) & strPassPhraseFilePath & Chr(34) & " "
WriteTextFile strDebugLog, " Push Command Line is : " & strCommandForPrint
strFxService = "frsvc"
strVxService = "svagents"

strExtraCommand = " && echo success > " & strRemoteDirSpecial & "\success.txt"

'******************************************************************************

'On Error Resume Next

'Connect to WMI.
Set objSWbemLocator = CreateObject("WbemScripting.SWbemLocator")
Set objSWbemServices = objSWbemLocator.ConnectServer _
    (strComputer, "root\cimv2", strUsername,strPassword)
objSWbemServices.Security_.ImpersonationLevel = 3
objSWbemServices.Security_.authenticationLevel=pktPrivacy

If Err.Number <> 0 Then 
    WriteTextFile strDebugLog, "WMI connection to " & strComputer & " Failed with Error " & Err.Number
    HandleError Err, strComputer
     'Retry Logic in case of WMI Connection failures
      WScript.Sleep 5000
      WriteTextFile strDebugLog, "Waited for some time before retry count=" &retrycount
      Do 
        retrycount=retrycount+1
        'Connect to WMI.
	    Set objSWbemLocator = CreateObject("WbemScripting.SWbemLocator")
	    Set objSWbemServices = objSWbemLocator.ConnectServer _
    		    (strComputer, "root\cimv2", strUsername,strPassword)
		    objSWbemServices.Security_.ImpersonationLevel = 3
            objSWbemServices.Security_.authenticationLevel=pktPrivacy

        If Err.Number <> 0 Then
	        WriteTextFile strDebugLog, "WMI connection to " & strComputer & " Failed with Error " & Err.Number
	        WScript.Sleep 5000
	        WriteTextFile strDebugLog, "Waited for some time before retry count=" &retrycount
        Else
            Exit Do
        End If

      Loop Until retrycount>5

      If retrycount > 5 then
  	    WriteTextFile strDebugLog, "WMI connection to " & strComputer & " Failed even after 5 retires aborting installation."
	      WScript.Quit 2
      End If
Else
	'Check if remote host already has VX/FX installed and quit if so
	Set fxService = objSWbemServices.Get("Win32_Service.Name='" & strFxService & "'")
	Set vxService = objSWbemServices.Get("Win32_Service.Name='" & strVxService & "'")
	
	If IsObject(fxService) or IsObject(vxService) Then
		WScript.Echo "Remote host " & strComputer & " already has VX/FX agent installed.."
		WScript.Echo "The installation will succeed if it is an upgrade case and otherwise, it will fail."
		WriteTextFile strDebugLog, "Remote host " & strComputer & " already has VX/FX agent installed.."
		WriteTextFile strDebugLog, "The installation will succeed if it is an upgrade and otherwise it will fail."
		'WScript.Quit 6
    Else
        WriteTextFile strDebugLog, " VX/FX agent not installed on remote host " & strComputer
        Err.Clear
	End If

    If blnCopyFile Then
        'Copying PushClient.exe
        strRemoteShare = "\\" & strComputer & "\c$"
        strTarget = "\\" & strComputer & strRemoteDir
        Set objNetwork = CreateObject("WScript.Network")
        objNetwork.MapNetworkDrive "", strRemoteShare, FALSE, strUsername, strPassword 
        If Err <> 0 then
            WriteTextFile strDebugLog, "MapNetworkDrive to " & strRemoteShare & " returned " & Hex(Err.Number) & "(" & Err.Description & ")"
        End If
        intFC = FileCopy(strExe, strTarget)
        If intFC = 0 Then
	        WriteTextFile strDebugLog, "Successfully copied the file " & strExe & " to path " & strTarget
        Else
            WriteTextFile strDebugLog,"Failed to copy the file " & strExe & " to path " & strTarget
	        WScript.Quit 7
        End If

        'Copying the Unified Agent Installer (Build)
	    intFCB = FileCopy(strBuild, strTarget)
	    If intFCB = 0 Then
		    WriteTextFile strDebugLog, "Successfully copied UA installer file " & strBuild & " to path " & strTarget
        Else
		    WriteTextFile strDebugLog, "Unable to copy the UA installer file " & strBuild & " to the remote host"			
            WScript.Quit 8
	    End If	
      
        'Copying the PassPhrase File of the Cx for doing secure communication with Cx
        intPassPhrase = FileCopy(strPassPhraseFilePath,strTarget)
        If intPassPhrase = 0 Then
            WriteTextFile strDebugLog, "Successfully copied the Cx PassPhrase File" & strPassPhraseFilePath & " to path " & strTarget
        Else
            WriteTextFile strDebugLog, "Failed to copy Cx PassPhrase File that the new agent should use to identify the Cx."
            WScript.Quit 9
        End If
        
        strCommand = strRemoteDirSpecial & "\" & strExeBaseName & " " & strCxIp & " " & strCxPort & " " & strUsername & " " & strEncryPassword & " " & strRestart & " " & strJobId & " " & Chr(34) & strInstallPath & Chr(34) & " " & strOsName & " " & strHttps & " " & Chr(34) & strToPassPhraseFilePath & Chr(34) & " " & strDisableCodeSignatureVerification & " " & strSymCryptoKeyFilePath
        strCommandForPrint = strRemoteDirSpecial & "\" & strExeBaseName & " " & strCxIp & " " & strCxPort & " " & strUsername & " " & "**************" & " " & strRestart & " " & strJobId & " " & Chr(34) & strInstallPath & Chr(34) & " " & strOsName & " " & strHttps & " " & Chr(34) & strToPassPhraseFilePath & Chr(34) & " "
        WriteTextFile strDebugLog, " Push Command Line is : " & strCommandForPrint
        objNetwork.RemoveNetworkDrive strRemoteShare, TRUE, TRUE
        If Err <> 0 then
            WriteTextFile strDebugLog, "RemoteNetworkDrive to " & strRemoteShare & " returned " & Hex(Err.Number) & "(" & Err.Description & ")"
        End If
        intPID = CreateProcess(strCommand)' & strExtraCommand)
        If intPID <> -1 Then
      	    WriteTextFile strDebugLog, "Successfully created process on remote host with PID " & intPID
      	    WriteTextFile strDebugLog, "Going to start monitor script on this PID..."
            ExecMonitorScript strComputer, intPID, strTarget
		    WScript.Sleep 60000
		    WriteTextFile strDebugLog, "Monitoring the spawned PushClient.exe ..."
	    Else
	        WriteTextFile strDebugLog, "Unable to create the process " & strCommandForPrint & " " & strExtraCommand
		    HandleError Err, strComputer
		    WScript.Quit 1
        End If

    Else
        intPID = CreateProcess(strCommand & strExtraCommand)
        If intPID <> -1 Then
            ExecMonitorScript strComputer, intPID, strTarget	  
	        WScript.Sleep 60000
	        WriteTextFile strDebugLog, "Waited for some time and monitoring the spawned PushClient.exe..."
	    Else
	        WriteTextFile strDebugLog, "Unable to create the process " & strCommandForPrint & " " & strExtraCommand
	        HandleError Err, strComputer
	        WScript.Quit 1
        End If
    End If
End If

'******************************************************************************

Function FileCopy(strSourceFile, strTargetFolder)
'Copy executable or script to remote machine.
'If remote folder does not exist, creates it.
'Overwrites file if it exists in remote folder.

On Error Resume Next

Set objFSO = CreateObject("Scripting.FileSystemObject")
If Not objFSO.FileExists(strSourceFile) Then
  WriteTextFile strDebugLog, "Error: File " & strSourceFile & " not found."
  'WScript.Quit 4
End If

'If objFSO.FolderExists(strTargetFolder) Then
'  objFSO.DeleteFolder(strTargetFolder)
'End If

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
	'WScript.Quit 3
  End If
Else
  WriteTextFile strDebugLog, "Unable to create folder " & strTargetFolder & ". Error = " & Err.Number & "(" & Err.Description & ")"
  FileCopy = 1
  HandleError Err, strHost
  WScript.Quit 5
End If

End Function
 '******************************************************************************

Function CreateProcess(strCL)
'Create a process.

On Error Resume Next

Set objProcess = objSWbemServices.Get("Win32_Process")
intReturn = objProcess.Create _
 (strCL, Null, Null, intProcessID)
If intReturn = 0 Then
  WriteTextFile strDebugLog, "Process Created." & _
   vbCrLf & "Process ID: " & intProcessID
  CreateProcess = intProcessID
Else
  WriteTextFile strDebugLog, "Process could not be created." & _
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
