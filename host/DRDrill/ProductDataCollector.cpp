#include "stdafx.h"

#include "Common.h"
using namespace DrDrillNS;

#include <string>
#include <windows.h>
#include <time.h>
#include "Drdefs.h"
#include "ProductDataCollector.h"


ProductDataCollector::ProductDataCollector(void)
{
	drInit();	
}
ProductDataCollector::~ProductDataCollector(void)
{

}
ProductDataCollector::ProductDataCollector(const _tstring &srcHost,
											const _tstring &tgtHost,
											const _tstring &evsName,
											const _tstring &appName,
											bool &bdrdrill)
{
	m_srcHostName = srcHost;
	m_tgtHostName = tgtHost;
	m_evsName = evsName;
	m_appName = appName;
	m_bDrDrill = bdrdrill;	
	drInit();	 
}
void ProductDataCollector::drInit(void)
{
	inmagePath = obj_adIntf.getAgentInstallPath();
	if (inmagePath.empty()) 
	{
		std::cout << "Could not determine the InMage Scout install location.\n";
	}
}
bool ProductDataCollector::PerformAppValidation()
{
	bool result = false;
	return result;
}
bool ProductDataCollector::CollectAppData()
{
	bool result = false;
	//std::cout<<"\nevsName = "<<m_evsName;
	return result;
}
bool ProductDataCollector::GenerateAppReport()
{
	bool result = false;
	return result;
}
bool ProductDataCollector::CollectData()
{
	bool result = false;
	result = CollectCommonData();
	if(result == false)
	{
		std::cout<<"\nFailed to collect common data for drdrill\n";
	}
	return true;
}
bool ProductDataCollector::CollectCommonData()
{
	bool result = false;
	result = GetDataFromSource();
	if(result == false)
	{
		std::cout<<"\nFailed to collect source common data for drdrill\n";
	}
	return result;
}
bool ProductDataCollector::GetDataFromSource()
{
	bool result = true;
	std::list<ULONG>::const_iterator ipAddrIter;
		
	//get hostID
	result = GetHostId();
	std::cout<<"\nGetHostId() = "<<result;
	std::cout<<"\nhostID = "<<m_chostID<<"\n";
			
	//get hostName
	result = GetHostName();
	std::cout<<"\nGetHostName() = "<<result;
	std::cout<<"\nhostName = "<<m_cHostName<<"\n";
	if(strcmp(m_srcHostName.c_str(),m_cHostName) != 0)
	{
		std::cout<<"\nERROR : Input of Source host name not matches with Local host name\n";
	}
		
	//get hostIP
	m_ulIpAddress = GetIpAddress(m_srcHostName);
	ipAddrIter = m_ulIpAddress.begin();
	while(ipAddrIter != m_ulIpAddress.end())
	{
		struct in_addr addr;
		addr.s_addr = (*ipAddrIter);
		std::cout<<"\n" << "IP = "<<inet_ntoa(addr);
		ipAddrIter++;
	}
	

	//get systemDrive
	m_cSystemDrive = GetSystemDrive();
	std::cout<<"\nsystemDrive = "<<m_cSystemDrive<<"\n";

	//get agentServiceStatus
	result = GetAgentServiceStatus();
	std::cout<<"\nGetAgentServiceStatus() = "<<result;
	std::cout<<"\nVXServiceStatus = "<<m_bvxSrvStatus;
	std::cout<<"\nFXServiceStatus = "<<m_bfxSrvStatus;
	std::cout<<"\nFXLogonAccount = "<<m_strFxLogOnAccount<<"\n";

	//get timeStamp
	result = GetTimeStamp();
	std::cout<<"\nGetTimeStamp() = "<<result;
	std::cout<<"\ntimeStamp = "<<m_cTimeStamp;

	return result;
}
bool ProductDataCollector::GetHostId()
{
	bool result = true;
	DWORD ret;
	_tstring drscoutPath = inmagePath;
	drscoutPath += std::string("\\") + APPLICATION_DATA_DIRECTORY + std::string("\\") + ETC_DIRECTORY + std::string("\\") + DRSCOUT_CONF_FILE;
	ret = GetPrivateProfileString(DRSCOUT_CONF_FILE_SECTION,DRSCOUT_CONF_HOSTID, NULL,m_chostID, MAX_PATH-1,drscoutPath.c_str());
	if(ret == 0)
	{
		std::cout<<"\nFailed to retrieve HostId from drscout.conf file "<<ret<<"\n";
		result = false;
	}
	return result;	
}
bool ProductDataCollector::GetHostName()
{
	bool result = true;
	int res = 0;
	res = gethostname(m_cHostName,sizeof(m_cHostName));
	
	if(res != 0)
	{
		res = WSAGetLastError();
		std::cout<<"\nERROR : gethostname failed with error code = "<<result<<"\n\n";
		result = false;
	}
	return result;	
}
std::list<ULONG> ProductDataCollector::GetIpAddress(const _tstring hostName)
{
	WSADATA wsaData;
    int iResult;

    DWORD dwError;
    int i = 0;
	std::list<ULONG> ipAddress;

    struct hostent *remoteHost;
    struct in_addr addr;
	
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
	{
        printf("WSAStartup failed: %d\n", iResult);
    }
	
	remoteHost = gethostbyname(hostName.c_str());
	if (remoteHost == NULL) 
	{
        dwError = WSAGetLastError();
        if (dwError != 0) 
		{
            if (dwError == WSAHOST_NOT_FOUND)
			{
                printf("Host not found\n");               
            } 
			else if (dwError == WSANO_DATA) 
			{
                printf("No data record found\n");                
			}
			else
			{
                printf("Function failed with error: %ld\n", dwError);                
            }
        }
    } 
	else 
	{
        i = 0;
        if (remoteHost->h_addrtype == AF_INET)
        {
            while (remoteHost->h_addr_list[i] != 0)
			{
                addr.s_addr = *(u_long *) remoteHost->h_addr_list[i++];
				ipAddress.push_back(addr.s_addr);
			}
        }
        else if (remoteHost->h_addrtype == AF_INET6)
        {   
            printf("IPv6 address was returned\n");
        }   
    }
    return ipAddress;
}
//Find the system drives
char ProductDataCollector::GetSystemDrive(void)
{
	char szSysDrive[MAX_PATH] = {0};	
	DWORD dw = GetSystemDirectory(szSysDrive, MAX_PATH);
     if(dw == 0) 
	 { 
		 std::cout<<"[ERROR][" << GetLastError() << "]GetSystemDirectory failed.\n";
	 }
	 	 return szSysDrive[0];
}
bool ProductDataCollector::GetAgentServiceStatus(void)
{
	bool result = true;
	std::list<_tstring> serviceName;
	std::list<_tstring>::const_iterator serviceIter;

	SC_HANDLE schService; 
	SERVICE_STATUS_PROCESS  serviceStatusProcess;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;
	DWORD dwBytesNeeded = 0;
	LPQUERY_SERVICE_CONFIG lpServiceConfig;
	DWORD cbBufSize = 0;
	DWORD dwError = 0;

	serviceName.push_back("frsvc");
	serviceName.push_back("svagents");
	serviceName.sort();

	SC_HANDLE srvSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == srvSCManager) 
	{
		//cout << "Failed to Get handle to Windows Services Manager in OpenSCMManager for "<<srchostname.c_str ()<<"\n";
		::CloseServiceHandle(srvSCManager);
	}
	for(serviceIter = serviceName.begin(); serviceIter != serviceName.end(); serviceIter++)
	{
		schService = ::OpenService(srvSCManager, (*serviceIter).c_str(), SERVICE_ALL_ACCESS); 
		if (NULL != schService)
		{ 
			if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize))
			{
				if(SERVICE_STOPPED != serviceStatusProcess.dwCurrentState) 
				{
					//cout << "The service current stat is :" << AgentServiceStat[serviceStatusProcess.dwCurrentState-1]<<"\n\n";
					//::CloseServiceHandle(schService);
					
					if((*serviceIter).find("frsvc") == NULL)
					{
						m_bfxSrvStatus = true;//AgentServiceStat[serviceStatusProcess.dwCurrentState-1];
						if( !QueryServiceConfig(schService,NULL,0,&dwBytesNeeded))
						{
							dwError = GetLastError();
							if( ERROR_INSUFFICIENT_BUFFER == dwError )
							{
								cbBufSize = dwBytesNeeded;
								lpServiceConfig = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, cbBufSize);
							}
							else
							{
								printf("QueryServiceConfig failed (%d)", dwError);
							}
						}
						if( !QueryServiceConfig(schService,lpServiceConfig,cbBufSize,&dwBytesNeeded))
						{
							printf("QueryServiceConfig failed (%d)", GetLastError());
							m_strFxLogOnAccount = "NULL";
							result = false;
        				}
						else
						{
							m_strFxLogOnAccount = lpServiceConfig->lpServiceStartName;
						}
					}
					else
						m_bvxSrvStatus = true;//AgentServiceStat[serviceStatusProcess.dwCurrentState-1];
				}
				else
				{
					if((*serviceIter).find("frsvc") == NULL)
					{
						m_bfxSrvStatus = false;//AgentServiceStat[serviceStatusProcess.dwCurrentState-1];
						m_strFxLogOnAccount = "NULL";
						result = false;
					}
					else
					{
						m_bvxSrvStatus = false;//AgentServiceStat[serviceStatusProcess.dwCurrentState-1];
						result = false;
					}
					//cout << "The service current stat is :" << AgentServiceStat[serviceStatusProcess.dwCurrentState-1]<<"\n\n";
					::CloseServiceHandle(schService);
				}
			}
			else
			{
				//cout << "Failed to query the service status\n";
				m_bfxSrvStatus = false;
				m_bvxSrvStatus = false;
				m_strFxLogOnAccount = "NULL";
				dwRet = GetLastError();
				result = false;
			}
		}
		else
		{
			dwRet = GetLastError();
		}
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    dwRet,
                    0,
                    (LPTSTR)&lpMsgBuf,
                    0,
                    NULL);
		if(dwRet)
		{
			printf("\n[ServiceName: %s]Error:[%d] %s\n",(*serviceIter).c_str(),dwRet,(LPCTSTR)lpMsgBuf);
			result = false;
		}
		// Free the buffer
		LocalFree(lpMsgBuf);
	}
	// Close the service handle
	::CloseServiceHandle(srvSCManager);
	return result;
}
bool ProductDataCollector::GetTimeStamp(void)
{
	bool result = true;
	time_t now;
	struct tm * timeinfo;

	now = time(NULL);
	if (now != -1)
	{
		timeinfo = localtime(&now);
		m_cTimeStamp =asctime(timeinfo);
	}
	else
	{
		std::cout<<"\nFailed to retrieve timeStamp\n";
		result = false;
	}
	return result;
}
std::list<ULONG> ProductDataCollector:: GetVirtualServerIP(const _tstring vsName)
{
	std::list<ULONG> vsIpAddress;
	
	vsIpAddress = GetIpAddress(vsName);
	return vsIpAddress;
}

	