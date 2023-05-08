
#ifdef SV_WINDOWS
#include "logger.h"
#include "portable.h"
#include "servicemajor.h"
#include "portablehelpersmajor.h"

WinService::WinService() : servicetitle(NULL), servicename(NULL), servicedesc(NULL), quit(0),m_hServiceHandle(NULL)
{
    HRESULT InmHRes;
	do
	{
		InmHRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (InmHRes != S_OK)
		{
			DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
			DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", InmHRes);
			break;
		}

		InmHRes = CoInitializeSecurity(
			NULL, -1, NULL,	NULL, RPC_C_AUTHN_LEVEL_CONNECT,
			RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	
		// Check if the securiy has already been initialized by someone in the same process.
		if (InmHRes == RPC_E_TOO_LATE)
		{
			InmHRes = S_OK;
		}

		if (InmHRes != S_OK)
		{
			DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
			DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", InmHRes);
			CoUninitialize();
			break;
		}
	} while(false);

}

WinService::~WinService()
{
 
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

void WinService::ChangeServiceDescription()
{

    SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCM != NULL)
    {
		// PR#10815: Long Path support
        SC_HANDLE hInmService = ::OpenServiceW(hSCM, this->servicename, SERVICE_ALL_ACCESS);
        if (hInmService != NULL)
        {
			SERVICE_DESCRIPTIONW ServiceDescription = { this->servicedesc };
			if( !ChangeServiceConfig2W( hInmService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription ) )
			{} // Ignore failure on setting the description: not a critical error
			
            ::CloseServiceHandle(hInmService);
        }
        ::CloseServiceHandle(hSCM);
    }
}

void WinService::SetServiceRecoveryOptions()
{

	SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCM != NULL)
	{
		SC_HANDLE hInmService = ::OpenServiceW(hSCM, this->servicename, SERVICE_ALL_ACCESS);
		if (hInmService != NULL)
		{
			DWORD dwRet = SetServiceFailureActionsRecomended(hInmService);
			if (ERROR_SUCCESS != dwRet)
			{
				DebugPrintf(SV_LOG_ERROR, "FAILED: Set service recovery options for the service - %s. Error = %lu \n.", this->servicename, dwRet);
			}

			::CloseServiceHandle(hInmService);
		}
		::CloseServiceHandle(hSCM);
	}
}

int WinService::run_startup_code()
{
	
	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);

	//Register with Service Control Manager/Dispatcher
	ACE_NT_SERVICE_RUN (InMageAgent, SERVICE::instance (), ret);
	if (ret == 0)
	{
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
    //DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
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
   // DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
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

void DeleteAgentQuitEventFiles( std::string const& filename )
{
}


#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Singleton<WinService, ACE_Mutex>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Singleton<WinService, ACE_Mutex>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

#endif /* SV_WINDOWS */
