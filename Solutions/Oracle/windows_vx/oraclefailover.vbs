Dim objFileSystem, objInputFile,dumpdirectory,objDumpDirectory
Dim strInputFile, inputData, strData,inputdata1
Dim Drives,Tag,ArgCount,command,value,OracleServiceName
Dim strFirst,ShellObj,ServiceEnv,strComputer,objComputer,strService,OraSid
Dim ServiceStatus

Drives=""
ServiceStatus=0

'Taking Tag name as Arguement to rollback to particular consistent point

ArgCount=WScript.Arguments.Count

	If (ArgCount <> 2) Then
	
		WScript.Echo ("Arguement count is less than one ")
		WScript.Echo  ("Exiting...")
		WScript.Quit (1)
	
	End If
	
Tag=WScript.Arguments(0)
OraSid=WScript.Arguments(1)
Const OPEN_FILE_FOR_READING = 1




' generate a filename base on the script name, here readfile.in
strOutputFile = ".\failover\data\drivelist.txt"


Set objFileSystem = CreateObject("Scripting.fileSystemObject")


	If  Not (objFileSystem.FileExists(strOutputFile)) Then
	
		WScript.Echo "drivelist.txt does not exist in <INSTALLATION-Directory>\failover\data Directory"
		WScript.Echo "Make sure you have replicated \failover\data directory from source machine"
		WScript.Echo "Exiting...."
		WScript.Quit(1)
	
	End If
	

Set objInputFile = objFileSystem.OpenTextFile(strOutputFile,OPEN_FILE_FOR_READING)
Set objDumpDirectory = 	objFileSystem.OpenTextFile("c:\temp\dumpdirectory",OPEN_FILE_FOR_READING)
	
Set ShellObj=CreateObject("Wscript.Shell")
' read everything in an array
inputData = Split(objInputFile.ReadAll, vbNewLine)

	


'Stopping Vxagent Service before Roll back of Volumes

ServiceOperation "svagents","status"
If ServiceStatus = 1 Then

	ServiceOperation "svagents","stop"
End If


'Read one by one element and removing white spaces and preparing cdpcli command
For Each strData In inputData
         If strData <> "" Then
           	strFirst=Split(strData," ")
			Drives=strFirst(0)
			command="cdpcli --rollback --dest=" + Drives  + " --force=yes --event=" + Tag 
			Set value=ShellObj.Exec(command)
			WScript.Echo value.StdOut.ReadAll
			WScript.Echo value.StdErr.ReadAll
	    End If
Next
'Starting svagents services...
ServiceOperation "svagents","start"	

'Creating necessary directories...bdump/dump/udump..etc
WScript.Echo "Creating necessary directories..."
Do While Not objDumpDirectory.AtEndOfStream
	inputdata1 = Split(objDumpDirectory.Readline , vbNewLine)
   

	For Each strData In inputdata1
		WScript.Echo strData
		
		If strData <> "" Then
		   strFirst=Split(strData,"'")
		   dumpdirectory=strFirst(1)
		   'WScript.Echo dumpdirectory
		   command="cmd /k "+ "mkdir " + dumpdirectory + "& exit"
		   'WScript.Echo command
		   ShellObj.Run(command)
		   
		End If		

	Next
Loop

WScript.Echo "creation of directories...done"


	

'Stopping Oracle Services
OracleServiceName="OracleService" + OraSid

ServiceOperation OracleServiceName,"status"
If ServiceStatus = 1 Then

	ServiceOperation OracleServiceName,"stop"
	
End If

ServiceOperation OracleServiceName,"start"


WScript.Quit(0)





		
Sub ServiceOperation (strService,Operation)
	
		Set ServiceEnv=ShellObj.Environment("PROCESS")
		strComputer=ServiceEnv("COMPUTERNAME")		
		Set objComputer = GetObject("WinNT://" & strComputer & ",computer")
		Set objService  = objComputer.GetObject("Service",strService)
		' define ADSI status constants
		Const ADS_SERVICE_STOPPED          = 1
		Const ADS_SERVICE_START_PENDING    = 2
		Const ADS_SERVICE_STOP_PENDING     = 3
		Const ADS_SERVICE_RUNNING          = 4
		Const ADS_SERVICE_CONTINUE_PENDING = 5
		Const ADS_SERVICE_PAUSE_PENDING    = 6
		Const ADS_SERVICE_PAUSED           = 7
		Const ADS_SERVICE_ERROR            = 8
		ServiceStatus=0
		If Operation = "start" Then
		
		
			If objService.status = ADS_SERVICE_STOPPED Then
			
				WScript.Echo "Starting "+ strService + " Services...."
				objService.start
				WScript.Sleep(10000)
				Do While objService.Status <> 4
					WScript.Sleep(1000)
				
				Loop
				
				WScript.Echo strService + " Started..."
						
			ElseIf objService.status = ADS_SERVICE_RUNNING Then
				WScript.Echo "Stopping " + strService + " Services....."
				objService.stop
				WScript.Sleep(50000)
				WScript.Echo "Starting " + strService +" Services...."
				objService.start
				WScript.Sleep(10000)
				Do While objService.Status <> 4
					WScript.Sleep(1000)
				
				Loop
				
				WScript.Echo strService + "Service Started..."
	
			End If
		ElseIf Operation = "stop" Then
				
				WScript.Echo "Stopping " + strService + " Services....."
				objService.stop
				WScript.Sleep(100000)
				Do While objService.Status <> 1
					WScript.Sleep(1000)
				
				Loop
				
				WScript.Echo strService + " Service Stopped..."
		ElseIf Operation = "status" Then
				
				If  objService.status = 4 Then
				
					ServiceStatus=1
					
				End If
				
			
		End If
		
						
	End Sub