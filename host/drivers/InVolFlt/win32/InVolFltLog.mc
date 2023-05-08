;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;
;Module Name:
;
;    InVolFlt.mc
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Revision History:
;
;--*/
;
;#ifndef _InVolFlt_mc_
;#define _InVolFlt_mc_
;
;//
;//  Status values are 32 bit values layed out as follows:
;//
;//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
;//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;//  +---+-+-------------------------+-------------------------------+
;//  |Sev|C|       Facility          |               Code            |
;//  +---+-+-------------------------+-------------------------------+
;//
;//  where
;//
;//      Sev - is the severity code
;//
;//          00 - Success
;//          01 - Informational
;//          10 - Warning
;//          11 - Error
;//
;//      C - is the Customer code flag
;//
;//      Facility - is the facility code
;//
;//      Code - is the facility's status code
;//
;
MessageIdTypedef=NTSTATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )
;// facility codes 0-0xff are reserved for Microsoft
FacilityNames=(System=0x0
               RpcRuntime=0x2:FACILITY_RPC_RUNTIME
               RpcStubs=0x3:FACILITY_RPC_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
               InVolFlt=0x112:FACILITY_INVOLFLT_ERROR_CODE
              )
;// The sources file should set the "-c" message compiler (MC_OPTIONS=-c) option to turn on the customer bit in codes

MessageId=0x0001 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_NO_NPAGED_POOL_FOR_DIRTYBLOCKS
Language=English
Encountered resource limitation to store changes to volume %2 (GUID = %3).
.

MessageId=0x0002 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_ERR_VOLUME_WRITE_PAST_EOV
Language=English
A write attempt past the end of the volume was detected on volume %2 (GUID = %3). This may
indicate the volume has dynamically been grown. 
.

MessageId=0x000B Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_NO_MEMORY
Language=English
Not enough memory was available to perform an operation, as a result replication on
volume %2 (GUID = %3) has failed.
.

MessageId=0x000C Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_CANT_OPEN
Language=English
The file used to store change information for volume %2 (GUID = %3) could not be opened. Check the
registry entries for InMage Volume Replication.
.

MessageId=0x000D Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_CANT_UPDATE_HEADER
Language=English
The file used to store change information for volume %2 (GUID = %3) could not be written to.
.

MessageId=0x000E Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_CANT_READ
Language=English
The file used to store change information for volume %2 (GUID = %3) could not be read. Check for
disk errors on the device.
.

MessageId=0x000F Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_LOG_DAMAGED
Language=English
The file used to store change information for volume %2 (GUID = %3) is damaged and could not
be automatically repaired.
.

MessageId=0x0010 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_CANT_APPLY_SHUTDOWN_CHANGES
Language=English
Changes on volume %2 (GUID = %3)that occured at the previous system shutdown could not be merged with
current changes.
.

MessageId=0x0011 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_ERR_BITMAP_FILE_CREATED
Language=English
A new file used to store change information for volume %2 (GUID = %3) is created.
.

MessageId=0x0012 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_ERR_LOST_SYNC_SYSTEM_CRASHED
Language=English
The system crashed or experienced a non-controlled shutdown. Replication sync on
volume %2 (GUID = %3) will need to be reestablished.
.

MessageId=0x0013 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_NAME_ERROR
Language=English
The file used to store change information for volume %2 (GUID = %3) could not be opened
because of a naming problem. Check the registry.
.

MessageId=0x0014 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_WRITE_ERROR
Language=English
The file used to store change information for volume %2 (GUID = %3) could not be written to.
.

MessageId=0x0015 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_CANT_INIT
Language=English
The file used to store change information for  volume %2 (GUID = %3) could not be initialized.
.

MessageId=0x0016 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_ERR_BITMAP_FILE_LOG_FIXED
Language=English
The file used to store change information for volume %2 (GUID = %3) was repaired, but the volume
needs to be resynchronized.
.

MessageId=0x0017 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_TOO_MANY_LAST_CHANCE
Language=English
The file used to store change information for volume %2 (GUID = %3) did not have sufficent reserved
space to store all changes at system shutdown.
.

MessageId=0x0018 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_ERR_IN_SYNC
Language=English
The replication on volume %2 (GUID = %3) has resumed correctly.
.

MessageId=0x0019 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FINAL_HEADER_VALIDATE_FAILED
Language=English
The final write to store change information for volume %2 (GUID = %3) failed header validation.
.

