FileToGetUnZipped = "C:\Temp\php-5.4.27-nts-Win32-VC9-x86.zip"
DestPathForUnzippedFile = "C:\thirdparty\php5nts"

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