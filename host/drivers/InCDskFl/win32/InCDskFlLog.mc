;/*
;
;Module Name:
;
;    InCDskFlLog.mc
;
;Abstract:
;
;    Constant definitions for the I/O error code log values.
;
;Revision History:
;
;--*/
;
;#ifndef _InCDskFlLog_mc_
;#define _InCDskFlLog_mc_
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

SeverityNames = (
	Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
)
;// facility codes 0-0xff are reserved for Microsoft
FacilityNames = (
	System=0x0
    RpcRuntime=0x2:FACILITY_RPC_RUNTIME
    RpcStubs=0x3:FACILITY_RPC_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
    InCDskFl=0x113:FACILITY_INCDSKFL_ERROR_CODE
)
;// The sources file should set the "-c" message compiler (MC_OPTIONS=-c) option to turn on the customer bit in codes

MessageId=0x0001 Facility=InCDskFl Severity=Informational SymbolicName=INCDSKFL_DRIVER_IN_BYPASS_MODE_DUE_TO_BOOT_INI
Language=English
Found bypass setting in BOOT.INI for InCDskFl driver. InCDskFl is in bypass mode.
.

MessageId=0x0002 Facility=InCDskFl Severity=Informational SymbolicName=INCDSKFL_DRIVER_IN_BYPASS_MODE_NO_INVOLFLT
Language=English
InVolFlt driver not found, InCDskFl driver is in bypass mode
.

MessageId=0x0003 Facility=InCDskFl Severity=Informational SymbolicName=INCDSKFL_DRIVER_IN_BYPASS_MODE_NO_CLUSDISK
Language=English
ClusDisk driver not found, InCDskFl driver is in bypass mode
.
MessageId=0x0004 Facility=InCDskFl Severity=Error SymbolicName=INCDSKFL_DRIVER_LOAD_FAILURE
Language=English
InCDskFl driver is failed to load at the time of initialization of driver context. See the binary data for exact error status.
.
;#endif

