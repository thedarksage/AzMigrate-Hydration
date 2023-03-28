
#ifdef SV_WINDOWS
#include "logger.h"
#include "portable.h"
#include "servicemajor.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"

WinService::WinService() : servicetitle(NULL), servicename(NULL), servicedesc(NULL), quit(0),m_hServiceHandle(NULL), m_bVolumeMonitorStarted(false)
{


    HRESULT hr;
	m_jobObjectHandle = NULL;

	do
	{
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

		if (hr != S_OK)
		{
			DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
			DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
			break;
		}

		hr = CoInitializeSecurity
			(
			NULL,                                //  IN PSECURITY_DESCRIPTOR         pSecDesc,
			-1,                                  //  IN LONG                         cAuthSvc,
			NULL,                                //  IN SOLE_AUTHENTICATION_SERVICE *asAuthSvc,
			NULL,                                //  IN void                        *pReserved1,
			RPC_C_AUTHN_LEVEL_CONNECT,           //  IN DWORD                        dwAuthnLevel,
			RPC_C_IMP_LEVEL_IMPERSONATE,         //  IN DWORD                        dwImpLevel,
			NULL,                                //  IN void                        *pAuthList,
			EOAC_NONE,                           //  IN DWORD                        dwCapabilities,
			NULL                                 //  IN void                        *pReserved3
			);
	

		// Check if the securiy has already been initialized by someone in the same process.
		if (hr == RPC_E_TOO_LATE)
		{
			hr = S_OK;
		}

		if (hr != S_OK)
		{
			DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
			DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
			CoUninitialize();
			break;
		}
	} while(false);

}

WinService::~WinService()
{
    stopVolumeMonitor();
    stopClusterMonitor();
   
    if ( NULL != m_hServiceHandle )
        ::CloseServiceHandle(m_hServiceHandle);

	if (servicetitle != NULL)
	{
		delete[] servicetitle;
		servicetitle = NULL;
	}
	if (&WinService::name != NULL)
	{
		delete[] servicename;
		servicename = NULL;
	}
	if (&ACE_NT_Service::desc != NULL)
	{
		delete[] servicedesc;
		servicedesc = NULL;
	}

	closeJobObjectHandle();
	
	CoUninitialize();
}

ACE_NT_SERVICE_REFERENCE(InMageAgent)

int WinService::install_service()
{
   int status = 0;
   
   status = this->insert(SERVICE_AUTO_START);
   
   if(this->servicedesc != NULL && this->servicename != NULL)
	   ChangeServiceDescription();

   if (this->servicename != NULL)
	   SetServiceRecoveryOptions();

   return status;
}

int WinService::remove_service()
{	
	this->stop_service();
	return this->remove();
}

int WinService::start_service()
{
	return this->start_svc();
}

int WinService::stop_service()
{
	return this->stop_svc();
}

const std::string WinService::GetVolumesOnlineAsString()
{
    return m_VolumeMonitor.GetVolumesOnlineAsString();
}

void WinService::name(const ACE_TCHAR *svcname, const ACE_TCHAR *svctitle, const ACE_TCHAR *svcdesc)
{
	this->ACE_NT_Service::name(svcname, svctitle);

	delete[] this->servicetitle;
	delete[] this->servicename;
	delete[] this->servicedesc;

	this->servicename = ACE::strnew(svcname);
	this->servicetitle = ACE::strnew(svctitle);
	this->servicedesc = ACE::strnew(svcdesc);	 
}

void WinService::SetServiceRecoveryOptions()
{

	SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCM != NULL)
	{
		SC_HANDLE hService = ::OpenServiceW(hSCM, this->servicename, SERVICE_ALL_ACCESS);
		if (hService != NULL)
		{
			DWORD dwRet = SetServiceFailureActionsRecomended(hService);
			if (ERROR_SUCCESS != dwRet)
			{
				DebugPrintf(SV_LOG_ERROR, "FAILED: Set service recovery options for the service - %s. Error = %lu \n.", this->servicename, dwRet);
			}
			::CloseServiceHandle(hService);
		}
		::CloseServiceHandle(hSCM);
	}
}

void WinService::ChangeServiceDescription()
{

    SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCM != NULL)
    {
		// PR#10815: Long Path support
        SC_HANDLE hService = ::OpenServiceW(hSCM, this->servicename, SERVICE_ALL_ACCESS);
        if (hService != NULL)
        {
			SERVICE_DESCRIPTIONW ServiceDescription = { this->servicedesc };
			if( !ChangeServiceConfig2W( hService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription ) )
			{
				//
				// Ignore failure on setting the description: not a critical error
				//
			}
            ::CloseServiceHandle(hService);
        }
        ::CloseServiceHandle(hSCM);
    }
}

