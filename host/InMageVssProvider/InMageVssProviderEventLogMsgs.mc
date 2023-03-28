;#ifndef _INMAGE_VSS_PROVIDER_EVENTLOGMSGS_H_
;#define _INMAGE_VSS_PROVIDER_EVENTLOGMSGS_H_

MessageIdTypedef=DWORD
SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )
              
FacilityNames=(System=0x0
               RpcRuntime=0x2:FACILITY_RPC_RUNTIME
               RpcStubs=0x3:FACILITY_RPC_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
              )

MessageId=0x0001 Facility=InMageVssProvider Severity=Information SymbolicName=INMAGE_SWPRV_EVENTLOG_INFO_GENERIC_MESSAGE
Language=English 
%1
.

;#endif // _INMAGE_VSS_PROVIDER_EVENTLOGMSGS_H_

