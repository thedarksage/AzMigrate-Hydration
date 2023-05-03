strComputer = "." 
Set objWMIService = GetObject("winmgmts:\\" & strComputer & "\root\CIMV2") 
Set colItems = objWMIService.ExecQuery( _
    "SELECT * FROM Win32_Service where Name like 'frsvc' or name like 'svagents' ",,48) 
Dim frsvcFound , svagentsFound 
frsvcFound = False
svagentsFound = False
For Each objItem in colItems  
   If objItem.Name = "frsvc" then
    Wscript.Echo "Found FX: " & objItem.Name   
    frsvcFound = True 
    Wscript.Echo "Found FX: " & frsvcFound     
    ElseIf objItem.Name = "svagents" then
     Wscript.Echo "Found FX: " & objItem.Name
     svagentsFound = True 
    Wscript.Echo "Found FX: " & svagentsFound 
    End If
    Next
    
Set filesys = CreateObject("Scripting.FileSystemObject")
Set filetxt = filesys.CreateTextFile("c:\windows\temp\agentoutput.txt", True)
getname = filesys.GetFileName("c:\windows\temp\agentoutput.txt")    
If  frsvcFound = True OR  svagentsFound = True Then
Wscript.Echo "Found FX: " & svagentsFound
Dim SysVarReg, Value, filesys, filetxt, getname, path
Set SysVarReg = WScript.CreateObject("WScript.Shell")
on error resume next
value = SysVarReg.RegRead ("HKLM\Software\SV Systems\ServerName")
Wscript.Echo "Error " & Err.Number
If Err.Number = 0 Then
If  frsvcFound = True and  svagentsFound = True then 
WScript.Echo "Value is= " +Value 
filetxt.WriteLine(Value + "=0")
filetxt.Close
ElseIf  frsvcFound = True and  svagentsFound = False then     
WScript.Echo "value is = " + Value 
filetxt.WriteLine(Value + "=2")
filetxt.Close
ElseIf  frsvcFound = False and  svagentsFound = True then     
WScript.Echo "value is = " + Value 
filetxt.WriteLine(Value + "=3")
filetxt.Close
End if
Else
Value = SysVarReg.RegRead("HKLM\SOFTWARE\Wow6432Node\SV Systems\ServerName")
If  frsvcFound = True and  svagentsFound = True then     
WScript.Echo "value is = " + Value 
filetxt.WriteLine(Value + "=0")
filetxt.Close
ElseIf  frsvcFound = True and  svagentsFound = False then     
WScript.Echo "value is = " + Value 
filetxt.WriteLine(Value + "=2")
filetxt.Close
ElseIf  frsvcFound = False and  svagentsFound = True then     
WScript.Echo "value is = " + Value 
filetxt.WriteLine(Value + "=3" )
filetxt.Close
End  If
End If
Else 
WScript.Echo "Agent Not installed"
filetxt.WriteLine("NotInstalled" )
filetxt.Close
End If