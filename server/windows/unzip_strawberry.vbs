FileToGetUnZipped = "C:\Temp\strawberry-perl-5.8.8.4.zip"
DestPathForUnzippedFile = "C:\strawberry"

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