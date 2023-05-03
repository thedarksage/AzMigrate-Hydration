#include "service.h"
#include <fstream>
#include <string>
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <winbase.h>
#include <winsvc.h>
#include <tchar.h>

#include <boost/lexical_cast.hpp>

#include "servicemgr.h"
#include "logger.h"
#include "portable.h"

const int SERVICES_STOP_TIMEOUT = 600;//Max services stop call time out in second
const int SERVICES_START_TIMEOUT = 600;//Max services start call time out in second


bool ServiceMgr::getServiceList(const std::string& installationtype, std::list<std::string>& services, bool isMTInstalled)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "CS_INSTALLATION_TYPE  : %s and %s :\n", installationtype.c_str(), CS_INSTALLATION_TYPE);

	if (installationtype.compare(CS_INSTALLATION_TYPE) == 0)
	{
        // Always stop ProcessServerMonitor service first to avoid generating alerts if other services are stopped during install/upgrade/uninstall.
        services.push_back("ProcessServerMonitor");
        services.push_back("ProcessServer");
		services.push_back("tmansvc");
		services.push_back("cxprocessserver");
		services.push_back("InMage PushInstall");
		return rv;
	}
    services.push_back("ProcessServerMonitor");
    services.push_back("ProcessServer");
    services.push_back("tmansvc");
    services.push_back("InMage PushInstall");
    services.push_back("cxprocessserver");
	if (isMTInstalled)
	{
		//Please do not change the below order of push svagents and then obengine to the list , this list will be using in preserving service stop order of svagents and obengine.
		services.push_back("svagents");
		services.push_back("obengine");
		services.push_back("InMage Scout Application Service");
		//services.push_back("frsvc");
	}

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}

bool stopServiceWithOutWait(const std::string& svcName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entered stopServiceWithOutWait for %s.\n", svcName.c_str());
	bool bRet = true;

	SERVICE_STATUS_PROCESS  serviceStatusProcess;
	DWORD actualSize = 0;
	SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCManager == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "OpenSCManager failedfor service %s.\n", svcName.c_str());
		return FALSE;
	}

	SC_HANDLE hService = OpenService(hSCManager, svcName.c_str(), SERVICE_ALL_ACCESS);

	if (hService == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "OpenService failed for service %s.\n", svcName.c_str());
		::CloseServiceHandle(hSCManager);
		return FALSE;
	}
	if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState)
	{
		SERVICE_STATUS status;
		if (!::ControlService(hService, SERVICE_CONTROL_STOP, &status))
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to stop the service %s. Error %ld\n", svcName.c_str(), GetLastError());
			bRet = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully called stop for the service %s ...\n", svcName.c_str());
		}
	}
	else
	{
		if (SERVICE_STOPPED == serviceStatusProcess.dwCurrentState)
		{
			DebugPrintf(SV_LOG_INFO, "%s is already in stopped state\n", svcName.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed in QueryServiceStatusEx, Unable to stop the service %s. Error %ld\n", svcName.c_str(), GetLastError());
			bRet = false;
		}
	}

	::CloseServiceHandle(hService);
	::CloseServiceHandle(hSCManager);

	DebugPrintf(SV_LOG_DEBUG, "Exited stopServiceWithOutWait for %s.\n", svcName.c_str());
	return bRet;
}

bool startServiceWithOutWait(const std::string& svcName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	
	bool bRet = false;
	SERVICE_STATUS_PROCESS  serviceStatusProcess;
	DWORD actualSize = 0;

	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "OpenSCManager failed for service %s.\n", svcName.c_str());
		return false;
	}

	SC_HANDLE schService = OpenService(schSCManager, svcName.c_str(), SERVICE_ALL_ACCESS);
	if (schService == NULL)
	{
		DebugPrintf(SV_LOG_ERROR, "OpenService failed for service %s.\n", svcName.c_str());
		CloseServiceHandle(schSCManager);
		return false;
	}

	if (QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING != serviceStatusProcess.dwCurrentState)
	{
		if (!StartService(schService, 0, NULL))
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to start the service %s. Error %ld\n", svcName.c_str(), GetLastError());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully called start for the service %s ...\n", svcName.c_str());
			bRet = true;
		}
	}
	else
	{
		if (SERVICE_RUNNING == serviceStatusProcess.dwCurrentState)
		{
			DebugPrintf(SV_LOG_INFO, "%s is already in running state\n", svcName.c_str());
			bRet = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed in QueryServiceStatusEx, Unable to start the service %s. Error %ld\n", svcName.c_str(), GetLastError());
		}
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bRet;
}

