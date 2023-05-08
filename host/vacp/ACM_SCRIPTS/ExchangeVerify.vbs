'*============================================================================+
'*============================================================================+
'*		Exchange Integrity Verifiction Script                                 '
'*		Version 1.1.7 Beta	                                                  '
'*		Sept 14, 2007                                                         '
'*		(c) Copyright InMage Systems Inc,2006                                 '
'*============================================================================+
'*  Purpose: The basic objective for this script is to verify				  '
'*           Exchange Integrity using Scout Virtual Snapshot Features      '
'*============================================================================+
'* Version History                                                            '
'* 1.0  Alpha Preview Release - Aug 04,2006									  '
'* 1.0.1 Alpha - Aug 08,2006                                                  '
'*	   a) Ability to use Custom File Names for debug/Report Files	          '
'*     b) Print of status file based on the result of execution               '
'*     c) returning of error code on runtime/integrity failure cases          '
'* 1.0.2 Alpha - Aug 09,2006                                                  '
'*     a) Support for 3.5.1 Mode (Last Exchange Consistent Point)			  '
'*     b) Support for taking parameters from command line					  '
'* 1.0.3 Beta - Aug 12,2006													  '	
'* 	   a) Change the implementation based on Review Session	     			  '		 
'*     b) Use log range from *.edb instead of checkpoint from *.chk file      '
'*	   c) Verification logic is tuned for one *.edb file instead of DBVolume  '
'*     d) Handling of ranges in Hex values  					  			  '
'*     e) Addition of Seq Number 										      '
'*     f) Using VSNAP defaults as mount points instead of volumes		      '	
'*     g) No retention db required as per 3.5.1 implementation			      '
'* 1.0.4 Beta - Aug 16,2006													  '	
'*     a) Better handling of unmount cases 								      '
'* 1.0.5 Beta - Aug 19,2006													  '	
'*     b) Fixed a parsing bug for targetVolumeLog							  '
'* 1.0.6 Beta - Aug 23,2006													  '	
'*     a) Fixed a Interfacing bug with batch file,Had to add                  '
'*        another parameter exchangeLogFilePath                               '
'*	   b) Minor Default value changes and grammer correction				  '
'*     c) Program Exits with a failure if Log/DB Consistency Events don't     '
'*        match                                                               '
'*     d) Report now gets printed in /seq directory instead of parent 		  '           
'* 		  directory and is no per consistency point                           '		
'* 1.0.7 Beta - Aug 24,2006													  '	
'*     a) MountPoints directory will now be in Application Data Directory     '
'*     b) Program exists gracefully, if the current event tag is not the same '
'*        as previous tag - some gaurd against some conditions  			  '
'*	   c) Status file shall include event tag instead of recommendation		  '	
'* 1.0.8 Beta - Aug 27,2006													  '	
'*     a) Program Exists, if the current consistency event is same as previous'
'*        consistency event.                                                  '
'*     b) Creating and Maintaining Global Tracker.csv comprising of exchange  '
'*        event verification status                                           '
'* 1.0.9 Beta - Aug 28,2006													  '	
'*     a) Tracker & Status files are generated in system32 folder,            '
'*        changed it to script Directory                                      '
'* 1.1.0 Beta - Aug 30,2006													  '	
'*     a) Fixed an corner case, where previous consistency check is not       '
'*        Working as desired												  '
'*     b) "filenum" parameter changed to "file-num"							  '
'*	   c) Deleting the status file, before starting, it is a bug			  '
'*	   d) avoided the failure cases due the same previous consistent points   '
'* 1.1.1 Beta - Aug 30,2006													  '	
'*     a) Fixed an issues as results of Exchange SP2 Patch					  '
'* 1.1.2 Beta - Dec 27,2006													  '	
'*     a) Made it complaint for 3.5.1 & 3.5.2								  '
'* 1.1.3 Beta - Mar 08,2007													  '	
'*     a) Discovery of the InMage's CDPCLI Path								  '
'* 1.1.4 Beta - June 21,2007
'*	   a) Hack or Fix for CDP Retention Log Problem for 4.1.0 Release         '   	
'* 1.1.5 Beta - June 26,2007
'*	   a) Mountpoint support provided. Previously only drive letter was       '
'*        considered for target database & log/vsnap volumes.				  '
'* 1.1.6 Beta - September 14, 2007											  '
'*	   a) Changed the registry key to be read to get Exchange 2007 bin path	  '
'*		  This script, when run from FX templates, is not able to read 64-bit '
'*		  registry view as FX is a 32 bit application.						  '
'* 1.1.7 Beta - October 12, 2007                      						  '
'*	   a) In case of Exchange 2007, "ExchangeIS" will be the parameter to     '
'*		  option "--app" in cdpcli command. The parameter will remain to be   '
'*		  "Exchange" in case of Exchange 2003.								  '
'* 1.2 Beta - March 26, 2010                        						  '
'*     a) Now uses the most recent common application consistency point across'
'*        all database/log volumes, instead of picking up latest event for    '
'*        each database                                                       '
'*============================================================================*
'* USER DEFINED SETTING FOR EXCHANGE VERIFICATION                    		  |
'* Note: Please fill up the settings for your exchange setup                  |
'*============================================================================*
'*                                                                            '
'*Usage :-                                                                    ' 
'*                                                                            '
'*  Binary Paths                                                              '
'*	------------                                                              '
'*	cdpCLIPath		  = Absolute path of InMage CDPCLI utility                '
'*	exchangeBinPath   = Folder Path of Exchange Bin directory Or Path of      '
'*					    Eseutil	                                              '
'*				                                                              '
'*	Replication Settings                                                      '
'*	--------------------                                                      '
'*	eventTag		 = Consistency Event                                      '
'*	eventNum		 = 1 (Event Sequence Number)                              '
'*  vsnapMode		 = 0 ' 0 - Event Supplied , 1 - Last Application Event Tag'	
'*	retentionlogDB   = Complete Retention Log Path For Exchange DB Volume     '
'*	targetVolumeDB   = Exchange DB Target Volume                              ' 
'*	vsnapVolumeDB    = Snapshot Volume For Exchange DB                        '
'*				                                                              '
'*	retentionlogLOG  = Complete Retention Log Path For Exchange LOG Volume    ' 
'*	targetVolumeLOG  = Exchange LOG Target Volume                             '
'*	vsnapVolumeLOG   = Snapshot Volume For Exchange LOG                       ' 
'*	seq				 = sequenceNumber                                         '			
'*============================================================================*

'*============================================================================*
'							Function Main                                     '
'	                    	ENTRY POINT                                       '
'X============================================================================X

 ' Global Program Settings
   Option Explicit	

 ' Program Control Options
  
 ' Debug Related Settings
   Dim debugMode,debuglogFile,reportFile 
   debugMode = 2  '0 - No Debugging, 1 - Errors Only, 2 - Errors & Debug
   Dim trackerFile: trackerFile="Tracker.csv"
 ' Optional - Provide an absolute path, if you require the files to be in someother path.
   debuglogFile = "debug.log"
   reportFile = "report.txt"

 ' Global FileSystem Object   
   Dim objFSO
   Set objFSO  = WScript.CreateObject("Scripting.FileSystemObject")
	
 ' Change this values during the customization procedure

  Dim retentionlogLOG,retentionlogDB,targetVolumeDB,chkFilePrefix
  Dim vsnapVolumeDB,targetVolumeLOG,vsnapVolumeLOG
  Dim eventTag,eventNum,vsnapMode
  Dim exchangeBinPath, exchangeDBPath,exchangeRelativeDBPath,exchangeLogPath
  Dim exchangeDBEventTag,exchangeLOGEventTag
  Dim exchangeRelativeLogPath 
  Dim exchangeDBFilePath,exchangeLogFilePath
  Dim cdpCLIPath
  Dim exchangeEseutilPath
  Dim seq: seq=000001	 
  'Variable for multiple invocation tracking.
  Dim filenum,fileid,totalfileCount
  
  
'Agent Related Settings
 cdpCLIPath="C:\Program Files\InMage Systems\cdpcli"

'Exchange Path Settings
 exchangeBinPath="C:\Program Files\Exchsrvr\bin"

