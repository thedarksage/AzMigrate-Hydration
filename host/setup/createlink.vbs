set objWSHShell = CreateObject("WScript.Shell")

sComponent = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(0))

If sComponent = "VCON" Then
	' command line arguments
	' TODO: error checking
	sShortcut = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(1))
	sTargetPath = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(2))
	sIcon = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(3))

	set objSC = objWSHShell.CreateShortcut(sShortcut) 

	objSC.TargetPath = sTargetPath
	objSC.IconLocation = sIcon
	objsc.WindowStyle = 7
	objSC.Save

ElseIf sComponent = "UA" Then
	' command line arguments
	' TODO: error checking
	sShortcut = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(1))
	sTargetPath = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(2))

	set objSC = objWSHShell.CreateShortcut(sShortcut) 

	objSC.TargetPath = sTargetPath
	objsc.WindowStyle = 1
	objSC.Save
ElseIf sComponent = "CX" Then
	' command line arguments
	' TODO: error checking
	sShortcut = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(1))
	sTargetPath = objWSHShell.ExpandEnvironmentStrings(WScript.Arguments.Item(2))

	set objSC = objWSHShell.CreateShortcut(sShortcut) 

	objSC.TargetPath = sTargetPath
	objsc.WindowStyle = 1
	objSC.Save
End If