MessageId=0x0020 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FINAL_HEADER_DIRECT_WRITE_FAILED
Language=English
The final direct write to store change information for volume %2 (GUID = %3) failed.
.

MessageId=0x0021 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FINAL_HEADER_FS_WRITE_FAILED
Language=English
The final file system write to store change information for volume %2 (GUID = %3) failed.
.

MessageId=0x0022 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FINAL_HEADER_READ_FAILED
Language=English
The final direct read to store change information for volume %2 (GUID = %3) failed.
.

MessageId=0x0023 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_DELETE_BITMAP_FILE_NO_NAME
Language=English
Deleting the file used to store changes on volume %2 (GUID = %3) failed because the filename was not set.
.

MessageId=0x0024 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_BITMAP_FILE_EXCEEDED_MEMORY_LIMIT
Language=English
The memory limit for storing changes on volume %2 (GUID = %3) has exceeded. Increase memory limit or increase
change granularity using appropriate registry entries.
.

MessageId=0x0025 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_VOLUME_SIZE_SEARCH_FAILED
Language=English
The driver was unable to determine the correct size of volume %2 (GUID = %3) using a last sector search.
.

MessageId=0x0026 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_VOLUME_GET_LENGTH_INFO_FAILED
Language=English
The driver was unable to determine the correct size of volume %2 (GUID = %3) using IOCTL_DISK_GET_LENGTH_INFO.
.

MessageId=0x0027 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_TOO_MANY_EVENT_LOG_EVENTS
Language=English
The driver has written too many events to the system event log recently. Events will be discarded for 
the next time interval.
.

MessageId=0x0028 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_FIRST_FAILURE_TO_OPEN_BITMAP
Language=English
The driver failed to open bitmap file for volume %2 (GUID = %3) on its first attempt.
.

MessageId=0x0029 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_SUCCESS_OPEN_BITMAP_AFTER_RETRY
Language=English
The driver succeeded to open bitmap file for volume %2 (GUID = %3) after %4 retries. 
TimeInterval between first failure and success to open bitmap is %5 seconds.
.

MessageId=0x0030 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_INFO_OPEN_BITMAP_CALLED_PRIOR_TO_OBTAINING_GUID
Language=English
The driver is opening bitmap for volume prior to symbolic link with GUID is created.
.

MessageId=0x0031 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FAILED_TO_ALLOCATE_DATA_POOL
Language=English
The driver has failed to allocate memory required for data pool.
.

MessageId=0x0032 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_THREAD_SHUTDOWN_IN_PROGRESS
Language=English
Thread shutdown is in progess.
.

MessageId=0x0033 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_DATA_FILE_OPEN_FAILED
Language=English
Data file open failed for volume %2 (GUID = %3) with status %4
.

MessageId=0x0034 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_WRITE_TO_DATA_FILE_FAILED
Language=English
Write to data file %4 for volume %2 (GUID = %3) failed with status %5
.

MessageId=0x0035 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_STATUS_NO_MEMORY
Language=English
Allocation of memory of size %4 type %2 failed in file %3 line %5
.

MessageId=0x0036 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_DELETE_FILE_FAILED
Language=English
Deletion of data file for volume %2 (GUID = %3) failed with  error %4
.

MessageId=0x0037 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_FILE_OPEN_FAILED
Language=English
Failed to open %2 with status %4 in file %3 line %5
.

MessageId=0x0038 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_DELETE_ON_CLOSE_FAILED
Language=English
Delete on close failed on %2 with status %4 in file %3 line %5
.

MessageId=0x0039 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_PERSISTENT_THREAD_WAIT_FAILED
Language=English
Persistent Thread Exited as it failed to wait.
.

MessageId=0x003A Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_PERSISTENT_THREAD_OPEN_REGISTRY_FAILED
Language=English
Persistent Thread Exited as it failed to open the Registry.
.

MessageId=0x003B Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL
Language=English
Encountered Generic Non Paged resource limitation for volume %2 (GUID = %3).
.

MessageId=0x003C Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_NO_GENERIC_PAGED_POOL
Language=English
Encountered Generic Paged resource limitation for volume %2 (GUID = %3).
.

MessageId=0x003D Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_SEQUENCE_MISMATCH
Language=English
Global Sequence %2 greater than DriverContext ullTimeStampSequenceNumber %3 (%4:%5).
.

