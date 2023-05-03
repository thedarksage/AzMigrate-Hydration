;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;
;Module Name:
;
;    InVirVol.mc
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Revision History:
;
;--*/
;
;#ifndef _InVirVol_mc_
;#define _InVirVol_mc_
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
               InVirVol=0x114:FACILITY_INVIRVOL_ERROR_CODE
              )
;// The sources file should set the "-c" message compiler (MC_OPTIONS=-c) option to turn on the customer bit in codes
MessageId=0x0001 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_DATA_FILE_OPEN_FAILED
Language=English
Opening Data File %3 is failed for Vsnap id %4 (GUID : %2)
.
MessageId=0x0002 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_WRITE_TO_DATA_FILE_FAILED
Language=English
Write to Data file with File Id %3 failed with Status %5  for Vsnap Id %4 (GUID : %2)
.
MessageId=0x0003 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_READ_FROM_DATA_FILE_FAILED
Language=English
Read from Data file with File id %3 failed with Status %5 for Vsnap Id %4 (GUID : %2)
.
MessageId=0x0004 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_ERR_GET_FILENAME_FROM_FILEID
Language=English
Get FileName for File Id %3 failed with Status %5 for Vsnap Id %4 (GUID : %2)
.
MessageId=0x0005 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_ERR_TARGET_READ_FAILED
Language=English
Target Read failed with Status %2
.
MessageId=0x0006 Facility=InVirVol Severity=Informational SymbolicName=INVIRVOL_VSNAP_MOUNT_SUCCESS
Language=English
Vsnap with GUID %2 mounted successfully
.
MessageId=0x0007 Facility=InVirVol Severity=Informational SymbolicName=INVIRVOL_VSNAP_DEVICE_NAME_SUCCESS
Language=English
Vsnap with GUID %2 has got DeviceName %3 successfuly
.
MessageId=0x0008 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_VSNAP_WRITE_TO_FILEID_TABLE_FAILED
Language=English
Adding File Name %3 failed for Vsnap Id  %4 (GUID : %2)
.
MessageId=0x0009 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_VSNAP_FILEID_TABLE_MISMATCH
Language=English
Mismatch between File Id and respective FileSize while adding FileName %3 for Vsnap Id %4 (GUID : %2)
.
MessageId=0x000A Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_ERR_TOO_MANY_EVENT_LOG_EVENTS
Language=English
The driver has written too many events to the system event log recently. Events will be discarded for 
the next time interval.
.
MessageId=0x000B Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_OFFSET_BEYOND_FILE_SIZE
Language=English
FileOffset %3 is beyond the EOF offset %5  for Vsnap Id %4 (GUID : %2). Refer Message prior to this to know FileName by looking at File Id.
.
MessageId=0x000C Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_OFFSET_READ_FAILURE
Language=English
Reading data size of %4 failed for GUID : %2 at offset %3. Refer message prior to this for more details.
.
MessageId=0x000D Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_VSNAP_MOUNT_FAILURE
Language=English
Vsnap Mount failed with Status %2
.
MessageId=0x000E Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_VOLPACK_READ_FAILURE
Language=English
Reading data size of %4 failed for GUID : %2 at offset %3 for Vsnap Id %5.
.
MessageId=0x000F Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_VOLPACK_OPEN_FILE_FAILURE
Language=English
Opening VolPack target File %2 is failed with Status %3
.
MessageId=0x0010 Facility=InVirVol Severity=Error SymbolicName=INVIRVOL_OPEN_FILE_FAILURE
Language=English
Opening File %2 is failed with Status %3. FileName is manipulated due to max limit of event log buffer
.
MessageId=0x0011 Facility=InVirVol Severity=Informational SymbolicName=INVIRVOL_IMPERSONATION_STATUS
Language=English
Impersonation Status %2 returned
.
;#endif /* _InVirVol_mc_ */