bool isServiceStop(const std::string& svcName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED isServiceStop\n");
	bool bRet = false;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS  serviceStatusProcess;
	DWORD actualSize = 0;
	SERVICE_STATUS serviceStatus;
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager)
	{
		schService = OpenService(schSCManager, svcName.c_str(), SERVICE_ALL_ACCESS);
		if (NULL != schService)
		{
			if (QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize))
			{
				if (bRet = (SERVICE_STOPPED == serviceStatusProcess.dwCurrentState))
				{
					DebugPrintf(SV_LOG_DEBUG, "Service %s is in stopped state\n", svcName.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Service %s is not in stopped state\n", svcName.c_str());
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed in QueryServiceStatusEx, Unable to start the service %s. Error %ld\n", svcName.c_str(), GetLastError());
			}
			CloseServiceHandle(schService);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "OpenService failed for Service %s .\n", svcName.c_str());
			CloseServiceHandle(schSCManager);
			return false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "OpenScManager failed for Service %s .\n", svcName.c_str());
		return false;
	}
	CloseServiceHandle(schSCManager);

	DebugPrintf(SV_LOG_DEBUG, "EXITED isServiceStop\n");
	return bRet;
}

bool isServiceStart(const std::string& svcName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED isServiceStart for service %s\n", svcName.c_str());
	bool bRet = false;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS  serviceStatusProcess;
	DWORD actualSize = 0;
	SERVICE_STATUS serviceStatus;
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager)
	{
		schService = OpenService(schSCManager, svcName.c_str(), SERVICE_ALL_ACCESS);
		if (NULL != schService)
		{
			if (QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize))
			{
				if (bRet = (SERVICE_RUNNING == serviceStatusProcess.dwCurrentState))
				{
					DebugPrintf(SV_LOG_DEBUG, "Service %s is in running state\n", svcName.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Service %s is not in running state\n", svcName.c_str());
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed in QueryServiceStatusEx, Unable to start the service %s. Error %ld\n", svcName.c_str(), GetLastError());
			}
			CloseServiceHandle(schService);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "OpenService failed for Service %s .\n", svcName.c_str());
			CloseServiceHandle(schSCManager);
			return false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "OpenScManager failed for Service %s .\n", svcName.c_str());
		return false;
	}
	CloseServiceHandle(schSCManager);

	DebugPrintf(SV_LOG_DEBUG, "EXITED isServiceStart for service %s\n", svcName.c_str());
	return bRet;
}

bool startsvc(std::list<std::string>& services, std::list<std::string>& failedToStartservices)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	SV_UINT totalWaitTime = 0;

	std::list<std::string> listOfServicesSuccessAttemptedForStart;

	std::list<std::string>::const_iterator it = services.begin();
	while (it != services.end())
	{
		if (!startServiceWithOutWait(*it))
		{
			failedToStartservices.push_back(*it);
		}
		else
		{
			listOfServicesSuccessAttemptedForStart.push_back(*it);
		}
		it++;
	}


	std::list<std::string>::iterator svcAttemptedStartForSvcIt;

	while (!listOfServicesSuccessAttemptedForStart.empty() && totalWaitTime <= SERVICES_START_TIMEOUT)
	{
		svcAttemptedStartForSvcIt = listOfServicesSuccessAttemptedForStart.begin();
		while (svcAttemptedStartForSvcIt != listOfServicesSuccessAttemptedForStart.end())
		{
			if ((*svcAttemptedStartForSvcIt).compare("obengine") == 0 || isServiceStart(*svcAttemptedStartForSvcIt))
			{
				DebugPrintf(SV_LOG_DEBUG, "Erasing svc %s from the temp service start failed list and remaining list will copy later it to failed to start service list\n", (*svcAttemptedStartForSvcIt).c_str());
				listOfServicesSuccessAttemptedForStart.erase(svcAttemptedStartForSvcIt);
			}
			if (svcAttemptedStartForSvcIt != listOfServicesSuccessAttemptedForStart.end())
			{
				svcAttemptedStartForSvcIt++;
			}
		}

		if (!listOfServicesSuccessAttemptedForStart.empty())
		{
			Sleep(30 * 1000);
		}

		totalWaitTime += 30;
	}

	if (!listOfServicesSuccessAttemptedForStart.empty() || !failedToStartservices.empty())
	{
		if (!listOfServicesSuccessAttemptedForStart.empty())
		{
			svcAttemptedStartForSvcIt = listOfServicesSuccessAttemptedForStart.begin();
			while (svcAttemptedStartForSvcIt != listOfServicesSuccessAttemptedForStart.end())
			{
				failedToStartservices.push_back(*svcAttemptedStartForSvcIt);
				svcAttemptedStartForSvcIt++;
			}
		}

		rv = false;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return rv;

}

