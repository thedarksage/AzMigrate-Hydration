'On Error Resume Next

strDebugLog = "vbs_monitor_process.log"

strComputer = WScript.Arguments(0)
intProcessID = WScript.Arguments(1)
strRemoteDir = WScript.Arguments(2)
strUsername = WScript.Arguments(3)
strPassword = WScript.Arguments(4)

'Check to make sure arguments are not empty.
If IsEmpty(strComputer) Or IsEmpty(intProcessID) Or IsEmpty(strRemoteDir) Then
  strArgError = _
   "Computer Name: " & strComputer & vbCrLf & _
   "  Process ID: " & intProcessID & vbCrLf & _
   "  Remote Directory: " & strRemoteDir & vbCrLf & _
   "  " & Now & vbCrLf & _
   "  ERROR: Computer name, process ID and remote directory not passed as arguments."
  WScript.Echo strArgError
  WriteTextFile strDebugLog, strArgError
  WScript.Quit 1
End If

'Connect to WMI.
Set objSWbemLocator = CreateObject("WbemScripting.SWbemLocator")
Set objWMIService = objSWbemLocator.ConnectServer(strComputer, "root\cimv2", strUsername,strPassword)
objWMIService.Security_.ImpersonationLevel = 3
objWMIService.Security_.authenticationLevel=pktPrivacy

If Err.Number <> 0 Then
  WriteTextFile strDebugLog, "Error while creating WMI connection to " & strComputer
  HandleError Err, strComputer
  WScript.Quit 1
End If

Set colMonitorProcess = objWMIService.ExecNotificationQuery _
 ("SELECT * FROM __InstanceDeletionEvent " & _ 
 "WITHIN 1 WHERE TargetInstance ISA 'Win32_Process' " & _
 "AND TargetInstance.ProcessId = '" & intProcessID & "'")
If Err.Number <> 0 Then
  WriteTextFile strDebugLog, "Query on __InstanceDeletionEvent failed..."
  HandleError Err, strComputer
  WScript.Quit 1
End If

WScript.Echo "Waiting for process to stop ..."
WriteTextFile strDebugLog, "Waiting for process to stop ..."

Set objLatestEvent = colMonitorProcess.NextEvent
strProcDeleted = Now
strProcCreated = _
 WMIDateToString(objLatestEvent.TargetInstance.CreationDate)
strProcessName = objLatestEvent.TargetInstance.Name
strCmdLine = objLatestEvent.TargetInstance.CommandLine
strPID = objLatestEvent.TargetInstance.ProcessId
strCSName = objLatestEvent.TargetInstance.CSName
intSecs = DateDiff("s", strProcCreated, strProcDeleted)
arrHMS = SecsToHours(intSecs)

strData = "Computer Name: " & strCSName & VbCrLf & _
 "  Process Name: " & strProcessName & VbCrLf & _
 "  Process ID: " & strPID & VbCrLf & _
 "  Time Created: " & strProcCreated & VbCrLf & _
 "  Time Deleted: " & strProcDeleted & VbCrLf & _
 "  Duration: " & arrHMS(2) & " hours, " & _
 arrHMS(1) & " minutes, " & arrHMS(0) & " seconds"
 
 Set objFSO = CreateObject("Scripting.FileSystemObject")
 If objFSO.FileExists(strRemoteDir & "\success.txt") Then
 	strData = strData & VbCrLf & _
 "  Exit Status: Success"
 Else
 	strData = strData & VbCrLf & _
 "  Exit Status: Failure"
 End If

WScript.Echo strData
WriteTextFile strDebugLog, strData
WScript.Echo "Data written to " & strDebugLog


'******************************************************************************

Function WMIDateToString(dtmDate)
'Convert WMI DATETIME format to US-style date string.

WMIDateToString = CDate(Mid(dtmDate, 5, 2) & "/" & _
                  Mid(dtmDate, 7, 2) & "/" & _
                  Left(dtmDate, 4) & " " & _
                  Mid(dtmDate, 9, 2) & ":" & _
                  Mid(dtmDate, 11, 2) & ":" & _
                  Mid(dtmDate, 13, 2))

End Function

'******************************************************************************

Function SecsToHours(intTotalSecs)
'Convert time in seconds to hours, minutes, seconds and return in array.

intHours = intTotalSecs \ 3600
intMinutes = (intTotalSecs Mod 3600) \ 60
intSeconds = intTotalSecs Mod 60

SecsToHours = Array(intSeconds, intMinutes, intHours)

End Function

'******************************************************************************

'Handle errors.
Sub HandleError(Err, strHost)

strError = "Computer Name: " & strHost & VbCrLf & _
 "ERROR " & Err.Number & VbCrLf & _
 "Description: " & Err.Description & VbCrLf & _
 "Source: " & Err.Source
WScript.Echo strError
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

'On Error Resume Next

Const FOR_APPENDING = 8

'Open text file for output.
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