'Tag Settings	
 'eventTag="exchange44d33a54"
 'eventNum=1
 vsnapMode=1
   
 'Exchange LOG Settings
 
  targetVolumeDB=""
 
 'Exchange LOG Settings
  targetVolumeLOG=""
 
 'Optional [Don't touch unless required]
 
 'Derived Exchange Paths during Verification
  exchangeEseutilPath=exchangeBinPath
  exchangeRelativeLogPath="" 'Optional - Define if the log files are not installed on root folder
  exchangeRelativeDBPath=""  'Optional - Define if the db files are not installed on root folder
  exchangeDBPath= vsnapVolumeDB & "\" &  exchangeRelativeDBPath
  exchangeLogPath=vsnapVolumeLOG & "\" &  exchangeRelativeLOGPath	
  
  
  chkFilePrefix="E00"
   
 'Status Files Naming Convention
  Dim successFile,failedFile 
  successFile="Success.status" 
  failedFile = "Failed.status" 
 
 'Program Control Decision Variables
  Dim mountDBOK: mountDBOK=False 
  Dim mountLOGOK:mountLOGOK=False
  Dim dbOk,logOk,unmountOk,mountOk
  Dim overallOk
  Dim usingMountPointsDB,usingMountPointsLOG
  Dim vsnapMounts
  Dim pathForMounting,prevConsistencyFilePattern,currentConsistencyFilePath
  Dim checkRangeFrom,checkRangeTo,checkRangeFileCount
  Dim prevConsistencyStatus: prevConsistencyStatus = True
  
  'Global return Codes
  Dim returnCode:returnCode=0
  Dim EXCHANGE_VERIFICATION_SUCCESS:EXCHANGE_VERIFICATION_SUCCESS=0
  Dim VSNAP_MOUNT_FAILURE:VSNAP_MOUNT_FAILURE=2
  Dim VSNAP_UNMOUNT_FAILURE:VSNAP_UNMOUNT_FAILURE=3
  Dim EVENT_TAG_MISMATCH_FAILURE:EVENT_TAG_MISMATCH_FAILURE=4  
  Dim EVENT_TAG_SAME:EVENT_TAG_SAME=5  
  
  Dim SCRIPT_RUNTIME_FAILURE:SCRIPT_RUNTIME_FAILURE=6  
  
  Dim SCRIPT_PREREQUSITE_FAILURE:SCRIPT_PREREQUSITE_FAILURE=7
  
  Dim EXCHANGE_DB_VERIFY_FAILED:EXCHANGE_DB_VERIFY_FAILED=10
  Dim EXCHANGE_LOG_VERIFY_FAILED:EXCHANGE_LOG_VERIFY_FAILED=11
  Dim EXCHANGE_DB_AND_LOG_VERIFY_FAILED:EXCHANGE_DB_AND_LOG_VERIFY_FAILED=12

'+----------------------------------------------------------------------------+
'   Program Entry Point                                                       |	
'+----------------------------------------------------------------------------+
     'test()  'Test Procedures
     main()  'Main Logic

'+----------------------------------------------------------------------------+
'   Main Procedures                                                           |	
'+----------------------------------------------------------------------------+
Sub main
	'Declartion
	 'returnCode=0
	 Dim stepResult,msg
     Dim starttime,endtime 
     starttime = Timer()
   	 'Parse Command Line arguments, before proceeding any further
   	  processCommandLineArguments
         
         
     'Create Working Folders
	  createWorkingFolders()
  	
	'Initialize file names
	  successFile= seq & "_" & successFile
      failedFile = seq & "_" & failedFile
      Dim pathPrefix: pathPrefix =  pathForMounting & seq & "_" &  fileid & "_" 
	  debuglogFile = pathPrefix & debuglogFile
	  reportFile = pathPrefix & eventTag & "_" & reportFile
	
	 'Print Report Headers	 
	 debug ("================================================")
	 debug ("Exchange Log Verification Process Started .....")
	 debug (" By InMage ExchangeVerifier On " & Now())
	 debug ("================================================")
	 
	 'reportFile = seq &  "-" & fileid & "-" & eventTag & "-" & reportFile
	  printHeader()	
      msg ="File Id # : " & fileid & " will be processed. Total Files to be processed: "  & totalfileCount 
      Debug(msg)
         	  
   
   	 'Print Working & Mount point directory informtion
   	  Debug("Working Directory : " & vsnapMounts)
   	      
     'Delete existing status files
      deleteStatusFile()

	
	'Process Logic
	exchangeDBEventTag  = eventTag
	exchangeLOGEventTag = eventTag
 
	  
	  'Check, if the log volumes point to the same consistent point
	If returnCode = 0 Then 
		   prevConsistencyFilePattern = seq & "_" &  fileid &  "_" & "*prev_consistency"
		   currentConsistencyFilePath = pathPrefix &  "prev_consistency"
		 If exchangeLOGEventTag <> exchangeDBEventTag Then
		    msg="Event Tags for Exchange DB and Log volumes are different: DB Eventtag: " &  exchangeDBEventTag & " LOG Eventtag:" &  exchangeLOGEventTag 
	        returnCode = EVENT_TAG_MISMATCH_FAILURE
		 End If
	End If
	If returnCode = 0 Then 
		 eventTag = exchangeLOGEventTag
		 'Check,if the previous/current consistent points are same, if they are the
		 'Same, quit the program as we don't want to create false positives
		 If checkIfCurrentEventIsSameAsPreviousEvent() = True Then
		    msg="Previous and Current Event Tags for verification are the same, No Point processing any further"
	        returnCode = EVENT_TAG_SAME
		 End If 
	End If  
	 
	'Verify the Data Files
	If returnCode = 0 Then 		
		 dbOk=verifyChecksumForDBFiles()
		 msg = "Exchange DB Integrity : " & dbOk
		 debug(msg)
		 print(msg)
		
		'Verify LogRange
		 logOk=verifyExchangeLogRange()
		 msg = "Exchange Log Integrity : " & logOk
		 debug(msg)
		 print(msg)
		
		'Overall Consistency
		 overallOk = dbOk And logOk
		
		'Make Conclusions and print out summary in Verification Report.
	    
		 msg = "Recommendation:" & vbcrlf & _
		       "---------------"
		 debug(msg)
		 print(msg)
		
		 If overallOK Then
		   msg="Exchange backup image for event [" & eventTag & "] is consistent, recommended image for a backup"
	    Else
	      msg="Exchange backup image for event [" & eventTag & "] has problems, not recommended for backup." & vbcrlf & _
		        "Alternatively, you can perform a repair operation on existing image"
	    End If		
		debug(msg)
		
		Dim status_output
		status_output = "Sequence: " & seq & vbCrLf & _
		                "Event Tag: " & eventTag & vbCrLf & _
		                "DB File: " & exchangeDBFilePath & vbCrLf & _
		                "Log Path: " & exchangeLogFilePath & vbCrLf & _
		                "Remarks: " & msg
	 
		print(status_output)
		
		'Close down Reports & Debug Logs
	    printFooter()

	    'Write Tracker Record
	    Dim trackerFilePath:trackerFilePath=ScriptFolderPath() & trackerFile
	    writeTrackRecord(trackerFilePath)
   

	   'Raise pre-defined ERROR_CODE(if any) for the caller batch program to handle it in a graceful manner.
	   'Alternatively, the code snippet also writes a status file to be used by other programs in the sequence
	   'Todo: Write Code to post the status to CDPCLI/CX Server the verification status of the consistent point.
		
	      
	   If overallOK = False Then
	     If dbOk=False And logOk=False Then
	         returnCode=EXCHANGE_DB_AND_LOG_VERIFY_FAILED
	         msg = "Both Exchange DB and LOG files have failed integrity test, please refer to detail report"
	      ElseIf dbOk = False Then
	         returnCode=EXCHANGE_DB_VERIFY_FAILED
	         msg = "Exchange DB files have failed integrity test, please refer to detail report"
	      ElseIf logOk = False Then
	         returnCode=EXCHANGE_LOG_VERIFY_FAILED
	         msg = "Exchange LOG files have failed integrity test, please refer to detail report"
	      End If
	   End If
  End If


	'Teardown Operations: Unmounting Virtual Volumes
	'teardown()
	
	
	'Compute Status and write relevant files
	 If returnCode > 0 Then 
       processStatus False,msg 
     Else
       processStatus True,msg 
     End If  
  
	 
   Debug ("---------------")
   Debug ("Exchange Log Verification Process Ended At " & Now() & " .....")
   Debug("Return Code: " & returnCode)
   endtime = Timer()
   Debug("Time Taken :" & (endtime-starttime) & " Seconds")
   debug( "Message: " & msg)
   Debug ("============  End of Debug Log   ============")
	
  If returnCode > 0 Then 
     'Err.Raise returnCode,"ExchangeIntegrityVerifier",msg
     WScript.Echo("ExchangeIntegrityVerifier : " & msg)
     WScript.Quit(returnCode)
  End If 