MessageId=0x003E Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRST_LAST_SEQUENCE_MISMATCH
Language=English
Change node First Sequence Number %2 is greater than Last Sequence Number %3 (%4:%5).
.

MessageId=0x003F Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRST_LAST_TIME_MISMATCH
Language=English
Change node First Time Stamp %2 is greater than Last Time Stamp %3 (%4:%5).
.

MessageId=0x0040 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRST_NEXT_SEQDELTA_MISMATCH
Language=English
Previous Sequence Delta %2 (index = %3) is greater than Next Sequence Delta %4 (index = %5).
.

MessageId=0x0041 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRST_NEXT_TIMEDELTA_MISMATCH
Language=English
Previous Time Delta %2 (index = %3) is greater than Next Time Delta %4 (index = %5).
.

MessageId=0x0042 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_LAST_SEQUENCE_GENERATION
Language=English
Last Sequence Number %2 not equal to Last Delta %3 plus First Sequence Number %4 (%5).
.

MessageId=0x0043 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_PREV_CUR_SEQUENCE_MISMATCH
Language=English
Previous Change node Last Sequence Number %2 is greater than Current Change node First Sequence Number %3 (%4:%5).
If the first parameter is all F's(MAX value), either there may be Unexpected shutdown or resync reported.
.

MessageId=0x0044 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_PREV_CUR_TIME_MISMATCH
Language=English
Previous Change node Last Time stamp %2 is greater than Current Change node First Time stamp %3 (%4:%5).
If the first parameter is all F's(MAX value), either there may be Unexpected shutdown or resync reported.
.

MessageId=0x0045 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_INFO_FIRSTFILE_VALIDATION1
Language=English
First diff file after Resync start (in Data File mode) being drained, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0046 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION2
Language=English
Start-End TS Problem with First diff file after Resync start (in Data File mode) being drained, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0047 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION3
Language=English
PrevEndTS Problem with First diff file after Resync start (in Data File mode) being drained, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0048 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_INFO_FIRSTFILE_VALIDATION4
Language=English
First diff file after Resync start (Not in Data File mode) being drained, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0049 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION5
Language=English
Start-End TS Problem with First diff file after Resync start (Not in Data File mode) being drained, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0050 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION6
Language=English
PrevEndTS Problem with First diff file after Resync start (Not in Data File mode) being drained, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0051 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION7
Language=English
LastChangeNode is of type DataFile. So not closing it. Volume: %2 (%3), Current timestamp: %4, Last ResyncStartTS: %5.
.

MessageId=0x0052 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION8
Language=English
LastChangeNode is NULL. So can not close it. Volume: %2 (%3), Current timestamp: %4, Last ResyncStartTS: %5.
.

MessageId=0x0053 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_INFO_FIRSTFILE_VALIDATION9
Language=English
First diff file after Resync start (TSO file) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, TSOEndTS: %5.
.

MessageId=0x0054 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION10
Language=English
PrevEndTS Problem with First diff file after Resync start (TSO file) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, TSOEndTS: %5.
.

MessageId=0x0055 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_INFO_FIRSTFILE_VALIDATION_C1
Language=English
First diff file after Resync start (in Data File mode) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0056 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION_C2
Language=English
Start-End TS Problem with First diff file after Resync start (in Data File mode) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0057 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION_C3
Language=English
PrevEndTS Problem with First diff file after Resync start (in Data File mode) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0058 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_INFO_FIRSTFILE_VALIDATION_C4
Language=English
First diff file after Resync start (Not in Data File mode) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0059 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION_C5
Language=English
Start-End TS Problem with First diff file after Resync start (Not in Data File mode) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0060 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_FIRSTFILE_VALIDATION_C6
Language=English
PrevEndTS Problem with First diff file after Resync start (Not in Data File mode) commited, Volume name: %2, PrevEndTS: %3, ResyncStartTS: %4, CurrentEndTS: %5, CurrentStartTS: %6.
.

MessageId=0x0061 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED
Language=English
INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED for volume %2 (GUID = %3), Total count = %4, Success Count = %5.
.

MessageId=0x0062 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED
Language=English
Boundary validation failed for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Success Count = %5.
.

MessageId=0x0063 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED2
Language=English
Input sector-aligned validation failed for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Status = %5.
.

MessageId=0x0064 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3
Language=English
Input no-overlap validation failed for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Status = %5.
.

