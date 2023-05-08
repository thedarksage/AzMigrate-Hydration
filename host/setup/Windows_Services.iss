[Code]
type
	SERVICE_STATUS = record
    	dwServiceType				: cardinal;
    	dwCurrentState				: cardinal;
    	dwControlsAccepted			: cardinal;
    	dwWin32ExitCode				: cardinal;
    	dwServiceSpecificExitCode	: cardinal;
    	dwCheckPoint				: cardinal;
    	dwWaitHint					: cardinal;
	end;
	HANDLE = cardinal;

	GUID = record
	   data : array [1..16] of AnsiChar;
	end;

const
	SERVICE_QUERY_CONFIG		= $1;
	SERVICE_CHANGE_CONFIG		= $2;
	SERVICE_QUERY_STATUS		= $4;
	SERVICE_START				= $10;
	SERVICE_STOP				= $20;
	SERVICE_ALL_ACCESS			= $f01ff;
	SC_MANAGER_ALL_ACCESS		= $f003f;
	SERVICE_WIN32_OWN_PROCESS	= $10;
	SERVICE_WIN32_SHARE_PROCESS	= $20;
	SERVICE_WIN32				= $30;
	SERVICE_INTERACTIVE_PROCESS = $100;
	SERVICE_BOOT_START          = $0;
	SERVICE_SYSTEM_START        = $1;
	SERVICE_AUTO_START          = $2;
	SERVICE_DEMAND_START        = $3;
	SERVICE_DISABLED            = $4;
	SERVICE_DELETE              = $10000;
	SERVICE_CONTROL_STOP		= $1;
	SERVICE_CONTROL_PAUSE		= $2;
	SERVICE_CONTROL_CONTINUE	= $3;
	SERVICE_CONTROL_INTERROGATE = $4;
	SERVICE_STOPPED				= $1;
	SERVICE_START_PENDING       = $2;
	SERVICE_STOP_PENDING        = $3;
	SERVICE_RUNNING             = $4;
	SERVICE_CONTINUE_PENDING    = $5;
	SERVICE_PAUSE_PENDING       = $6;
	SERVICE_PAUSED              = $7;
  SERVICE_NO_CHANGE           = $ffffffff;


// Start, stop and status of services.

function OpenSCManager(lpMachineName, lpDatabaseName: AnsiString; dwDesiredAccess :cardinal): HANDLE;
external 'OpenSCManagerA@advapi32.dll stdcall';

function OpenService(hSCManager :HANDLE;lpServiceName: AnsiString; dwDesiredAccess :cardinal): HANDLE;
external 'OpenServiceA@advapi32.dll stdcall';

function CloseServiceHandle(hSCObject :HANDLE): boolean;
external 'CloseServiceHandle@advapi32.dll stdcall';

function CreateService(hSCManager :HANDLE;lpServiceName, lpDisplayName: AnsiString;dwDesiredAccess,dwServiceType,dwStartType,dwErrorControl: cardinal;lpBinaryPathName,lpLoadOrderGroup: AnsiString; lpdwTagId : cardinal;lpDependencies,lpServiceStartName,lpPassword :AnsiString): cardinal;
external 'CreateServiceA@advapi32.dll stdcall';

function DeleteService(hService :HANDLE): boolean;
external 'DeleteService@advapi32.dll stdcall';

function StartNTService(hService :HANDLE;dwNumServiceArgs : cardinal;lpServiceArgVectors : cardinal) : boolean;
external 'StartServiceA@advapi32.dll stdcall';

function ControlService(hService :HANDLE; dwControl :cardinal;var ServiceStatus :SERVICE_STATUS) : boolean;
external 'ControlService@advapi32.dll stdcall';

function QueryServiceStatus(hService :HANDLE;var ServiceStatus :SERVICE_STATUS) : boolean;
external 'QueryServiceStatus@advapi32.dll stdcall';

function QueryServiceStatusEx(hService :HANDLE;ServiceStatus :SERVICE_STATUS) : boolean;
external 'QueryServiceStatus@advapi32.dll stdcall';

function ChangeServiceConfig( hService: HANDLE; dwServiceType: cardinal; dwStartType: cardinal; dwErrorControl: cardinal; pszBinaryPathname: cardinal; pszLoadOrderGroup: cardinal; dwTagId: cardinal; pszDependencies: cardinal; pszUserName: AnsiString; pszPassword: AnsiString; pszDisplayName: cardinal ): Boolean;
external 'ChangeServiceConfigA@advapi32.dll stdcall';

function UuidCreate( var guid: GUID ): Integer;
external 'UuidCreate@rpcrt4.dll stdcall';

function CharToHex( ch: Char ): AnsiString;
var
  val: Byte;
  digits: AnsiString;
begin
  digits := '0123456789ABCDEF';
  val := Ord( ch );
  Result := digits[ 1 + val / 16 ] + digits[ 1 + val mod 16 ];
end;

function UuidToString( guid: GUID ): AnsiString;
begin
  Result :=
    CharToHex(guid.data[1]) + CharToHex(guid.data[2]) + CharToHex(guid.data[3]) + CharToHex(guid.data[4])
  + '-'
  + CharToHex(guid.data[5]) + CharToHex(guid.data[6])
  + '-'
  + CharToHex(guid.data[7]) + CharToHex(guid.data[8])
  + '-'
  + CharToHex(guid.data[9]) + CharToHex(guid.data[10]) + CharToHex(guid.data[11]) + CharToHex(guid.data[12])
  + CharToHex(guid.data[13]) + CharToHex(guid.data[14]) + CharToHex(guid.data[15]) + CharToHex(guid.data[16])
  ;