End Sub


'+----------------------------------------------------------------------------+
' Sub Routine to Process Command Line Arguments	   		      	              | 
'+----------------------------------------------------------------------------+

Sub processCommandLineArguments
	Dim args,num
	Set args = WScript.Arguments
	num = args.Count
	' Parameters are all optional, if they are specified they shall overwrite the 
	' Default values specified in the script.
	If num > 0 Then
	    'Case 1: Check if the first command is a help command
	   
	    If args(0) = "help" Or args(0) = "?" Or args(0) = "\?" Then
	    	'Display the usage
	    	printUsage
	    	WScript.Quit 1
	    Else
			'Parse Command Line arguments
			setCommandLineArguments(args)	    
	    End If
  	End If
  	'Parse file num
  	 
     Dim fileAry: fileAry = Split(filenum,"-")
     fileid=fileAry(0)
     totalfileCount=fileAry(1)
  	
End Sub

'+----------------------------------------------------------------------------+
' Sub to print usage         					   		      	              | 
'+----------------------------------------------------------------------------+
Sub printUsage
   	Dim usage: usage = "Usage: [CScript] ExchangeVerify.vbs" & vbCrLf & _
   		        "/cdpCLIPath:<Absolute path of InMage CDPCLI utility>" & vbCrLf & _
   		        "/exchangeBinPath:<Folder Path of Exchange Bin directory Or Path of Eseutil>" & vbCrLf & _
   		        "/eventTag:<Consistency Event>" & vbCrLf & _
   		        "/vsnapMode:<0 ' 0 - Event Supplied , 1 - Last Application Event Tag>" & vbCrLf & _
   				"/retentionlogDB:<Complete Retention Log Path For Exchange DB Volume>" & vbCrLf & _
   				"/targetVolumeDB:<Exchange DB Target Volume>" & vbCrLf & _
   				"/exchangeDBFilePath:<Exchange DBFile Name>" & vbCrLf & _
   				"/vsnapVolumeDB:<Snapshot Volume For Exchange DB>" & vbCrLf & _
   				"/retentionlogLOG:<Complete Retention Log Path For Exchange LOG Volume>" & vbCrLf & _
   				"/targetVolumeLOG:<Exchange LOG Target Volume>" & vbCrLf & _
   				"/exchangeLogPath:<Exchange Folder Path where Logs are Stored>"  & vbCrLf & _
   				"/exchangeLogFilePath:<Exchange Folder Path where Logs are Stored>"  & vbCrLf & _
   				"/vsnapVolumeLOG:<Snapshot Volume For Exchange LOG>" & vbCrLf & _
   				"/debugMode:<'0 - No Debugging, 1 - Errors Only, 2 - Errors & Debug>" & vbCrLf & _
   				"/seq:<sequencenumber>" & vbCrLf & _
   				"/filenum:<file num of total files>" & vbCrLf & _ 
   				"/chkFilePrefix:<chkFilePrefix>" & vbCrLf & _ 
   				"/logSeqNum:<Sequence number of log volume>" & vbCrLf & _ 
   				"/dbSeqNum:<Sequence number of database volume>" & vbCrLf & _ 
   				"/sourceServer:<sourceExchangeServer>"
   				   				
	WScript.Echo usage
End Sub

