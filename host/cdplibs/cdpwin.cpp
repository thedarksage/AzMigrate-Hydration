//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpwin.cpp
//
// Description: 
// This file contains windows specific implementation for routines
// defined in cdpplatform.h
//

#ifdef WIN32
#include "localconfigurator.h"

#include "cdpplatform.h"
#include "cdpglobals.h"
#include "error.h"
#include "portable.h"

#include "globs.h"
#include "cdpdrtd.h"
#include <windows.h>
#include <atlbase.h>
#include <winioctl.h>

#include "VVDevControl.h"
#include "vsnapuser.h"
#include "hostagenthelpers.h"
#include "portablehelpersmajor.h"
#include "InstallInVirVol.h"
#include <windows.h>
#include <Tlhelp32.h>
#include <psapi.h>

#include <ace/OS_NS_unistd.h>
#include <ace/Process_Manager.h>

#include <sstream>
#include "inmsafecapis.h"

using namespace std;

struct ProcessBasicInformation {
	DWORD ExitStatus;
	PVOID PebBaseAddress;
	DWORD AffinityMask;
	DWORD BasePriority;
	ULONG UniqueProcessId;
	ULONG InheritedFromUniqueProcessId;
};

bool GetServiceState(const std::string & ServiceName, SV_ULONG & state)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	do
	{
		// Open the SCM and the desired service.
		SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if(!hSCM)
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< " Error Message: " << Error::Msg() << "\n";

			DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
			rv = false;
			break;
		}

		// PR#10815: Long Path support
		SC_HANDLE hService = SVOpenService(hSCM, ServiceName.c_str(), SERVICE_QUERY_STATUS );
		if(!hService)
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< " Error Message: " << Error::Msg() << "\n";

			DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
			rv = false;

			CloseServiceHandle(hSCM);
			break;
		}

		SERVICE_STATUS ss;
		int fService = QueryServiceStatus(hService, &ss);

		if(!fService)
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< " Error Message: " << Error::Msg() << "\n";

			DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
			rv = false;

			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			break;
		}

		state = ss.dwCurrentState;

		rv = true;

	} while (FALSE);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool GetDiskFreeCapacity(const string & dir, SV_ULONGLONG & freespace)
{
	ULARGE_INTEGER ulTotalNumberOfFreeBytes;
	// see PR # 6869
	// PR#10815: Long Path support
	if ( !SVGetDiskFreeSpaceEx(dir.c_str(),&ulTotalNumberOfFreeBytes,NULL,NULL) )
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< " Error: unable to calculate free space under " << dir << "\n"
			<< USER_DEFAULT_ACTION_MSG << "\n";

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	freespace =  ulTotalNumberOfFreeBytes.QuadPart;
	return true;
}

bool OpenVsnapControlDevice(ACE_HANDLE &CtrlDevice)
{
	HANDLE VVDev = INVALID_HANDLE_VALUE;

	// PR#10815: Long Path support
	VVDev= SVCreateFile ( VV_CONTROL_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		NULL, NULL);

	if(INVALID_HANDLE_VALUE == VVDev)
		return false;
	else
	{
		CtrlDevice = VVDev;
		return true;
	}
}

long OpenInVirVolDriver(ACE_HANDLE &hDevice)
{
	DRSTATUS Status = DRSTATUS_SUCCESS;
	if(!OpenVsnapControlDevice(hDevice))
	{

		INSTALL_INVIRVOL_DATA InstallData;
		InstallData.DriverName = _T("invirvol");
		char SystemDir[128];
		GetSystemDirectory(SystemDir, 128);

		const size_t PATH_AND_FILE_NAME_LEN = 128;
		InstallData.PathAndFileName = new TCHAR[PATH_AND_FILE_NAME_LEN];
		inm_tcscpy_s(InstallData.PathAndFileName, PATH_AND_FILE_NAME_LEN, SystemDir);
		inm_tcscat_s(InstallData.PathAndFileName, PATH_AND_FILE_NAME_LEN, _T("\\drivers\\invirvol.sys"));
		Status = InstallInVirVolDriver(InstallData);

		if(!DR_SUCCESS(Status))
		{
			return Status;
		}

		START_INVIRVOL_DATA StartData;
		StartData.DriverName = _T("invirvol");

		Status = StartInVirVolDriver(StartData);
		if(!DR_SUCCESS(Status))
		{
			return Status;
		}

		if(!OpenVsnapControlDevice(hDevice))
		{
			Status = DRSTATUS_UNSUCCESSFUL;
			return Status;
		}
	}

	return Status;
}

void CloseVVControlDevice(ACE_HANDLE &CtrlDevice)
{
	if(CtrlDevice!=NULL)
		CloseHandle((HANDLE)CtrlDevice);
}