int WinService::run_startup_code()
{
	//__asm int 3;

    // PR #2094
	// SetErrorMode controls  whether the process will handle specified 
	// type of errors or whether the system will handle them
	// by creating a dialog box and notifying the user.
	// In our case, we want all errors to be sent to the program.
	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);

	//Register with Service Control Manager/Dispatcher
	ACE_NT_SERVICE_RUN (InMageAgent, SERVICE::instance (), ret);
    
	// If we could not able to register, do nothing
	if (ret == 0)
	{
		//printf("Could not start service \n");
		return -1;
	}

	

	return 0;
}

void WinService::InitServiceHandle()
{
    SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCM != NULL)
    {
		// PR#10815: Long Path support
        m_hServiceHandle = ::OpenServiceW(hSCM, this->servicename, SERVICE_QUERY_CONFIG);
        if (m_hServiceHandle == NULL)        
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to acquire service handle.\n");
        }
        ::CloseServiceHandle(hSCM);
    }
}

int WinService::svc ()
{  
 //__asm int 3;

  int status = 0;

  this->SetupWinSockDLL();


  report_status (SERVICE_RUNNING);

  if (startfn != NULL) 
  {
	status = (*startfn)(startfn_data);
  }
  
  this->CleanupWinSockDLL();
  return status;
}

void WinService::handle_control (DWORD control_code)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
  if ( (control_code == SERVICE_CONTROL_SHUTDOWN
      || control_code == SERVICE_CONTROL_STOP ) )
    {
        report_status(SERVICE_STOP_PENDING, 75000 );
        if (stopfn != NULL) 
            (void)(*stopfn)(stopfn_data);	           
    }
  else
  {
      inherited::handle_control (control_code);
  }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool WinService::isInstalled()
{
	return(this->test_access() == 0);
}

int WinService::RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data)
{
	startfn = fn;
	startfn_data = fn_data;
	return 0;
}

int WinService::RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data)
{
	stopfn = fn;
	stopfn_data = fn_data;
	return 0;
}