'+----------------------------------------------------------------------------+
' Sets the command line arguments to global parameters	      	              | 
'+----------------------------------------------------------------------------+
Sub setCommandLineArguments(args)

   				
' Only Support Named Arguments
  If args.Named.Exists("cdpCLIPath") Then
    cdpCLIPath = args.Named.Item("cdpCLIPath")
  End If 
 
  If args.Named.Exists("exchangeBinPath") Then
    exchangeBinPath = args.Named.Item("exchangeBinPath")
  End If 
 
  If args.Named.Exists("eventTag") Then
    eventTag = args.Named.Item("eventTag")
  End If 
  
  If args.Named.Exists("vsnapMode") Then
    vsnapMode = args.Named.Item("vsnapMode")
  End If 
  
  If args.Named.Exists("retentionlogDB") Then
    retentionlogDB = args.Named.Item("retentionlogDB")
    retentionlogDB = Replace(retentionlogDB,"/","\")
  End If 
  
  If args.Named.Exists("targetVolumeDB") Then
    targetVolumeDB = args.Named.Item("targetVolumeDB")
  End If

  If Len(targetVolumeDB) = 1 Then
	targetVolumeDB = targetVolumeDB & ":"
  End If 

  If args.Named.Exists("exchangeDBFilePath") Then
    exchangeDBFilePath = args.Named.Item("exchangeDBFilePath")
    
  End If 

  If args.Named.Exists("vsnapVolumeDB") Then
    vsnapVolumeDB = args.Named.Item("vsnapVolumeDB")
  End If 
  
  If args.Named.Exists("retentionlogLOG") Then
    retentionlogLOG = args.Named.Item("retentionlogLOG")
    retentionlogLOG = Replace(retentionlogLOG,"/","\")
  End If 
  
  If args.Named.Exists("targetVolumeLOG") Then
    targetVolumeLOG = args.Named.Item("targetVolumeLOG")
  End If 

  If Len(targetVolumeLOG) = 1 Then
	targetVolumeLOG = targetVolumeLOG & ":"
  End If 
  
  If args.Named.Exists("exchangeLogPath") Then
    exchangeLogPath= args.Named.Item("exchangeLogPath")
  End If 
    
  'Get Drive Letter for the folder - Commented below line because of the bug. Ideally this should be in above line
  'targetVolumeLOG = DriveLetterForFolderPath(exchangeLogPath)  
  
  If args.Named.Exists("exchangeLogFilePath") Then
     exchangeLogFilePath= args.Named.Item("exchangeLogFilePath")
  End If
  
  If args.Named.Exists("vsnapVolumeLOG") Then
    vsnapVolumeLOG = args.Named.Item("vsnapVolumeLOG")
  End If 
  
  If args.Named.Exists("debugMode") Then
   debugMode = args.Named.Item("debugMode")
  End If 
  
  If args.Named.Exists("exchangeBinPath") Then
    exchangeBinPath= args.Named.Item("exchangeBinPath")
    exchangeEseutilPath=exchangeBinPath
  End If
    
  If args.Named.Exists("seq") Then
    seq = args.Named.Item("seq")
  End If
   
  If args.Named.Exists("file-num") Then
    filenum = args.Named.Item("file-num")
  End If
    
  If args.Named.Exists("chkFilePrefix") Then
    chkFilePrefix = args.Named.Item("chkFilePrefix")
  End If
  
  If args.Named.Exists("dbSeqNum") Then
    dbSeqNum = args.Named.Item("dbSeqNum")
  End If
  
  If args.Named.Exists("logSeqNum") Then
    logSeqNum = args.Named.Item("logSeqNum")
  End If  
  
End Sub



'+----------------------------------------------------------------------------+
' Create Base Directories for Mountpoints  					                  | 
'+----------------------------------------------------------------------------+

Sub createWorkingFolders
   
    'Discovery Working Directories
     Discovery
    'Initialize mount point folders 
    Dim tkenAry
    Dim workingDirectoryName:workingDirectoryName="ExchangeConsistency"
    tkenAry = Split(ScriptFolderPath,"\")
  
    If UBound(tkenAry) > 1 Then
     	tkenAry(UBound(tkenAry)-1) = "Application Data"
    	vsnapMounts = Join(tkenAry,"\")
    	If objFSO.FolderExists(vsnapMounts) = False Then
    	   tkenAry = Split(ScriptFolderPath,"\")
    	   tkenAry(UBound(tkenAry)-1) = workingDirectoryName
    	   vsnapMounts = Join(tkenAry,"\")
    	Else
    	   tkenAry = Split(ScriptFolderPath,"\")
    	   tkenAry(UBound(tkenAry)-1) = "Application Data\" & workingDirectoryName
    	   vsnapMounts = Join(tkenAry,"\")
    	End If  
   Else
   	    tkenAry(UBound(tkenAry)-1) = workingDirectoryName
    	vsnapMounts = Join(tkenAry,"\")
   End If
  
      
   'vsnapMounts = ScriptFolderPath & "VSNAPMounts\"
   	
    'Create First Level Parent Folder
     If objFSO.FolderExists(vsnapMounts) = False Then
          objFSO.CreateFolder(vsnapMounts)
     End If
    
    'Create Second Level folder with Seq Number
    
    pathForMounting = vsnapMounts & seq & "\"
    If objFSO.FolderExists(pathForMounting) = False Then
          objFSO.CreateFolder(pathForMounting)
    End If
    
    'Create Third Level Folder for VSNAP DB - If it doesn't exist.    
    If IsEmpty(vsnapVolumeDB) Then
   		   usingMountPointsDB=True
   		   vsnapVolumeDB= pathForMounting & "DBMountpoint"
   		   'On Error Resume Next
   		   If objFSO.FolderExists(vsnapVolumeDB) = False Then
   		   		objFSO.CreateFolder(vsnapVolumeDB)
   		   End If
    End If
    
    'Create Third Level Folder for VSNAP Log - If it doesn't exist.
    If IsEmpty(vsnapVolumeLOG) Then
   		   usingMountPointsLOG=True
   		   vsnapVolumeLOG=pathForMounting & "LOGMountpoint"
   		   'On Error Resume Next
   		   If objFSO.FolderExists(vsnapVolumeLOG) = False Then
   		   		objFSO.CreateFolder(vsnapVolumeLOG)
 		   End If
    End If


End Sub



'+----------------------------------------------------------------------------+
' Discovery Operation						              | 
'+----------------------------------------------------------------------------+


Sub Discovery
	debug("Discovering InMage Scout Installation Path")
	Dim cdpcliKey:cdpcliKey = "HKLM\SOFTWARE\SV Systems\VxAgent\InstallDirectory"
	Dim WshShell
	'Discovery CDPCLI Path from Registry
	Set WshShell = WScript.CreateObject("WScript.Shell")
	On Error Resume Next
	Dim val: val = WshShell.RegRead(cdpcliKey) 

	if isEmpty(val) then
	 cdpclikey = "HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\SV Systems\VxAgent\InstallDirectory"
	 val = WshShell.RegRead(cdpcliKey)	
	end if

	If Not isEmpty(val) Then 
		val = val & "\cdpcli.exe" 
		'Verify if the file really exist
		If FileExists(val) Then 
			cdpCLIPath = val
			debug("InMage CDPCLI Path : " & cdpCLIPath )
		End If 
	End If
	
	'Exchange Path Discovery
	
	debug("Discovering Exchange Installation Path")
	dim exPath
	Dim exchKey:exchKey = "HKLM\SOFTWARE\Microsoft\Exchange\Setup\ExchangeServerAdmin"
	exPath = WshShell.RegRead(exchKey) 
	If Not isEmpty(exPath) Then
		exPath = exPath & "\bin" 
	End If
	
	'In case of Exchange 2007 which runs on 64-bit machine, as frsvc is a 32 bit application WOW64 subsystem is redirecting 
	'the access to C:\Windows\system32\cscript.exe to C:\Windows\SYSWOW64\cscript.exe 
	If isEmpty(exPath) Then
		exchKey = "HKLM\SYSTEM\CurrentControlSet\Services\MSExchangeIS\ImagePath"
		exPath  = WshShell.RegRead(exchKey)
		If Not isEmpty(exPath)	Then
			exPath  = Trim(Replace(exPath, """", " "))		
			exPath  = Mid(exPath,1,InStrRev(exPath, "\")-1)
		End If
	End If	

    If Not isEmpty(exPath) Then    
        exchangeEseutilPath = exPath
    End If
        
	If Not isEmpty(exchangeEseutilPath) Then 
		'Verify if the file really exist
		If FileExists(exchangeEseutilPath  & "\eseutil.exe") Then 			
			exchangeEseutilPath = exchangeEseutilPath & "\eseutil"
			WScript.Echo("ESEUTIL.EXE discovered : " & exchangeEseutilPath)
		Else
			debug("File not found : " & exchangeEseutilPath)
		End If
	Else 
	    debug("[ERROR]: Invalid eseutil path.")
     	WScript.Echo("[ERROR] ExchangeIntegrityVerifier : Invalid eseutil path.")
     	WScript.Quit(-1)
	End If
	
End Sub

function FileExists(Fname)
  Set fs = CreateObject("Scripting.FileSystemObject")

  if fs.FileExists(Fname) = False then
    FileExists = True
  else
    FileExists = False
  end if
Set fs = Nothing
end Function

'+----------------------------------------------------------------------------+
' Teardown Operation											              | 
'+----------------------------------------------------------------------------+
Sub teardown
	 Dim stepResult
     If mountDBOK = True Then 
     	debug ("Unmounting VSNAP for Exchange DB Volume - Teardown Operation")
	 	stepResult = unmountVSNAP(vsnapVolumeDB,False,usingMountPointsDB)  
	 	If stepResult = True Then
	 		 If usingMountPointsDB Then
		 		objFSO.DeleteFolder vsnapVolumeDB,True
			 End If
	 	Else
	       unmountOk=False	 
	    End If
	 End If
	
	If mountLOGOK = True Then 
	   Debug ("Unmounting VSNAP for Exchange LOG Volume - Teardown Operation")
	   stepResult = unmountVSNAP(vsnapVolumeLOG,False,usingMountPointsLOG)
	   If stepResult = True Then
		 If usingMountPointsLOG Then
	 		objFSO.DeleteFolder vsnapVolumeLOG,True
	 	 End If
	   Else
	     unmountOk=False	 
	   End If
	End If
	
	'Deleting the Seq Folder, if present - Open it on exclusivity. 
     'deleteMountPointFolders()
	 
End sub

'+----------------------------------------------------------------------------+
' Deleting Mount point directories  							              | 
'+----------------------------------------------------------------------------+
Sub deleteMountPointFolders
    If usingMountPointsLOG Or usingMountPointsDB Then 
   	  Dim pathForMounting
     'pathForMounting = vsnapMounts & seq & "\"
      pathForMounting = vsnapMounts & seq
     'On Error Resume Next
      WScript.Echo "JOB Mount Point Path : " & pathForMounting
      objFSO.DeleteFolder pathForMounting,True
      objFSO.DeleteFolder Mid(vsnapMounts,1,Len(vsnapMounts)-1),True
    End If 
End Sub 

'+----------------------------------------------------------------------------+
'  Check if Current Consistent Point is same as Previous Consistent Point     | 
'+----------------------------------------------------------------------------+
Function checkIfCurrentEventIsSameAsPreviousEvent()
  
 Dim result:result = False
 Dim rcd
 If objFSO.FileExists(currentConsistencyFilePath) Then  
 	rcd = ReadFile(currentConsistencyFilePath)
 	If IsEmpty(rcd) = False Then 
 		rcd = Trim(Replace(rcd,vbCrLf,""))
  		Dim ary:ary = Split(rcd,",")
    	Dim prevConsistencyEvent:prevConsistencyEvent=ary(0)
    	If eventTag =  prevConsistencyEvent Then
      	  result = True
   		End If         
 	End If 
 End If 
 
' Dim files: files = listDir(pathForMounting,prevConsistencyFilePattern)
' Dim file
' For Each file In files
' 	Debug("Consistency File : " & file)
' 	Dim rcd: rcd = Trim(Replace(ReadFile(file),vbCrLf,""))
' 	Dim ary:ary = Split(rcd,",")
' 	Dim prevConsistencyEvent:prevConsistencyEvent=ary(0)
 	
'    If eventTag =  prevConsistencyEvent Then
'      result = True
'    End If         
' Next
 
checkIfCurrentEventIsSameAsPreviousEvent = result

End Function 


'+----------------------------------------------------------------------------+
'  Writing Status files   							                          | 
'+----------------------------------------------------------------------------+
Function processStatus(status,msg)
 Dim fileName,files
 Dim pattern
 fileName= pathForMounting & "\" & seq & "_" & fileid  & "_" & status & "_p"
 
 'Before we create first file, we should clean up the existing files.
 If fileid = 1 Then
   pattern = seq & "*.status"
   files = listDir(ScriptFolderPath,pattern)
   Dim file
   For Each file In files
     objFSO.DeleteFile(file)
   Next
 End If 
 
 
'Write a new file. If the last tag is is the same, just retain it. 
Dim consistencyStatus,currentExecutionStatus

 
If  CInt(returnCode) <> CInt(EVENT_TAG_SAME) Then
   currentExecutionStatus = True
   consistencyStatus = status  
Else
   currentExecutionStatus = False
   Dim rcd: rcd = Trim(Replace(ReadFile(currentConsistencyFilePath),vbCrLf,""))
   Dim ary:ary = Split(rcd,",")
   consistencyStatus = ary(1)   
End If
 
 'Write Previous Result File
  Dim delim:delim=","
 
 'Create a Consistent Record
  Dim consistencyRcd: consistencyRcd = eventTag & delim & consistencyStatus & delim & currentExecutionStatus
  WriteToFileNew consistencyRcd,currentConsistencyFilePath
  
 'If the fileid is last one, then aggregate the result and create a status file
 Dim i,opOk,integrityOk
 If fileid = totalfileCount Then
    pattern = seq & "_"  & "*_prev_consistency"
    files = listDir(pathForMounting,pattern)
    opOk = False
    integrityOk = True 
    For i = 1 To totalfileCount 
         Dim fileFound: fileFound=False 
         For Each file In files
              Dim tmpAry:tmpAry = Split(file,"\")
              Dim shortFileName: shortFileName = tmpAry(UBound(tmpAry))
              Dim fileNameAry:fileNameAry = Split(shortFileName,"_")
              Dim evalFileId,evalResult
              evalFileId=0
              evalFileId = fileNameAry(1)
              If i=CInt(evalFileId) Then
                rcd = Trim(Replace(ReadFile(file),vbCrLf,""))
   			    ary = Split(rcd,",")
   			    opOk = opOk Or ary(2)
   			    integrityOk = integrityOk And ary(1)
   			    fileFound=True	
                Exit For
              End If 
         Next
         If fileFound = False Then 
            integrityOk = integrityOk And False
            opOk = opOk Or False 
         End If  
    Next
 
    
   
    'Write Final Result File 
    If opOk And integrityOk Then
         writeStatusFile ScriptFolderPath() & successFile,seq
    Else
         writeStatusFile ScriptFolderPath() & failedFile,seq
    End If 
  
 End If 
 
 
End Function 


'+----------------------------------------------------------------------------+
' Mounts the VSNAP Volume for a given consistent point			              | 
'+----------------------------------------------------------------------------+
Function mountVSNAP(targetVolume,vsnapVolume,retentionLog,isMountPoint)

   Dim vsnapPoint
   If isMountPoint = True Then 
    	Dim idx: idx = InStr(1,vsnapVolume,Chr(34))
		If idx < 1 Then
		 vsnapPoint = Chr(34) & vsnapVolume & Chr(34)
		End If
   Else
     vsnapPoint = vsnapVolume
   		
   End If 

	Dim command,output
	If vsnapMode = 0 Then 
 		command = cdpCLIPath & " --vsnap --force=yes --target=" & targetVolume & " --virtual="& vsnapPoint  & " --event=" & eventTag & " --op=mount"
	Else	
		On Error Resume Next
		Dim result
		Dim exchKey:exchKey = "HKLM\SOFTWARE\Microsoft\Exchange\Setup\ExchangeServerAdmin"
		Dim WshShell
		Set WshShell = WScript.CreateObject("WScript.Shell")
		result = WshShell.RegRead(exchKey) 
		If Not isEmpty(result) Then
		    command = cdpCLIPath & " --vsnap --force=yes --target=" & targetVolume & " --virtual="& vsnapPoint  & " --app=Exchange --event=" & eventTag & " --op=mount"
		Else
			command = cdpCLIPath & " --vsnap --force=yes --target=" & targetVolume & " --virtual="& vsnapPoint  & " --app=ExchangeIS --event=" & eventTag & " --op=mount"			    
		End If
    End If
	
    If IsEmpty(retentionLog) = False Then
	   command = command & " --db=" & retentionLog   
	End If
	
	debug(command)
	'Try Create a VSNAP Using CDPCLI
	output = createProcessWithReturnCode(command)
	
	'Parse to find out if VSNAP Has mounted Successfully.
	Dim token,msg,retCode 
	 
	'token = "Virtual volume " & UCase(vsnapVolume) + ":\ mounted successfully"
	 Dim aryTokens
	 aryTokens = Split(output,"##")
	 
	 retCode = aryTokens(0)
	 output = aryTokens(1)
	 
	token = "mounted successfully"
	msg = "VSNAP for [" & targetVolume & "] on [" & vsnapPoint & "] " 
	If CInt(retCode) = 0 Then
	'If isFound(token,output) Then
		'Success Case
		mountVSNAP=True
		debug(msg & "Successful")
		
		'Works only for 3.5.1
		token = "Event Name:"
		idx = InStr(1,output,token)
		If idx > 0 Then
		  'Debug(output)
		  eventTag = Trim(Mid(output,idx+Len(token)))
		  Dim tmpAry: tmpAry = Split(eventTag,vbCrLf)
		  eventTag = tmpAry(0)
		  eventTag = Trim(Replace(eventTag,vbCrLf,""))
		  Debug ("Parsed Event Tag : " & eventTag)
		End If
	Else
		mountVSNAP=False
		Error(msg & "Failed")
		Error(output)
        Err.Raise VSNAP_MOUNT_FAILURE,"mountVSNAP","CDPCLI: Execution Failure " 
	End If

End Function

'+----------------------------------------------------------------------------+
' UnMounts the given snapshot volume								          | 
'+----------------------------------------------------------------------------+
Function unmountVSNAP(vsnapVolume,ignoreFailure,isMountPoint)


   Dim vsnapPoint
   If isMountPoint = True Then 
    	Dim idx: idx = InStr(1,vsnapVolume,Chr(34))
		If idx < 1 Then
		 vsnapPoint = Chr(34) & vsnapVolume & Chr(34)
		End If
   Else
     vsnapPoint = vsnapVolume
   		
   End If 
   
    Dim command,output
	command = cdpCLIPath & " --vsnap --force=yes --virtual="& vsnapPoint & " --op=unmount"
	debug(command)
	'Try Create a VSNAP Using CDPCLI
	output = createProcess(command)
	
	'Parse to find out if VSNAP Has mounted Successfully.
	Dim token,msg 
	token = "successfully"
	
	msg = "Unmount VSNAP on [" & vsnapPoint & "] " 
	If isFound(token,output) Then
		'Success Case
		unmountVSNAP=True
		debug(msg & "Successful")
	Else
		
		If ignoreFailure = False Then
			unmountVSNAP=False
			Error(msg & "Failed. Probably Open Handles on VSNAP Volume")
			Error(output)
         
         'Remove the comment, if the unmount succeeds in all cases, which currently not the 
         'Case, if there are any open handles to VSNAP volume, it fails with an exit code.
         
         'Err.Raise VSNAP_UNMOUNT_FAILURE,"mountVSNAP","CDPCLI: Execution Failure: Probably Open Handles on VSNAP Volume" 
		Else
		
		'We cannot ignore all types of failure
		'The following messages can be ignored
		'   (a) UnMount failure as the specified volume doesn't exist
		'   (b) UnMount failure on the specified volume as the VSNAP has already been unmounted.
		
		   Dim errorToken: errorToken = "Unmount Failed: Specified drive letter or mount point is not a virtual volume."
		   Dim errorToken1: errorToken1 = "Unmount Failed: GET Symlink IOCTL FailedErrorCode: 2"
           Dim noError: noError = "ErrorCode 0x0"
		   Dim acceptableReason: acceptableReason = false
		   acceptableReason = isFound(errorToken,output) Or isFound(errorToken1,output) or isFound(noError, output)
		   If acceptableReason = False Then
		     unmountVSNAP=False
		     Error(msg & "Failed. Unknown Reason")
		     Error(output) 
		   Else
		     unmountVSNAP=True
		   End  If	
		End If
	End If
End Function 

'+----------------------------------------------------------------------------+
' Verifies Exchange DB Checksum for all the exchange DBFiles                  | 
' Checksum verification ex. eseutil.exe /K /i "Y:\Priv1.edb"				  |
'+----------------------------------------------------------------------------+
Function verifyChecksumForDBFiles()

'Dim files: files = listDir(exchangeDBPath,"*.edb")
'Dim file

'print("No Data Files: " & UBound(files)+1)
print("Data Files Verification")
print("-----------------------")
Dim integrity:integrity=True
Dim vsnapDBFilePath
vsnapDBFilePath =  vsnapVolumeDB & Mid(exchangeDBFilePath,Len(targetVolumeDB)+1)
integrity = verifyChecksumforDbFile(vsnapDBFilePath)

'For Each file In files
' integrity = integrity And verifyChecksumforDbFile(file)
'Next

verifyChecksumForDBFiles = integrity

End Function 
'+----------------------------------------------------------------------------+
'  Verifies Exchange DB Checksum for the passed DBFile                        | 
' Checksum verification ex. eseutil.exe /K /i "Y:\Priv1.edb"                  |
'+----------------------------------------------------------------------------+

Function verifyChecksumforDbFile(file)
 	
 	Dim idx: idx = InStr(1,file,Chr(34))
	If idx < 1 Then
	 file = Chr(34) & file & Chr(34)
	End If
 	
	Dim command: command = exchangeEseutilPath & " /K " & file & " /i"
	
	debug(command)
	'Verify the file using eseutil
	Dim output
	output = createProcess(command)
	
	
	'Parse to find out if Checksum computation was correct.
	Dim token,token1,result,msg 
	token = "0 bad checksums"
	token1 = "0 wrong page numbers"
	result = isFound(token,output) And isFound(token1,output)
	
	msg = "Checksum for [" & file & "] is "  
	If result Then
		'Success Case
		verifyChecksumforDbFile=True
		msg = msg & "fine"
		print(msg)
		debug(msg)
	Else
		verifyChecksumforDbFile=False
		msg = msg & "doesn't match"
		print msg
		Error(msg)
		Error(output)
	End If
End Function

'+----------------------------------------------------------------------------+
' Verifies Exchange Log Range                                                 | 
' This check needs to be done on *.edb file (db file)					      |
' eseutil /mk e:/db/priv.db													  |	
'+----------------------------------------------------------------------------+
Function verifyExchangeLogRange()

' Obsolete logic.
' Get *.chk files from the Exchange Log directory
' Dim files: files = listDir(exchangeLogPath,"*.chk")



Dim file
Dim logIntegrity:logIntegrity=True
'print("No Chk Files: " & UBound(files)+1)
print("DBFile File Range Computation")
print("--------------------------")
Dim vsnapDBFilePath
vsnapDBFilePath =  vsnapVolumeDB & Mid(exchangeDBFilePath,Len(targetVolumeDB)+1)

logIntegrity=verifyExchangeLogRangeForFile(vsnapDBFilePath)

'For Each file In files
 'logIntegrity=logIntegrity And verifyExchangeLogRangeForFile(file)
'Next

verifyExchangeLogRange = logIntegrity

End Function


'+----------------------------------------------------------------------------+
' Verifies Exchange Log Range                                                 | 
' The Log Range Specified in *.chk files and no of log files should match     |
' Example: eseutil.exe /mh e:\db\priv.edb                                     |
'+----------------------------------------------------------------------------+
Function verifyExchangeLogRangeForFile(file)
    
    Dim idx: idx = InStr(1,file,Chr(34))
	If idx < 1 Then
	 file = Chr(34) & file & Chr(34)
	End If
    
    
    
    Dim logIntegrityStatus:logIntegrityStatus = True
	Dim command: command = exchangeEseutilPath & " /mh " & file
	
	debug(command)
	'Verify the file using eseutil
	Dim output
	output = createProcess(command)
	
	Dim token,result,msg 
	token = "Log Required: "
	result = isFound(token,output) 
	
	If result And (Err.number=0) Then
	    'Atleast Parsing seems to be valid.
		msg = "DB file [" & file & "] is parsed and valid"
		debug(msg)
		print msg
		
		'Parse CheckSum Range
		Dim idx1,idx2,idx3
		idx1 = InStr(1,output,token,1) 
		idx1 = idx1+Len(token)
		idx2 = InStr(idx1,output,vbcrlf,1)  
		Dim checkRange: checkRange = Trim(Mid(output,idx1,(idx2-idx1)))
		
		'Fix for SP2
		
		idx1 = InStr(1,checkRange,"(",1)
		If idx1 > 0 Then 
		'is SP2 String
		Dim ary: ary = Split(checkRange,"(")
	  	    checkRange = Trim(ary(0))
		End If 
		
		Debug(checkRange)
		
		Dim ranges: ranges=Split(checkRange,"-",-1,1)
   	    checkRangeFrom = ranges(0) 'Hex Value'
		checkRangeTo = ranges(1) 'Hex Value'
		
		checkRangeFileCount = (checkRangeTo - checkRangeFrom)+1
		msg = "Check Range for [" & file & "] is " & checkRangeFrom & " to " & checkRangeTo & " ,total files: " & checkRangeFileCount
		
		debug(msg)
		print msg
		
		'Verify No of Log files matching the range
		
		Dim filePattern: filePattern = chkFilePrefix & "*.log"
		debug("File Pattern:" & filePattern)
		
		Dim vsnapLOGFilePath
		
		'exchangeLogFilePath
		If IsEmpty("exchangeLogFilePath") Then
			vsnapLOGFilePath =  vsnapVolumeLOG & Mid(exchangeLogPath,Len(targetVolumeLOG)+1)
		Else
		   	vsnapLOGFilePath =  vsnapVolumeLOG & Mid(exchangeLogFilePath,Len(targetVolumeLOG)+1)
		End If

		msg = "Searching in VSNAP Exchange Log Directory : " &  vsnapLOGFilePath
		Debug(msg)
		print msg
		
		Dim files: files = listDir(vsnapLOGFilePath,filePattern)
		Dim fileCount: fileCount = UBound(files)
		Dim checkResult
		
		'Loop through each of the files to check, if the required files are present
		Dim matchNotFound: matchNotFound=True
		Dim rangeCounter
		For rangeCounter = checkRangeFrom To checkRangeTo
			Dim logFile, fileSuffix
			
			 'Logic for a Corner case
			 Dim suffix: suffix = DecToHex(rangeCounter)
			 fileSuffix = suffix & ".log"
			 
			 
			 Dim test: test=False
			 For Each logFile In files
				 If InStrRev(logFile,fileSuffix,-1,1) > 0 Then 
				    test=True
					msg = "Required Log [" & logFile & "] found"
					debug(msg)
					print(msg)
					
					'Verify the log files
		 			logIntegrityStatus = logIntegrityStatus And verifyLogIntegrity(logFile) 
					Exit For
				 End If
			 Next
			 If test = False Then
			   Dim prefix: prefix = Split(filePattern,"*") 
			   msg = "Required Log following [" & prefix(0) &"*" & fileSuffix & "] pattern Not found"
				debug(msg)
				print msg
				matchNotFound=False
			 End If
		Next
				
		If matchNotFound Then
				checkResult = "Found"
		Else
				checkResult = "Not Found"
		End If 
		
		msg = "All Required Log Files were " & checkResult 
		debug(msg)
		print msg
		
		'Set cumulative status
		verifyExchangeLogRangeForFile=matchNotFound And logIntegrityStatus
	Else
		verifyExchangeLogRangeForFile=False
		msg = "Checksum file [" & file & "] seems to be invalid"
		print msg
		Error(msg)
		Error(output)
	End If
End Function 
'+----------------------------------------------------------------------------+
' Verifies Exchange Log Integrity                                             | 
' To Verify the Exchange log integrity    									  |
'+----------------------------------------------------------------------------+
Function verifyLogIntegrity(file)
	'Function to verify the integerity of all the log files
	'eseutil.exe /k Z:\E00
	Dim idx: idx = InStr(1,file,Chr(34))
	If idx < 1 Then
	 file = Chr(34) & file & Chr(34)
	End If
	
	Dim filePattern
	
	filePattern= file
   Dim command: command = exchangeEseutilPath & " /K " & filePattern
	
	debug(command)
	'Verify the file using eseutil
	Dim output
	output = createProcess(command)
		
	
	'Parse to find out if Checksum computation was correct.
	Dim token,result,msg 
	token = "Integrity check passed for log file:"
	
	result = isFound(token,output)
	
	If result Then
		'Success Case
		verifyLogIntegrity=True
		msg = "All the log files were verified to be fine"
		print(msg)
		debug(msg)
	Else
		verifyLogIntegrity=False
		msg = msg & "Some of the log file are damaged look below for the details"
		print msg
		Error(msg)
		Error(output)
	End If
End Function 
'+----------------------------------------------------------------------------+
'  Function Writes Status File on Completion                                  | 
'+----------------------------------------------------------------------------+
Function writeStatusFile(fileName,msg)
   
   Debug("Status File [" & fileName & "] Written" )
   If fileName <> VBNullString Then
		Dim objFile
      Set objFile = objFSO.OpenTextFile(fileName, 2, True)
		objFile.WriteLine msg
		objFile.Close
		Set objFile = Nothing
	End If
   writeStatusFile=True
End Function

'+----------------------------------------------------------------------------+
'  Function Track Record					                                  | 
'+----------------------------------------------------------------------------+
Function writeTrackRecord(fileName)
   debug("Track Record File : " & fileName)
   Dim msg
   Dim delim:delim=","	
   msg = Now() &  delim & _
         seq & delim & fileid & delim & totalfileCount & delim & _
         eventTag & delim & _
         overallOk & delim & _
		 exchangeDBFilePath & delim & _
		 dbOk  & delim & _
		 exchangeLOGFilePath & delim & _
		 logOk  & delim & _
		 checkRangeFrom & delim & checkRangeTo & delim & checkRangeFileCount        
   
   If fileName <> VBNullString Then
		Dim objFile
      Set objFile = objFSO.OpenTextFile(fileName, 8, True)
		objFile.WriteLine msg
		objFile.Close
		Set objFile = Nothing
	End If
   writeTrackRecord=True
End Function


'+----------------------------------------------------------------------------+
'  Function Writes Status File on Completion                                  | 
'+----------------------------------------------------------------------------+

Function deleteStatusFile()
 On Error Resume Next
 Set objFile = objFSO.deleteFile(successFile,True)
 Set objFile = objFSO.deleteFile(FailedFile,True)
End Function

'region COM ScriptComplete

'X============================================================================X
'								Process Related Functions				      '	
'	                                                                          '
'X============================================================================X
'+----------------------------------------------------------------------------+
'  Function to Create Process                                                 | 
'+----------------------------------------------------------------------------+
 Function createProcess(command)
	Dim WshShell,oExec
	Dim outStr, tryCount
	Dim startTime: startTime = Timer()
	outStr = ""
	tryCount = 0
	Set WshShell = CreateObject("wscript.Shell")
   On Error Resume Next
   Set oExec = WshShell.Exec(command)
	If Err.number = 0 Then
      Do While True
	     Dim outRec
	     outRec = readStdOutErrStream(oExec)
	     If -1 = outRec Then
	          If tryCount > 2 And oExec.Status = 1 Then
	               Exit Do
	          End If
	          tryCount = tryCount + 1
	          WScript.Sleep 100
	     Else
	          outStr = outStr & outRec
	          tryCount = 0
	     End If
	   Loop
      debug(outStr)
   Else
     Err.raise SCRIPT_PREREQUSITE_FAILURE,err.Source,err.Description & " for " & command
   End If
   Dim endTime: endTime = Timer
   Debug("Time taken for executing command [" & command & "] is :" & (endTime - startTime) & " Seconds")
	createProcess = outStr
	
End Function



'+----------------------------------------------------------------------------+
'  Function to Create Process with Return Code                                | 
'+----------------------------------------------------------------------------+
 Function createProcessWithReturnCode(command)
	Dim WshShell,oExec
	Dim outStr, tryCount,returnCode
	Dim startTime: startTime = Timer()
	outStr = ""
	tryCount = 0
	Set WshShell = CreateObject("wscript.Shell")
   On Error Resume Next
   Set oExec = WshShell.Exec(command)
  	If Err.number = 0 Then
      Do While True
	     Dim outRec
	     outRec = readStdOutErrStream(oExec)
	     If -1 = outRec Then
	          If tryCount > 2 And oExec.Status = 1 Then
	               Exit Do
	          End If
	          tryCount = tryCount + 1
	          WScript.Sleep 300
	     Else
	          outStr = outStr & outRec
	          tryCount = 0
	     End If
	   Loop
	  
      debug(outStr)
   Else
     Err.raise SCRIPT_PREREQUSITE_FAILURE,err.Source,err.Description & " for " & command
   End If
   
   returnCode = oExec.ExitCode 
   
   Dim endTime: endTime = Timer
   Debug("Time taken for executing command [" & command & "] is :" & (endTime - startTime) & " Seconds")
	createProcessWithReturnCode = returnCode & "##" & outStr
	
End Function



'+----------------------------------------------------------------------------+
'    Function: Reads from output stream, if EOF is reach then, it reads from  ' 
'	  Err Stream. if there is still no data, then it assigns a value           '   
'	  of -1		                                                               '	
'+-----------------------------------------------------------------------------
 Function readStdOutErrStream(oExec)

     If Not oExec.StdOut.AtEndOfStream Then
          readStdOutErrStream = oExec.StdOut.ReadAll
          Exit Function
     End If

     If Not oExec.StdErr.AtEndOfStream Then
          readStdOutErrStream = oExec.StdErr.ReadAll
          Exit Function
     End If
     
     readStdOutErrStream = -1
 End Function
'endregion

'X============================================================================X
'								Utility Functions                                     '	
'	                                                                           '
'X============================================================================X

'+----------------------------------------------------------------------------+
'  Function to Search a Substring within a String                             | 
'+----------------------------------------------------------------------------+
 Function isFound(token,sourceString)
	Dim resp
	resp = InStr(1,sourceString,token,1)
   If resp > 0 Then 
	  isFound=True
	Else
	  isFound=False
	End If
 End Function

'+----------------------------------------------------------------------------+
'  Function Returns Script Folder Path			                              | 
'+----------------------------------------------------------------------------+
 Function ScriptFolderPath()
   Dim scriptFullPath: scriptFullPath = WScript.Application.ScriptFullName
   Dim idx
   idx = InStrRev(scriptFullPath,WScript.Application.ScriptName,-1,1)
   ScriptFolderPath = Left(scriptFullPath,idx-1)
 End Function 

'+----------------------------------------------------------------------------+
'  Function Gets Drive Letter for a file path element                         | 
'+----------------------------------------------------------------------------+
 Function DriveLetterForFilePath(file)
   Dim idx
   idx = InStr(1,file,":",1)
   If idx>0 Then
   	 DriveLetterForFilePath = Mid(file,1,idx-1)
   End If
 End Function

'+----------------------------------------------------------------------------+
'  Function Gets Drive Letter for a Folder path element                       | 
'+----------------------------------------------------------------------------+
 Function DriveLetterForFolderPath(folder)
   Dim idx
   idx = InStr(1,folder,":",1)
   If idx>0 Then
   	 DriveLetterForFolderPath = Mid(folder,1,idx-1)
   End If
 End Function


'+----------------------------------------------------------------------------+
'  Function to Convert Hex to Decimal			                              | 
'+----------------------------------------------------------------------------+
Function HexToDec(strHex)
  dim lngResult
  dim intIndex
  dim strDigit
  dim intDigit
  dim intValue

  lngResult = 0
  for intIndex = len(strHex) to 1 step -1
    strDigit = mid(strHex, intIndex, 1)
    intDigit = instr("0123456789ABCDEF", ucase(strDigit))-1
    if intDigit >= 0 then
      intValue = intDigit * (16 ^ (len(strHex)-intIndex))
      lngResult = lngResult + intValue
    else
      lngResult = 0
      intIndex = 0 ' stop the loop
    end if
  next

  HexToDec = lngResult
End Function

'+----------------------------------------------------------------------------+
'  Function to Convert Decimal to Hex			                              | 
'+----------------------------------------------------------------------------+
Function DecToHex(strDec)
	DecToHex = Hex(strDec)
End Function

'+----------------------------------------------------------------------------+
'  Function to List Files matching the pattern in a given directory           | 
'+----------------------------------------------------------------------------+

Function listDir(path,pattern)
  Dim objLFSO
  Dim pathExists
  Set objLFSO  = WScript.CreateObject("Scripting.FileSystemObject")
  pathExists = objLFSO.FolderExists(path)
  
  If pathExists Then

     Dim Folder: Set Folder = objLFSO.GetFolder(path)
     Dim Files: Set Files = Folder.Files
     Dim File,n
     ReDim a(10)
     For Each file In files
        If CompareFileName(File.Name,pattern) Then
            If n > UBound(a) Then ReDim Preserve a(n*2)
                a(n) = File.Path
                n = n + 1
            End If
     Next
   ReDim Preserve a(n-1)
   ListDir = a
  Else
   debug("Supplied Path " & path & " doesn't exist")
   err.Raise 100,"ExchangeVerify"
  End If
  

End Function
'+----------------------------------------------------------------------------+
'  Function to Verify the Name of the file                                    | 
'+----------------------------------------------------------------------------+
Private Function CompareFileName (ByVal Name, ByVal Filter)
   CompareFileName = False
   Dim np, fp: np = 1: fp = 1
   Do
      If fp > Len(Filter) Then CompareFileName = np > Len(name): Exit Function
      
      If Mid(Filter,fp) = ".*" Then    
         If np > Len(Name) Then CompareFileName = True: Exit Function
         End If
      
      If Mid(Filter,fp) = "." Then     ' special case: "." at end of filter
         CompareFileName = np > Len(Name): Exit Function
         End If
      
      Dim fc: fc = Mid(Filter,fp,1): fp = fp + 1
      
      Select Case fc
         Case "*"
            CompareFileName = CompareFileName2(name,np,Filter,fp)
            Exit Function
         Case "?"
            If np <= Len(Name) And Mid(Name,np,1) <> "." Then np = np + 1
         Case Else
            If np > Len(Name) Then Exit Function
            Dim nc: nc = Mid(Name,np,1): np = np + 1
            If StrComp(fc,nc,vbTextCompare)<>0 Then Exit Function
      End Select
   Loop
   End Function
'+----------------------------------------------------------------------------+
'  Function to Verify the Name of the file                                    | 
'+----------------------------------------------------------------------------+
Private Function CompareFileName2 (ByVal Name, ByVal np0, ByVal Filter, ByVal fp0)
   Dim fp: fp = fp0
   Dim fc2
   
   ' skip over "*" and "?" characters in filter
   Do     
      If fp > Len(Filter) Then CompareFileName2 = True: Exit Function
      fc2 = Mid(Filter,fp,1): fp = fp + 1
      If fc2 <> "*" And fc2 <> "?" Then Exit Do
   Loop
   
   If fc2 = "." Then
      ' special case: ".*" at end of filter
      If Mid(Filter,fp) = "*" Then   
         CompareFileName2 = True: Exit Function
      End If
      If fp > Len(Filter) Then
         CompareFileName2 = InStr(np0,Name,".") = 0: Exit Function
      End If
   End If
   
   
   Dim np
   For np = np0 To Len(Name)
      Dim nc: nc = Mid(Name,np,1)
      If StrComp(fc2,nc,vbTextCompare)=0 Then
         If CompareFileName(Mid(Name,np+1),Mid(Filter,fp)) Then
            CompareFileName2 = True: Exit Function
            End If
         End If
      Next
   CompareFileName2 = False
   End Function
'X============================================================================X
'								Log Functions		   					                  '	
'	                                                                           '
'X============================================================================X

'+----------------------------------------------------------------------------+
'  Function to Log Debug Statements			                                    | 
'+----------------------------------------------------------------------------+

 Function debug(message)
	 If debugMode = 2 Then
		Log(message)
 	 End If
 End Function

'+----------------------------------------------------------------------------+
'  Function to Log Error Statements			                                    | 
'+----------------------------------------------------------------------------+
Function Error(message)
	 If debugMode >= 1 Then
		Log(message)
 	 End If
End Function

'+----------------------------------------------------------------------------+
'  Function to Log Debug/Error Statements to Console/File                     | 
'+----------------------------------------------------------------------------+
Function Log(message)
	WScript.Echo message
	LogToFile(message)
End Function 

'+----------------------------------------------------------------------------+
'  Function to Format Record and Write to Log file                            | 
'+----------------------------------------------------------------------------+
Function LogToFile(message)
    message = Now() & vbTab & message
    WriteToFile message,debuglogFile
End Function

'+----------------------------------------------------------------------------+
'  Function to write message to a file                                        | 
'+----------------------------------------------------------------------------+
Function WriteToFile(message,tfile)
   Dim objFile
	If tfile <> VBNullString Then
		Set objFile = objFSO.OpenTextFile(tfile, 8, True)
		objFile.WriteLine message
		objFile.Close
		Set objFile = Nothing
	End If
End Function 

'+----------------------------------------------------------------------------+
'  Function to write message to new file                                      | 
'+----------------------------------------------------------------------------+
Function WriteToFileNew(message,tfile)
   Dim objFile
	If tfile <> VBNullString Then
		Set objFile = objFSO.OpenTextFile(tfile, 2, True)
		objFile.WriteLine message
		objFile.Close
		Set objFile = Nothing
	End If
End Function 




'+----------------------------------------------------------------------------+
'  Function to Read all the contents from a file                              | 
'+----------------------------------------------------------------------------+

Function ReadFile(sFilePathAndName) 
   dim sFileContents,oTextStream 
   If objFSO.FileExists(sFilePathAndName) = True Then 
      Set oTextStream = objFSO.OpenTextFile(sFilePathAndName,1) 
      sFileContents = oTextStream.ReadAll 
      oTextStream.Close 
      Set oTextStream = nothing 
   End if 
   ReadFile = sFileContents 

End Function 


'X============================================================================X
'								Exchange Verification Report                  '	
'                                                                             '
'X============================================================================X

'+----------------------------------------------------------------------------+
'  Function to Print Detail Report Header                                     | 
'+----------------------------------------------------------------------------+
 Function printHeader()
   
  print("====================================================================")
  print("                 Exchange Data Integrity Report                     ")
  print("Generated    On: " & Now())
  print("Generated    By: InMage ExchangeVerify.vbs")
  print("Consistency Tag: " & eventTag) 
  print("====================================================================")
   
 End Function
 
'+----------------------------------------------------------------------------+
'  Function to Print Detail Report Footer                                     | 
'+----------------------------------------------------------------------------+
Function printFooter
	print("Ended At: " & Now())
	print("========================= End of Report ============================")
End Function 

'+----------------------------------------------------------------------------+
'  Function to Print Report Details                                           | 
'+----------------------------------------------------------------------------+
Function print(line)
 WriteToFile line,reportfile
End Function




'X============================================================================X
'								Test Suite                                            '	
'	                                                                           '
'X============================================================================X
Sub test
	 testHexToDecFunctions()
	 testIsFound()
     testDirectoryListing()
     testScriptPath()
     testStatusFiles()
End Sub
'+----------------------------------------------------------------------------+
'  Subroutine: To Test isFound function                                       | 
'+----------------------------------------------------------------------------+

Sub testIsFound
	Dim out,result
	
	'First Test Case 
	out= isFound("test","This is a test string")
	If out = True Then
		result = "pass"
	Else
		result = "fail"
	End If
	Log("Testcase:FindString  Subject:isFound Result:" & result)
	
	'First Test Case
	out= isFound("another","This is a test string")
	If out = True Then
		result = "fail"
	Else
		result = "pass"
	End If
	Log("Testcase:StringNotFound  Subject:isFound Result:" & result)
	
End Sub
'+----------------------------------------------------------------------------+
'  Subroutine: To Test Directory listing function                             | 
'+----------------------------------------------------------------------------+
Sub testDirectoryListing
     Dim files: files = listDir("C:\windows","*.exe")
     Dim file
     For Each file In files
      Log(file)
     Next
End Sub
'+----------------------------------------------------------------------------+
'  Subroutine: To Test Script Path                                            | 
'+----------------------------------------------------------------------------+
Sub testScriptPath
    Log("Script Folder Path : " & ScriptFolderPath) 
End Sub 
'+----------------------------------------------------------------------------+
'  Subroutine: Status File Handling in Program                                | 
'+----------------------------------------------------------------------------+
Sub testStatusFiles
  deleteStatusFile()
  'writeStatusFile successFile,"Success"
  'writeStatusFile failedFile,"Failed"
End Sub

'+----------------------------------------------------------------------------+
'  Subroutine: Hex to Dec Functions			                                  | 
'+----------------------------------------------------------------------------+
Sub testHexToDecFunctions()
  Dim val,hexStr,decStr
  val = 10

 Dim count
 
 For count=0 To 120 Step 1
 	Log("Decimal #: " & count & " Hex #: " & DecToHex(count))
  val = count	
  hexStr = DecToHex(val)
  decStr = HexToDec(hexStr)
  
  If val = decStr Then
    Log("DEC-HEX-DEC Conversion Pass")
  Else
  	 Log("DEC-HEX-DEC Conversion Fail")	
  End If
 Next	
End Sub