end;

function GetHostId( Param: AnsiString ): AnsiString;
var
  guid: GUID;
  rc: Integer;
begin
    rc := UuidCreate( guid );
    if rc < 0 then
        SuppressibleMsgBox('Could not create host id guid, rc='+IntToStr(rc),mbInformation,MB_OK, MB_OK)
    else
      Result := UuidToString( guid )
end;

//Used to open a service.
function OpenServiceManager() : HANDLE;
begin
	//Windows NT based systems support
	if UsingWinNT() = true then 
	begin
		Result := OpenSCManager('','ServicesActive',SC_MANAGER_ALL_ACCESS);
		if Result = 0 then
			SuppressibleMsgBox('The Service manager is not available.', mbError, MB_OK, MB_OK)
	end
	else 
	begin
			SuppressibleMsgBox('Only Windows NT based systems support services.', mbError, MB_OK, MB_OK)
			Result := 0;
	end
end;

//Used to check, whether service is installed or not.
function IsServiceInstalled(ServiceName: AnsiString) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then 
	begin
		hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_CONFIG);
        if hService <> 0 then 
        begin
            Result := true;
            CloseServiceHandle(hService)
		    end;
        CloseServiceHandle(hSCM)
	end
end;


//Used to install the service
function InstallService(FileName, ServiceName, DisplayName, Description : AnsiString;ServiceType,StartType :cardinal) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then 
	begin
		hService := CreateService(hSCM,ServiceName,DisplayName,SERVICE_ALL_ACCESS,ServiceType,StartType,0,FileName,'',0,'','','');
		if hService <> 0 then 
		begin
			Result := true;
			if Description<> '' then
				RegWriteStringValue(HKLM,'System\CurrentControlSet\Services\' + ServiceName,'Description',Description);
			CloseServiceHandle(hService)
		end;
        CloseServiceHandle(hSCM)
	end
end;

//Used to remove the service
function RemoveService(ServiceName: AnsiString) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then 
	begin
		hService := OpenService(hSCM,ServiceName,SERVICE_DELETE);
        if hService <> 0 then 
        begin
            Result := DeleteService(hService);
            CloseServiceHandle(hService)
		    end;
        CloseServiceHandle(hSCM)
	end
end;

//Used to start the service.
function StartService(ServiceName: AnsiString) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then 
	begin
		hService := OpenService(hSCM,ServiceName,SERVICE_START);
        if hService <> 0 then 
        begin
        	Result := StartNTService(hService,0,0);
            CloseServiceHandle(hService)
		    end;
        CloseServiceHandle(hSCM)
	end;
end;

//Used to stop the service.
function StopService(ServiceName: AnsiString) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
	Status	: SERVICE_STATUS;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then 
	begin
		hService := OpenService(hSCM,ServiceName,SERVICE_STOP);
        if hService <> 0 then 
        begin
        	Result := ControlService(hService,SERVICE_CONTROL_STOP,Status);
            CloseServiceHandle(hService)
		    end;
        CloseServiceHandle(hSCM)
	end;
end;

//Used to check, whether service is running or not.
function IsServiceRunning(ServiceName: AnsiString) : boolean;
var
	hSCM	: HANDLE;
	hService: HANDLE;
	Status	: SERVICE_STATUS;
begin
	hSCM := OpenServiceManager();
	Result := false;
	if hSCM <> 0 then 
	begin
		hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_STATUS);
    	if hService <> 0 then 
    	begin
    			if QueryServiceStatus(hService,Status) then 
    			begin
    				Result :=(Status.dwCurrentState = SERVICE_RUNNING)
          end;
            CloseServiceHandle(hService)
		  end;
        CloseServiceHandle(hSCM)
	end
end;

//Used to check the service status.
function ServiceStatusString(ServiceName: AnsiString) : AnsiString;
var
	hSCM	: HANDLE;
	hService: HANDLE;
	Status	: SERVICE_STATUS;
begin
	hSCM := OpenServiceManager();
	if hSCM <> 0 then
	begin
		hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_STATUS);
    	if hService <> 0 then
    	begin
			if QueryServiceStatus(hService,Status) then begin
				case Status.dwCurrentState of
					SERVICE_RUNNING: Result := 'SERVICE_RUNNING';
					SERVICE_START_PENDING: Result := 'SERVICE_START_PENDING';
					SERVICE_STOP_PENDING: Result := 'SERVICE_STOP_PENDING';
					SERVICE_CONTINUE_PENDING: Result := 'SERVICE_CONTINUE_PENDING';
					SERVICE_PAUSE_PENDING: Result := 'SERVICE_PAUSE_PENDING';
					SERVICE_STOPPED: Result := 'SERVICE_STOPPED';
					SERVICE_PAUSED: Result := 'SERVICE_PAUSED';
					SERVICE_DISABLED: Result := 'SERVICE_DISABLED';
				end;
        	end;
            CloseServiceHandle(hService)
		end;
        CloseServiceHandle(hSCM)
	end
end;

//Used to change the Username and password for particular service.
function ChangeServiceUser( ServiceName, UserName, Password: AnsiString ): boolean;
var
  hSCM: HANDLE;
  hService: HANDLE;
begin
  Result := False;
  hSCM := OpenServiceManager();
  if hSCM <> 0 then 
  begin
    hService := OpenService( hSCM, ServiceName, SERVICE_CHANGE_CONFIG );
    if hService <> 0 then 
    begin
      Result := ChangeServiceConfig( hService, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, 0, 0, 0, 0, UserName, Password, 0 );
      CloseServiceHandle( hService );
    end;
    CloseServiceHandle( hSCM );
  end
end;