int ServiceMgr::startservices(std::list<std::string>& services, std::stringstream& errmsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SVS_OK;

   // start requested services
    std::list<std::string>::const_iterator it = services.begin();

	std::list<std::string> failedToStartservices;
	if (!startsvc(services, failedToStartservices))
	{
		errmsg << "CSPSConfigtool.exe was unable to restart one or more services\n";
		std::list<std::string>::const_iterator failedToStartservicesIt = failedToStartservices.begin();

		int count = 1;
		while (failedToStartservicesIt != failedToStartservices.end())
		{
			//to append in the errmsg the proper failed services name
			errmsg << count << ". " << m_ServicesFullNames.find(*failedToStartservicesIt)->second << "\n";
			failedToStartservicesIt++;
			count++;
		}
		errmsg << "Please start these services using Windows Service Control Manager.\n";
		rv = SVS_FALSE;
		DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully started all services.\n");

		it = services.begin();

		while (it != services.end())
		{
			std::string serviceStartUpType;
			InmServiceStartType serviceType = INM_SVCTYPE_NONE;
			if (getStartServiceType((*it).c_str(), serviceType, serviceStartUpType))
			{
				if ((*it).compare("obengine") != 0 && serviceType != INM_SVCTYPE_AUTO)
				{
					DebugPrintf(SV_LOG_DEBUG, "Changing SVC startuptype from %s to Automatic\n", serviceStartUpType.c_str());
					if (changeServiceType((*it).c_str(), INM_SVCTYPE_AUTO) == SVS_FALSE)
					{
						errmsg << "Failed to chnage start type of svc " << (*it).c_str() << "  to automatic.\n";
						DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
						rv = SVS_FALSE;
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG, "Successfully chnaged to Automatic\n");
					}
				}
			}

			it++;
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}

int ServiceMgr::stopservices(std::list<std::string>& services, std::stringstream& errmsg)
{
	int rv = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	std::string value;
	SV_UINT totalWaitTime = 0;

	std::list<std::string> listOfServicesAttemptedForStop;

	bool failureSendingStopRequest = false;

	// stop requested services
	std::list<std::string>::const_iterator it = services.begin();
	while (it != services.end())
	{
		if (!stopServiceWithOutWait(*it))
		{
			failureSendingStopRequest = true; 
			break;
		}
		else
		{
			listOfServicesAttemptedForStop.push_back(*it);
		}

		it++;
	}

	while (!listOfServicesAttemptedForStop.empty() && totalWaitTime <= SERVICES_STOP_TIMEOUT)
	{
		std::list<std::string>::iterator svcAttemptedStopForSvcIt = listOfServicesAttemptedForStop.begin();
		while (svcAttemptedStopForSvcIt != listOfServicesAttemptedForStop.end())
		{
			if (isServiceStop(*svcAttemptedStopForSvcIt))
			{
				DebugPrintf(SV_LOG_DEBUG, "Erasing svc %s from the StopSvcList\n", (*svcAttemptedStopForSvcIt).c_str());
				listOfServicesAttemptedForStop.erase(svcAttemptedStopForSvcIt);
			}
			if (svcAttemptedStopForSvcIt != listOfServicesAttemptedForStop.end())
			{
				svcAttemptedStopForSvcIt++;
			}
		}

		if (!listOfServicesAttemptedForStop.empty())
		{
			Sleep(30 * 1000);
		}
		totalWaitTime += 30;
	}

	if (!listOfServicesAttemptedForStop.empty() || failureSendingStopRequest)
	{
		std::string resp;
		resp = "Unable to save changes as one or more services could not be stopped. Please try again.\n";

		std::list<std::string> failedToStartservices;
		if (!startsvc(services, failedToStartservices))
		{
			std::list<std::string>::const_iterator failedToStartservicesIt = failedToStartservices.begin();

			if (failedToStartservicesIt != failedToStartservices.end())
			{
				resp = "Unable to save changes as one or more services could not be stopped. Please start the following services from Windows Service Control Manager before you re-try the operation.\n\n";
			}

			int count = 1;
			while (failedToStartservicesIt != failedToStartservices.end())
			{
				//to append in the errmsg the proper failed services name
				resp = resp + boost::lexical_cast<std::string>(count) +". " + m_ServicesFullNames.find(*failedToStartservicesIt)->second + "\n";

				failedToStartservicesIt++;
				count++;
			}
		}

		errmsg << resp;
		rv = SVS_FALSE;
		DebugPrintf(SV_LOG_ERROR,"%s\n", errmsg.str().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return rv;
}

bool ServiceMgr::RestartServices(std::list<std::string>& services, std::list<std::string>& failedToRestartServices, std::stringstream& err)
{
	bool bRet = true;

	if (stopservices(services, err) == SVS_FALSE)
	{
		return false;
	}

	startsvc(services, failedToRestartServices);

	if (!failedToRestartServices.empty())
	{
		std::string resp;
		resp = "Unable to save changes as one or more services could not be re-started. Please start the following services from Windows Service Control Manager before you re-try the operation.\n\n";

		int count = 1;
		std::list<std::string>::const_iterator failedToRestartservicesIt = failedToRestartServices.begin();

		while (failedToRestartservicesIt != failedToRestartServices.end())
		{
			//to append in the errmsg the proper failed services name
			resp = resp + boost::lexical_cast<std::string>(count)+". " + m_ServicesFullNames.find(*failedToRestartservicesIt)->second + "\n";

			failedToRestartservicesIt++;
			count++;
		}

		err << resp;
	}

	return failedToRestartServices.empty();
}

