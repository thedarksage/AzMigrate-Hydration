On Error Resume Next
'Option Explicit

                Dim oFSO
                Dim sDirectoryPath
                Dim oFolder
                Dim oDelFolder
                Dim oFileCollection
                Dim oFile
                Dim oFolderCollection

Set objArgs = WScript.Arguments

strOriginalString = objArgs(0)
strStringToFind = "C:"

boolCaseSensitive = True

If boolCaseSensitive = True Then
	intPos = InStr(strOriginalString, strStringToFind)
Else
	intPos = InStr(LCase(strOriginalString), LCase(strStringToFind))
End If

If intPos > 0 Then

Else
		strOriginalString = Replace(strOriginalString,"home","")
                Set oFSO = CreateObject("Scripting.FileSystemObject")
                set oFolder = oFSO.GetFolder(strOriginalString)
                set oFolderCollection = oFolder.SubFolders
                set oFileCollection = oFolder.Files

                For each oFile in oFileCollection
                               oFile.Delete(True)
                Next

                For each oDelFolder in oFolderCollection
                                oDelFolder.Delete(True)
                Next
'Clean up

                Set oFSO = Nothing
                Set oFolder = Nothing
                Set oFileCollection = Nothing
                Set oFile = Nothing         
End If