bool WinService::InstallDriver()
{
    bool bInstalledDriver = true;

	CRegKey creg;
    if( ERROR_SUCCESS != creg.Open( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Class\\{71A27CDD-812A-11D0-BEC7-08002BE2092F}" ) )
    {
        DebugPrintf( "FAILED Couldn't install driver, couldn't open volume key, err %d\n", GetLastError() );
        bInstalledDriver = false;
    }
    else
    {
        TCHAR tszOldUpperFilters[ 2048 ] = "";
        ULONG cbBuffer = sizeof( tszOldUpperFilters );
        TCHAR const* UPPER_FILTERS_VALUE_NAME = _T("UpperFilters");

        if( ERROR_SUCCESS != creg.QueryMultiStringValue( UPPER_FILTERS_VALUE_NAME, tszOldUpperFilters, &cbBuffer ) )
        {
            ZeroMemory( tszOldUpperFilters, sizeof( tszOldUpperFilters ) );
        }

        TCHAR tszNewUpperFilters[ 2048 ] = "";
        TCHAR* pszNewUpperFilters = tszNewUpperFilters;
		TCHAR* basePtr = tszNewUpperFilters;
		int size = ARRAYSIZE(tszNewUpperFilters);
        BOOL bFilterPresent = false;

        for( TCHAR* pszOld = tszOldUpperFilters; '\0' != *pszOld; pszOld += _tcslen( pszOld ) + 1 )
        {
            if( 0 != _tcsicmp( pszOld, FILTER_NAME ) )
            {
                //
                // Cannot overflow because new buffer is always at least as big as original buffer
                //
				inm_tcscpy_s(pszNewUpperFilters, (size - (pszNewUpperFilters - basePtr)), pszOld);
                pszNewUpperFilters += _tcslen( pszOld );
                *pszNewUpperFilters = '\0';
                pszNewUpperFilters[ 1 ] = '\0';
                pszNewUpperFilters ++;
				size += _tcslen( pszOld );
            }
            else
            {
                bFilterPresent = true;
            }
        }

        if( !bFilterPresent )
        {
			inm_tcscpy_s(pszNewUpperFilters, (size - (pszNewUpperFilters - basePtr)), FILTER_NAME);
            pszNewUpperFilters += _tcslen( FILTER_NAME );
            *pszNewUpperFilters = '\0';
            pszNewUpperFilters[ 1 ] = '\0';
            pszNewUpperFilters ++;
        }

        if( ERROR_SUCCESS != creg.SetMultiStringValue( UPPER_FILTERS_VALUE_NAME, tszNewUpperFilters ) )
        {
            DebugPrintf( "FAILED Couldn't install driver, write to registry err %d\n", GetLastError() );
            bInstalledDriver = false;
        }
    }

    return( bInstalledDriver );
}

bool WinService::isDriverInstalled() const
{
    bool bDriverInstalled =  false;
    CRegKey creg;
    if( ERROR_SUCCESS == creg.Open( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Class\\{71A27CDD-812A-11D0-BEC7-08002BE2092F}" ) )
    {
        DebugPrintf( SV_LOG_INFO,"driver is installed.\n");
        bDriverInstalled =  true;
        creg.Close();
    }
    else
    {
        DebugPrintf( "FAILED Couldn't uninstall driver, couldn't open volume key, err %d\n", GetLastError() );
        bDriverInstalled = false;
    }
    return bDriverInstalled;
}

bool WinService::UninstallDriver()
{
    if ( !isDriverInstalled() )
        return false;

	bool bUninstalledDriver = true;
    CRegKey creg;
    if( ERROR_SUCCESS != creg.Open( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Class\\{71A27CDD-812A-11D0-BEC7-08002BE2092F}" ) )
    {
        DebugPrintf( "FAILED Couldn't uninstall driver, couldn't open volume key, err %d\n", GetLastError() );
        bUninstalledDriver = false;
    }
    else
    {
        TCHAR tszOldUpperFilters[ 2048 ] = "";
        ULONG cbBuffer = sizeof( tszOldUpperFilters );
        TCHAR const* UPPER_FILTERS_VALUE_NAME = _T("UpperFilters");

        if( ERROR_SUCCESS != creg.QueryMultiStringValue( UPPER_FILTERS_VALUE_NAME, tszOldUpperFilters, &cbBuffer ) )
        {
            DebugPrintf( "WARNING Couldn't uninstall driver, UpperFilters missing? err %d\n", GetLastError() );
        }
        else
        {
            TCHAR tszNewUpperFilters[ 2048 ] = "";
            TCHAR* pszNewUpperFilters = tszNewUpperFilters;
            TCHAR* basePtr = tszNewUpperFilters;
            int size = ARRAYSIZE(tszNewUpperFilters);
            
            for( TCHAR* pszOld = tszOldUpperFilters; '\0' != *pszOld; pszOld += _tcslen( pszOld ) + 1 )
            {
                if( 0 != _tcsicmp( pszOld, FILTER_NAME ) )
                {
                    //
                    // Cannot overflow because new buffer is always at least as big as original buffer
                    //
					inm_tcscpy_s(pszNewUpperFilters, (size - ( pszNewUpperFilters - basePtr)) , pszOld);
                    pszNewUpperFilters += _tcslen( pszOld );
                    *pszNewUpperFilters = '\0';
                    pszNewUpperFilters[ 1 ] = '\0';
                    pszNewUpperFilters ++;
                }
            }
            if( ERROR_SUCCESS != creg.SetMultiStringValue( UPPER_FILTERS_VALUE_NAME, tszNewUpperFilters ) )
            {
                DebugPrintf( "FAILED Couldn't uninstall driver, write to registry err %d\n", GetLastError() );
                bUninstalledDriver = false;
            }
        }
    } 
    return( bUninstalledDriver );
}

bool WinService::startClusterMonitor()
{
    if( ERROR_SUCCESS == m_Cluster.StartMonitor<WinService>( *this, &WinService::ResourceOfflineCallback ) )
	{
	    m_bClusterMonitorStarted = true;
    }
	else
	{
	    m_bClusterMonitorStarted = false;
        DebugPrintf( SV_LOG_ERROR,"FAILED: Couldn't start cluster monitor\n" );
    }
    return m_bClusterMonitorStarted;
}

bool WinService::stopClusterMonitor()
{
    m_Cluster.StopMonitor();
    return true;
}

bool WinService::VolumeAvailableForNode(std::string const & volumeName)
{
    return m_Cluster.VolumeAvailableForNode(volumeName);
}

bool WinService::isClusterVolume(std::string const & volumeName)
{
    return m_Cluster.IsClusterVolume(volumeName);
}

bool WinService::startVolumeMonitor()
{
    if (!m_bVolumeMonitorStarted)
    {
        m_VolumeMonitor.Start();
        m_VolumeMonitor.getVolumeMonitorSignal().connect(this,&WinService::ProcessVolumeMonitorSignal);
        m_VolumeMonitor.getBitlockerMonitorSignal().connect(this, &WinService::ProcessBitlockerMonitorSignal);
        m_bVolumeMonitorStarted = true;
    }

    return true;
}

bool WinService::stopVolumeMonitor()
{
    if (m_bVolumeMonitorStarted)
    {
        m_VolumeMonitor.getBitlockerMonitorSignal().disconnect(this);
        m_VolumeMonitor.getVolumeMonitorSignal().disconnect(this);
        m_VolumeMonitor.Stop();
        m_bVolumeMonitorStarted = false;
    }

    return true;
}

void WinService::SetupWinSockDLL()
{
  //Setup WinSock DLL
  WSADATA WsaData;
  ACE_OS::memset(&WsaData, 0, sizeof(WsaData));

  if( WSAStartup( MAKEWORD( 1, 0 ), &WsaData ) )
  {
	DebugPrintf( "FAILED Couldn't start Winsock, err %d\n", WSAGetLastError() );
  }
}

void WinService::CleanupWinSockDLL()
{
	WSACleanup();
}

void WinService::ProcessVolumeMonitorSignal(void* param)
{
    m_VolumeMonitorSignal(param);
}

void WinService::ProcessBitlockerMonitorSignal(void *param)
{
    m_BitlockerMonitorSignal(param);
}

/*
 * FUNCTION NAME : DeleteAgentQuitEventFiles
 *
 * DESCRIPTION : 
 *  If an agent process crashes or is terminated, memory-mapped files created
 *  to implement the quit event are left behind in a random state (unix only).
 *  We delete these files to prevent future processes from re-using the files.
 *
 * INPUT PARAMETERS : filename - quitevent file name 
 *
 * OUTPUT PARAMETERS :  none
 *
 * NOTES :
 *  On Windows, named events do not create mmap files. Thus, no work to do
 *
 * return value : none
 *
*/
void DeleteAgentQuitEventFiles( std::string const& filename )
{
}


//Creating job object for svagents child processes
bool WinService::InitializeJobObject()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	m_jobObjectHandle = CreateJobObject(NULL, NULL);
	if (m_jobObjectHandle == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "%s: Failed to create WIN32 Job Object.. Error %ld\n", FUNCTION_NAME, GetLastError());
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "%s: created job object successfully\n", FUNCTION_NAME);
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobObjExtendedLimitInfo = { 0 };

		// Configure all child processes associated with the job to terminate when the parents process dies
		jobObjExtendedLimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (0 == SetInformationJobObject(m_jobObjectHandle, JobObjectExtendedLimitInformation, &jobObjExtendedLimitInfo, sizeof(jobObjExtendedLimitInfo)))
		{
			DebugPrintf(SV_LOG_ERROR, "%s: Could not SetInformationJobObject with error = %ld\n", FUNCTION_NAME, GetLastError());
			//As the required flag could not set to jobObject handle, 
			// there is no use of holding this handle anymore.
			CloseHandle(m_jobObjectHandle);
			m_jobObjectHandle = NULL;
			return false;
		}
	}

	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	return true;

}

//Assging child process to job object for process cleanup in case of parent crash/exits
bool WinService::AssignChildProcessToJobObject(pid_t lProcessId)
{

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s pid : %ld\n", FUNCTION_NAME, lProcessId);

	if (m_jobObjectHandle)
	{

		HANDLE processHandle = NULL;
		processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, lProcessId);
		if (INVALID_HANDLE_VALUE == processHandle) {
			DebugPrintf(SV_LOG_DEBUG, "%s: failed to open the process %ld with error = %ld\n", FUNCTION_NAME, lProcessId, GetLastError());
			return false;
		}

		if (0 == AssignProcessToJobObject(m_jobObjectHandle, processHandle)) {
			DebugPrintf(SV_LOG_ERROR, "%s: Could not AssignProcessToObject for %ld with error = %ld\n", FUNCTION_NAME, lProcessId, GetLastError());
			CloseHandle(processHandle);
			return false;
		}

		DebugPrintf(SV_LOG_DEBUG, "%s: AssignProcessToObject is success\n", FUNCTION_NAME);
		CloseHandle(processHandle);
	}

	return true;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

}

void WinService::closeJobObjectHandle()
{
	if (NULL != m_jobObjectHandle)
	{
		CloseHandle(m_jobObjectHandle);
		m_jobObjectHandle = NULL;
	}

}



#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Singleton<WinService, ACE_Mutex>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Singleton<WinService, ACE_Mutex>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

#endif /* SV_WINDOWS */
