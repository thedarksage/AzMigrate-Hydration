FileToGetUnZipped = "C:\Temp\Support_UI_Files.zip"
DestPathForUnzippedFile = "C:\home\svsystems\system"

Set objFSO = CreateObject("Scripting.FileSystemObject")

If Not objFSO.FolderExists(DestPathForUnzippedFile) Then
    objFSO.CreateFolder(DestPathForUnzippedFile)
End If

UnZipFile FileToGetUnZipped, DestPathForUnzippedFile

Sub UnZipFile(strArchive, DestPathForUnzippedFile)
    Set objApp = CreateObject( "Shell.Application" )

    Set objArchive = objApp.NameSpace(strArchive).Items()
    Set objDest = objApp.NameSpace(DestPathForUnzippedFile)

    objDest.CopyHere objArchive,4
End Sub