On Error Resume Next 
Dim fso, folder, files, NewsFile,sFolder,payload,payload1
Set fso = CreateObject("Scripting.FileSystemObject")
Set subfso = CreateObject("Scripting.FileSystemObject")

Dim fsotemp
Set fsotemp = CreateObject("Scripting.FileSystemObject")
Dim tempfolder
Const TemporaryFolder = 0
Set tempfolder = fsotemp.GetSpecialFolder(TemporaryFolder)


sFolder = Wscript.Arguments.Item(0)
If sFolder = "" Then      
	Wscript.Echo "No Folder parameter was passed"      
	Wscript.Quit  
End If  



Set manifestfile=fso.CreateTextFile(sFolder&"\\..\\manifest.xml",true)
payload="<?xml version=""1.0""?>"
manifestfile.WriteLine(payload)
payload="<BoxCreate xmlns=""http://windowsmarketplace.com/sdk/schemas/boxcreatemanifest.xsd"">"
manifestfile.WriteLine(payload)
payload="<Caption>Extracting Files</Caption>"
manifestfile.WriteLine(payload)
payload="<ExecuteFile>UnifiedSetup.exe</ExecuteFile>"
manifestfile.WriteLine(payload)
payload="<Container>"
manifestfile.WriteLine(payload)

Set folder = fso.GetFolder(sFolder)

Set files = folder.Files
payload= "<Payload Compressed=""true"">"
manifestfile.WriteLine(payload)
For each fileIdx In files
    Payload = "<CompressFile>" & folder.Name & "\" & fileIdx.Name & "</CompressFile>"
    manifestfile.WriteLine(payload)
Next
payload= "</Payload>"
manifestfile.WriteLine(payload)

Set subFolders = folder.SubFolders
For each subfolderIdx In folder.SubFolders
    payload= "<Payload ExtractRelativePath = """ & subfolderIdx.Name & "\"" Compressed=""true"">"
    manifestfile.WriteLine(payload)
    Set subFolder=subfso.GetFolder(subfolderIdx)
    Set subFolderFiles= subFolder.Files
    For each subfolderFileIdx In subFolderFiles
        payload="<CompressFile>" & folder.Name & "\" & subfolderIdx.Name & "\" & subfolderFileIdx.Name & "</CompressFile>"
        manifestfile.WriteLine(payload)
    Next
    payload= "</Payload>"
    manifestfile.WriteLine(payload)
Next 
payload="</Container>"
manifestfile.WriteLine(payload)
payload="</BoxCreate>"
manifestfile.WriteLine(payload)

manifestfile.Close