MessageId=0x0065 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_ERROR
Language=English
Error tracking data in metadata mode for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Success Count = %5.
.

MessageId=0x0066 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_INFO
Language=English
DataLength greater than MaxUlong for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Success Count = %5.
.

MessageId=0x0067 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_ERROR2
Language=English
Assignment error for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Success Count = %5.
.

MessageId=0x0068 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_TAGINFO
Language=English
At time of Tag Issue stats for INVOLFLT_IOCTL_DISK_COPY_DATA for volume %2 (GUID = %3), Total count = %4, Success Count = %5.
.

MessageId=0x0069 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_PERSISTENT_THREAD_REGISTRY_WRITE_FAILURE
Language=English
Registry write failed with Status %2, either at the time of shutdown(Expected but not an alarm) 
or on persistent thread time-out
.

MessageId=0x006A Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_PERSISTENT_THREAD_REGISTRY_FLUSH_FAILURE
Language=English
Registry Flush failed with Status %2, either at the time of shutdown or on persistent thread time-out
.

MessageId=0x006B Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_READ_LAST_PERSISTENT_VALUE_FAILURE
Language=English
Reading Registry Key LastPersistentValues failed with Status %2 at the time of loading driver
.
MessageId=0x006C Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_LOG_SAME_TAG_IN_SEQ
Language=English
Same Tag in sequence for Volume with DriveLetter (%2 ) Total Tag IOCTLs = %3 Tags in DataMode = %4 Tags in NonDataMode = %5 dropped = %6
.
MessageId=0x006D Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_ERR_ENUMERATE_DEVICE
Language=English
Enumerating volumeclassguid devices failed with status %2
.
MessageId=0x006E Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_GET_ADDITIONAL_STATS_FAIL1
Language=English
GetAdditionalIOCTL returned wrong value with End TS : %2 FirstDB TS %3 VC WoState %4 Pending DB WO state %5
.
MessageId=0x006F Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_GET_ADDITIONAL_STATS_FAIL2
Language=English
GetAdditionalIOCTL returned wrong value with End TS : %2 FirstDB TS %3 VC WoState %4  %5
.
MessageId=0x0070 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_BITMAPSTATE1
Language=English
Bitmap CommitHeader failed, Current state %2
.
MessageId=0x0071 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_BITMAPSTATE2 
Language=English
GetFirstRuns Unexpected Bitmap state, Current state %2
.
MessageId=0x0072 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_BITMAPSTATE3 
Language=English
GetNextRuns Unexpected Bitmap state, Current state %2
.
MessageId=0x0073 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_BITMAPSTATE4 
Language=English
ChangeBitmapModeToRawIo : Unexpected Bitmap State, Current state %2
.
MessageId=0x0074 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_BITMAPSTATE5 
Language=English
SaveWriteMetaDataToBitmap : Unexpected Bitmap State, Current state %2
.
MessageId=0x0075 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_WARNING_BITMAPSTATE6 
Language=English
BitmapOpen : Setting Bitmap state to Uninitialized state Status1 : %2 Status2 : %3
.
MessageId=0x0076 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_ERR_VOLUME_WRITE_PAST_EOV1
Language=English
A write attempt past the end of the volume was detected on volume %2 (GUID = %3). This may
indicate the volume has dynamically been grown. Driver known size is %4. 
.
MessageId=0x0077 Facility=InVolFlt Severity=Informational SymbolicName=INVOLFLT_START_FILTERING_BEFORE_BITMAP_OPEN
Language=English
A start filtering IOCTL is seen from user space before opening the bitmap on volume %2 (GUID = %3) at TimeStamp %4
.
MessageId=0x0078 Facility=InVolFlt Severity=Warning SymbolicName=INVOLFLT_FILE_CLOSE_ON_BITMAP_WRITE_COMPLETION
Language=English
On Bitmap write completion, State of Bitmap file is set to Close for volume %2 (GUID = %3) 
.
MessageId=0x0079 Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_FS_WRITE_ON_BITMAP_HEADER_FAIL
Language=English
writing m_iobBitmapHeader with FS to volume failed with Status %2
.
MessageId=0x007A Facility=InVolFlt Severity=Error SymbolicName=INVOLFLT_RAW_WRITE_ON_BITMAP_HEADER_FAIL
Language=English
Writing m_iobBitmapHeader in raw mode failed with Status %2
.
;#endif /* _InVolFlt_mc_ */