int IssueMapUpdateIoctlToVVDriver(ACE_HANDLE& CtrlDevice, 
								  const char *VolumeName, 
								  const char *DataFileName,
								  const SV_UINT & length,
								  const SV_OFFSET_TYPE & volume_offset,
								  const SV_OFFSET_TYPE & file_offset,
								  bool Cow)
{
	DWORD dwReturn = 0;
	int bResult;

	UPDATE_VSNAP_VOLUME_INPUT LockInput;
	inm_strcpy_s(LockInput.ParentVolumeName, ARRAYSIZE(LockInput.ParentVolumeName), VolumeName);
	inm_strcpy_s(LockInput.DataFileName, ARRAYSIZE(LockInput.DataFileName), DataFileName);
	LockInput.VolOffset = volume_offset;
	LockInput.Length = length;
	LockInput.FileOffset = file_offset;
	LockInput.Cow = Cow;

	//	DebugPrintf(SV_LOG_DEBUG, "TargetVolume %s, DataFile %s, VolOff %I64u, Length %lu, FileOff %I64u\n", LockInput.TargetVolume,
	//				LockInput.DataFileName, LockInput.VolOffset, LockInput.Length, LockInput.FileOffset);

	bResult = DeviceIoControl( CtrlDevice, (DWORD)IOCTL_INMAGE_UPDATE_VSNAP_VOLUME, &LockInput, sizeof(UPDATE_VSNAP_VOLUME_INPUT), 
		NULL, 0, &dwReturn, NULL ); 

	return bResult;
}


void TerminateProcess(ACE_Process_Manager* pm, pid_t pid)
{   
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == snapshot) {      
		return;
	}

	PROCESSENTRY32 processEntry;

	if (Process32Next(snapshot, &processEntry)) {
		do {
			if (processEntry.th32ParentProcessID == pid) {
				HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processEntry.th32ProcessID);
				if (INVALID_HANDLE_VALUE == process) {
					break;
				}                                
				TerminateProcess(process, 0);
				break;
			}
		} while(Process32Next(snapshot, &processEntry));
	}

	CloseHandle(snapshot);

	pm->terminate(pid);
}

void GetAgentInstallPath(string & AgentPath)
{
	CRegKey cregkey;
	DWORD result;

	result = cregkey.Open(HKEY_LOCAL_MACHINE, SV_VXAGENT_VALUE_NAME);
	if (ERROR_SUCCESS != result)
		return;

	char path[ 2048 ];
	DWORD dwCount = sizeof( path );
	result = cregkey.QueryStringValue(SV_AGENT_INSTALL_LOCATON_VALUE_NAME, path, &dwCount);
	if (ERROR_SUCCESS != result) 
		return;

	AgentPath = path;
	AgentPath += '\\';

	return;
}

bool GetDiskFreeSpaceP(const string & dir,SV_ULONGLONG *quota,SV_ULONGLONG *capacity,SV_ULONGLONG *ulTotalNumberOfFreeBytes)
{
	ULARGE_INTEGER userquota = {0};
	ULARGE_INTEGER totalcapacity = {0};
	ULARGE_INTEGER freeSpace = {0};

	// PR#10815: Long Path support
	if (!SVGetDiskFreeSpaceEx(dir.c_str(), &userquota, &totalcapacity, &freeSpace)) {
		DebugPrintf(SV_LOG_ERROR, "FAILED GetDiskFreeSpace for %s: %d\n",dir.c_str(), GetLastError());
		return false;
	}

	if( NULL != capacity )
		*capacity = totalcapacity.QuadPart;

	if( NULL != quota )
		*quota = userquota.QuadPart;

	// see PR # 6869
	if( NULL != ulTotalNumberOfFreeBytes )
		*ulTotalNumberOfFreeBytes = userquota.QuadPart;  

	return true;
}

std::string getParentProccessName()
{
	std::string procName;
	ULONG procID = 	(ULONG) ACE_OS::getpid();;
	ULONG ppid;
	HANDLE hProc1 = NULL;
	struct ProcessBasicInformation sProcessInfo;

	typedef LONG ( __stdcall *FPTR_NtQueryInformationProcess ) ( HANDLE, INT, PVOID, ULONG, PULONG );
	FPTR_NtQueryInformationProcess NtQueryInformationProcess = ( FPTR_NtQueryInformationProcess ) GetProcAddress( GetModuleHandle( "ntdll" ), "NtQueryInformationProcess" );

	hProc1 = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, procID );
	if( hProc1 == NULL ) {
		return( "EMPTY" );
	}

	if( NtQueryInformationProcess( hProc1, 0, (void *) &sProcessInfo, sizeof( sProcessInfo ), NULL ) != 0 ) {
		CloseHandle( hProc1 );
		return( "EMPTY" );
	}

	ppid  = sProcessInfo.InheritedFromUniqueProcessId;
	CloseHandle( hProc1 );

	HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ,
		FALSE, ppid );
	// Get the process name.
	char szProcessName[MAX_PATH];
	memset(szProcessName,'\0',260);

	if ( hProcess )
	{
		HMODULE hMod;
		DWORD cbNeeded;

		if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod),&cbNeeded) )
		{
			GetModuleBaseName( hProcess, hMod, szProcessName,sizeof(szProcessName)/sizeof(TCHAR) );
		}
		CloseHandle( hProcess );
	}
	if(szProcessName[0] == '\0')
	{
		procName = "EMPTY";
	}
	else
	{
		procName = szProcessName;
	}
	return procName;
}
#endif
