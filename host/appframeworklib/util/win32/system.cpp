#include "system.h"
#include "appglobals.h"
#include "registry.h"
#include "config/applocalconfigurator.h"
#include "cdpplatform.h"
#include <sstream>
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <atlbase.h>
#include <strsafe.h>
//#include "system_win.h"
#include "InMageSecurity.h"
#include "Lm.h"
SVERROR getInterfaceGuidList(std::list<std::string>& list)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   SVERROR bRet = SVS_FALSE ;
   if( getSubKeysList("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}", list) != SVS_OK )
   {
       DebugPrintf(SV_LOG_ERROR, "Failed to get the network Guid names list\n") ;
   }
   else
   {
       std::list<std::string>::iterator guidNameIter = list.begin() ;
       while( guidNameIter != list.end() )
       {
           std::string name = *guidNameIter ;
           guidNameIter++ ;
           if( name[0] != '{')
           {
               list.remove(name) ;
           }
       }
       DebugPrintf(SV_LOG_DEBUG, "There were %d network interfaces found in registry\n", list.size()) ;
       bRet = SVS_OK ;
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}

SVERROR getNetworkNameFromGuid(const std::string& interfaceguid, std::string& networkname)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   SVERROR bRet = SVS_FALSE ;
   std::string regKey = "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\" + interfaceguid + "\\Connection" ;
   if( getStringValue(regKey, "Name", networkname) != SVS_OK )
   {
       DebugPrintf(SV_LOG_ERROR, "Failed to get the network name for the guid %s\n", interfaceguid.c_str()) ;
   }
   else
   {
       DebugPrintf(SV_LOG_DEBUG, "network name %s and guid %s\n", networkname.c_str(), interfaceguid.c_str()) ;
       bRet = SVS_OK ;
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}

SVERROR getIPAddressFromGuid(const std::string& interfaceguid, std::string& ipaddress)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   SVERROR bRet = SVS_FALSE ;
   std::string regKey = "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" + interfaceguid;
   std::list<std::string> list ;
   if( getMultiStringValue(regKey, "IPAddress", list) != SVS_OK )
   {
       DebugPrintf(SV_LOG_ERROR, "Failed to get the ipaddress for the guid %s\n", interfaceguid.c_str()) ;
   }
   else
   {
      std::list<std::string>::iterator iter = list.begin() ;
      if( iter != list.end() )
      {
            ipaddress = *iter ;
      }
      DebugPrintf(SV_LOG_DEBUG, "ipaddress name %s and guid %s\n", ipaddress.c_str(), interfaceguid.c_str()) ;
   }

   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;

}
bool isDynamicDNSRegistrationEnabled(const std::string& interfaceguid)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::string regKey = "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" +  interfaceguid ;
   SV_LONGLONG value = -1 ;
   bool bRet = false ;
   if( getDwordValue(regKey, "RegistrationEnabled", value) == SVS_OK )
   {
    if( value == 1 )
    {
        bRet = true ;
        DebugPrintf(SV_LOG_ERROR, "Dynamic DNS Update enabled on interface %s\n", interfaceguid.c_str()) ;
    }
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}

bool isDynamicDNSRegistrationEnabled()
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::list<std::string> interfaceguidsList ;
   bool ret = false ;
   if( getInterfaceGuidList(interfaceguidsList) != SVS_OK )
   {
       DebugPrintf(SV_LOG_ERROR, "Unable to get the interface guids list\n") ;
       ret = false ;
   }
   else
   {
       DebugPrintf(SV_LOG_DEBUG, "No. of network interfaces found from registry : %d\n", interfaceguidsList.size()) ;
       std::list<std::string>::const_iterator interfaceguidIter = interfaceguidsList.begin() ;
       while( interfaceguidIter != interfaceguidsList.end() )
       {
           if ( isDynamicDNSRegistrationEnabled(*interfaceguidIter) == true )
           {
               ret = true ;

               DebugPrintf(SV_LOG_DEBUG, "Dynamic DNS Update is enabled on interface %s\n", (*interfaceguidIter).c_str()) ;
               std::string networkname ;
               if( getNetworkNameFromGuid(*interfaceguidIter, networkname) != SVS_OK )
               {
                   DebugPrintf(SV_LOG_ERROR, "Failed to get the network name for interface guid %s\n", (*interfaceguidIter).c_str()) ;
               }
               DebugPrintf(SV_LOG_DEBUG, "Corresponding Network name for %s is %s\n", (*interfaceguidIter).c_str(), networkname.c_str()) ;
                break ;
           }
           else
           {
                DebugPrintf(SV_LOG_DEBUG, "Dynamic DNS Update is not enabled on interface %s\n", (*interfaceguidIter).c_str()) ;
                ret = false ;
               
           }
           interfaceguidIter++ ;
       }
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return ret ;
}

void ChangeDynamicDNSUpdateStatus(const std::string& interfaceguid, bool enable)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::string regKey = "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" +  interfaceguid ;
   SV_LONGLONG value = 1 ;
   if( enable == false )
   {
        value = 0 ;
   }
  
   if( setDwordValue(regKey, "RegistrationEnabled", value) != SVS_OK )
   {
       DebugPrintf(SV_LOG_ERROR, "Failed to set the RegistrationEnabled value for key %s\n", regKey.c_str()) ;
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ChangeDynamicDNSUpdateStatus(bool enable)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::string regKey = "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters" ;
   SV_LONGLONG value = 1 ;
   if( enable == false )
   {
        value = 0 ;
   }
  
   if( setDwordValue(regKey, "RegistrationEnabled", value) != SVS_OK )
   {
       DebugPrintf(SV_LOG_ERROR, "Failed to set the RegistrationEnabled value for key %s\n", regKey.c_str()) ;
   }
   std::list<std::string> interfaceList ;
   if( getInterfaceGuidList(interfaceList) != SVS_OK )
   {
       DebugPrintf(SV_LOG_DEBUG, "Failed to get the interface's list\n") ;
   }
   else
   {
       std::list<std::string>::const_iterator begin = interfaceList.begin() ;
       while( begin != interfaceList.end() )
       {
           ChangeDynamicDNSUpdateStatus(*begin, enable) ;
           begin++ ;
       }
   }

   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR getDomainName(std::string& domainName)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   SVERROR bRet = SVS_FALSE ;
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}
void RunSystemTests()
{
    std::list<std::string> list ;
    std::string str ;
    getEnvironmentVariable("SystemDrive", str) ;
    getInterfaceGuidList(list) ;
    std::list<std::string>::const_iterator iter = list.begin();
    while( iter != list.end() )
    {
        std::string networkname ;
        getNetworkNameFromGuid((*iter), networkname) ;
        ChangeDynamicDNSUpdateStatus((*iter), true) ;
        ChangeDynamicDNSUpdateStatus((*iter), false) ;
        isDynamicDNSRegistrationEnabled((*iter)) ;
        iter++ ;
    }
}

SVERROR getEnvironmentVariable(const std::string& variableName, std::string& value)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   SVERROR bRet = SVS_FALSE ;
   char buf[BUFSIZ] ;
   memset(buf, '\0', BUFSIZ) ;
   if( GetEnvironmentVariable(variableName.c_str(), buf, BUFSIZ) == 0 )
   {
       DebugPrintf(SV_LOG_ERROR, "GetEnvironmentVariable failed with error %d\n", GetLastError()) ;
   }
   else
   {
        value = buf ;
        bRet = SVS_OK ;
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}

void getworkgroupname(std::string& workgroupName)
{
	LPWSTR	lpNameBuffer = NULL;
    USES_CONVERSION ;
	NET_API_STATUS nas;
	NETSETUP_JOIN_STATUS BufferType;

	workgroupName = "Unknown" ;
	nas = NetGetJoinInformation(NULL, &lpNameBuffer, &BufferType);
	
	if (nas == NERR_Success)
	{
	    switch (BufferType)
	    {
	        case NetSetupUnknownStatus:
		        DebugPrintf(SV_LOG_DEBUG, "Unknown network status!\n");
		        break;

	        case NetSetupUnjoined:
		        DebugPrintf(SV_LOG_DEBUG, "Not joined to any workgroup or domain.\n");
		        break;

	        case NetSetupWorkgroupName:
		        DebugPrintf(SV_LOG_DEBUG, "joined to the workgroup %s.\n", lpNameBuffer);
                workgroupName = W2A(lpNameBuffer) ;
		        break;

	        case NetSetupDomainName:
		        DebugPrintf(SV_LOG_DEBUG, "I'm joined to the domain %s.\n", lpNameBuffer);
                workgroupName = W2A(lpNameBuffer) ;
		        break;
	    }
    }
    
	if( lpNameBuffer )
    {
	    NetApiBufferFree(lpNameBuffer);
    }
}
SVERROR getDnsName(std::string& domainName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    char buffer[256] ;
    SVERROR bRet = SVS_FALSE ;
    DWORD dwSize = sizeof(buffer);
	memset(buffer, '\0', 256) ;
    domainName = "Unknown" ;
    if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)ComputerNamePhysicalDnsDomain, buffer, &dwSize))
    {
        DebugPrintf(SV_LOG_ERROR, "GetComputerNameEx failed %d\n", GetLastError());        
        getworkgroupname (domainName) ;
    }
    else
    {
        domainName = buffer ;
        if( domainName.empty() )
        {
            getworkgroupname (domainName) ;
        }
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "Domain/Workgroup Name %s\n", domainName.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void getOSVersionInfo(OSVERSIONINFOEX& osVersionInfo)
{
    memset((void*)&osVersionInfo, '\0', sizeof(OSVERSIONINFOEX));
	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((LPOSVERSIONINFO)&osVersionInfo);
}

SVERROR isFirewallEnabled(InmFirewallStatus& status)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string regKey = "SYSTEM\\ControlSet001\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\DomainProfile" ;
    SVERROR bRet = SVS_FALSE ;
    SV_LONGLONG value = -1 ;
    if( getDwordValue(regKey, "EnableFirewall", value) == SVS_OK )
    {
        bRet = SVS_OK ;
        if( value == 1 )
        {
            status = FIREWALL_STAT_ENABLED ;
            DebugPrintf(SV_LOG_DEBUG, "Firewall is enabled \n") ;
        }
        else
        {
            status = FIREWALL_STAT_DISABLED ;
            DebugPrintf(SV_LOG_DEBUG, "Firewall is disabled \n") ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR getNetBiosName( std::string& netBiosName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK ;
	DWORD length = 128;
    CHAR szNodeName[MAX_COMPUTERNAME_LENGTH + 1];

    if (!GetComputerName(szNodeName, &length)) 
	{
		DebugPrintf(SV_LOG_ERROR, "GetComputerName() failed Error Code: %d \n", GetLastError() );
        retStatus = SVS_FALSE;
    }
	else
	{
		netBiosName = szNodeName ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}


bool RebootSystems(const char* pSystemTobeShutDown, bool bForce, char* szDomain,char* szUser,char* szPassword)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool ret = true;
	HANDLE hToken = NULL;
	if(szUser)
	{
		ret = LogonUser(szUser,szDomain,szPassword,LOGON32_LOGON_NEW_CREDENTIALS,
					LOGON32_PROVIDER_WINNT50,&hToken);
		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	SetShutDownPrivilege();

	DWORD dwRet = ERROR_SUCCESS;

	char szSystemName[MAX_PATH] = {0};
	ZeroMemory(szSystemName,MAX_PATH);
	inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", pSystemTobeShutDown);
	ret = InitiateSystemShutdownEx(szSystemName,"Shutdown is Initialted by InMage AppAgent",0,bForce,TRUE,SHTDN_REASON_MAJOR_APPLICATION |SHTDN_REASON_FLAG_PLANNED);
	if(ret)
	{
		DebugPrintf(SV_LOG_INFO,"\n Successfully initiated shutdown for the [%s] system\n",pSystemTobeShutDown);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to initiate shutdown for the [%s] system\n",pSystemTobeShutDown);
		DebugPrintf(SV_LOG_INFO,"Win32 Error:[%d]\n",GetLastError());
		ret = false;
	}
	
	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ret;
}

bool SetShutDownPrivilege() 
{ 
  DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
  HANDLE hProcToken = NULL; 
   
  bool bRet = TRUE;
 
   if (!OpenProcessToken(GetCurrentProcess(), 
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcToken)) 
   {
	   DebugPrintf(SV_LOG_ERROR, "OpenProcessToken failed with error code %d",GetLastError()) ;	
		  return false;
   }
 
   TOKEN_PRIVILEGES tokenPriv;
   LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME , 
        &tokenPriv.Privileges[0].Luid); 
 
   tokenPriv.PrivilegeCount = 1;   
   tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 

   AdjustTokenPrivileges(hProcToken, FALSE, &tokenPriv, 0,NULL, 0); 
 
   if (GetLastError() != ERROR_SUCCESS) 
		 bRet = FALSE; 
   if(hProcToken)
	   CloseHandle(hProcToken);
 
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet;
}

SVERROR setDynamicDNSUpdateKey(SV_LONGLONG value)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string regKey = "SYSTEM\\currentcontrolset\\Services\\tcpip\\Parameters" ;
    SVERROR bRet = SVS_FALSE ;
    if( setDwordValue(regKey, "DisableDynamicUpdate", 1) != SVS_OK )
    {
        DebugPrintf(SV_LOG_DEBUG, "Failed to set the DisableDynamicUpdate value from key %s\n", regKey.c_str()) ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully set the DisableDynamicUpdate value from key %s\n", regKey.c_str()) ;
        bRet = SVS_OK;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool isDisableDynamicUpdateSet()
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::string regKey = "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";
   SV_LONGLONG value = -1 ;
   bool bRet = false ;
   if( getDwordValue(regKey, "DisableDynamicUpdate", value) == SVS_OK )
   {
    if( value == 1 )
    {
        bRet = true ;
        DebugPrintf(SV_LOG_ERROR, "DisableDynamicUpdate is set\n") ;
    }
   }
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return bRet ;
}

bool CheckCacheDirSpace(std::string& cachedir, SV_ULONGLONG& capacity, SV_ULONGLONG& freeSpace, SV_ULONGLONG& expectedFreeSpace)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bRet = false;
    AppLocalConfigurator configurator ;
    cachedir = configurator.getCacheDirectory();
    SV_ULONGLONG quota = 0;
	capacity =0;
	freeSpace = 0;
    if (!GetDiskFreeSpaceP( cachedir, &quota, &capacity, &freeSpace))
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED CheckCacheDirSpace for %s: \n",cachedir.c_str());
    }
    else
    {
        unsigned long long pct = configurator.getMinCacheFreeDiskSpacePercent();
        unsigned long long expectedFreeSpace1  = (capacity * pct) / 100 ;
        unsigned long long expectedFreeSpace2  = static_cast<unsigned long long>(configurator.getMinCacheFreeDiskSpace());
        expectedFreeSpace = ((expectedFreeSpace1) < (expectedFreeSpace2)) ? (expectedFreeSpace1) : (expectedFreeSpace2);
        expectedFreeSpace +=1;
        if (freeSpace > expectedFreeSpace ) 
        {
            bRet = true;
        }
        else
        {

            DebugPrintf(SV_LOG_ERROR, "CheckCacheDirSpace low on local cache disk space:free space = " ULLSPEC,freeSpace);
            std::stringstream stream;
            stream << "\n\nLow Disk Space under Cache Directory: " << cachedir << "\n"
                   << "Volume Capacity in bytes: " << capacity << "\n"
                   << "Free Disk Space in bytes: " << freeSpace << "\n"
                   << "Minimum Required Free Disk Space in bytes: "  << expectedFreeSpace << "\n\n";
            DebugPrintf(SV_LOG_DEBUG,"CheckCacheDirSpace: %s\n",stream.str().c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool checkCMMinReservedSpacePerPair(std::string& cachedir, SV_ULONGLONG& capacity, SV_ULONGLONG& freeSpace, SV_ULONGLONG& expectedFreeSpace, SV_ULONGLONG& m_totalNumberOfPairs, SV_ULONGLONG& totalSpaceForCM, SV_ULONGLONG& totalCMSpaceReq)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool bRet = false;
	AppLocalConfigurator configurator ;
    cachedir = configurator.getCacheDirectory();
    SV_ULONGLONG quota = 0;
	SV_ULONGLONG consumedSpace;
	capacity =0;
	freeSpace = 0;
    if (!GetDiskFreeSpaceP( cachedir, &quota, &capacity, &freeSpace))
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED CheckCacheDirSpace for %s: \n",cachedir.c_str());
    }
    else
    {
		consumedSpace = getCacheDirConsumedSpace(cachedir);
		expectedFreeSpace = static_cast<unsigned long long>(configurator.getCMMinReservedSpacePerPair());
		totalSpaceForCM = consumedSpace + freeSpace;
		totalCMSpaceReq = expectedFreeSpace * m_totalNumberOfPairs;
		if(totalSpaceForCM >= totalCMSpaceReq)
		{
			bRet = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "CheckCacheDirSpace low on local cache disk space:free space\n");
            std::stringstream stream;
            stream << "\n\nLow Disk Space under Cache Directory: " << cachedir << "\n"
                   << "Volume Capacity in bytes: " << capacity << "\n"
                   << "Free Disk Space in bytes: " << freeSpace << "\n"
                   << "Minimum Required Free Disk Space per pair in bytes: "  << expectedFreeSpace << "\n"
				   << "The Total Number of Pairs: " << m_totalNumberOfPairs << "\n"
				   << "Total Cache Memory Required for all the pairs: " << totalCMSpaceReq << "\n"
				   << "Total Cache Memory Available including consumed Space: " << totalSpaceForCM << "\n\n";
            DebugPrintf(SV_LOG_DEBUG,"CheckCacheDirSpace: %s\n",stream.str().c_str());
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return bRet;
}
SV_ULONGLONG getCacheDirConsumedSpace(std::string& cacheDir)
{
	SV_ULONGLONG consumedSpace = 0;
	WIN32_FIND_DATA InmFileData;
	std::string InmFileName = cacheDir + "\\*.*";
	HANDLE hFile = FindFirstFile(InmFileName.c_str(),&InmFileData);
	if(hFile != INVALID_HANDLE_VALUE)
    {
        do 
		{
            if( (InmFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
            {
                if( strcmp(InmFileData.cFileName,".") != 0 && strcmp(InmFileData.cFileName,"..") != 0)
                {
					// We found a sub-directory, so get the files in it too
					InmFileName = cacheDir + "\\" + InmFileData.cFileName;
					consumedSpace += getCacheDirConsumedSpace(InmFileName);
				}
            }
            else
            {
                LARGE_INTEGER sz;                
                sz.LowPart = InmFileData.nFileSizeLow;
                sz.HighPart = InmFileData.nFileSizeHigh;
                consumedSpace += sz.QuadPart;
 
            }
        }while( FindNextFile(hFile,&InmFileData) != 0);
        FindClose(hFile);
	}
	return consumedSpace;
}

SVERROR readAndChangeServiceLogOn(HANDLE& userToken_)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE ;
	if(LogOnGrantProcess(userToken_) == SVS_OK)
	{
		DebugPrintf(SV_LOG_DEBUG, "LogOnGrantProcess() Success.\n");
		retStatus = SVS_OK;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "LogOnGrantProcess() Failed.\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR LogOnGrantProcess(HANDLE& userToken_)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE ;

	bool isDomainAccount = false;
	std::string strDomainName;	 
	std::string strDomainUser;
	std::string strDomainUserPwd;
	DWORD algId;
	std::string errorMsg;
	
	if(DecryptCredentials(strDomainName, strDomainUser, strDomainUserPwd, algId, errorMsg) == TRUE)
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully read the credentials from Registry.\n");
		userToken_ = NULL;
		if(InMLogon(strDomainUser,strDomainName, strDomainUserPwd, userToken_) == true)
		{
			retStatus = SVS_OK;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to Logon with Given domain Credentials.Error Code: %ld\n", GetLastError());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to fetch the Domain Credentials. Ensure that domain credentials are persitsted by using WinOp.exe.\n");
	}				

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR QueryAppServiceTofindAccountInfo(bool& isDomainAccount)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE ;
	isDomainAccount = false;
	SC_HANDLE schService, schSCManager;
	LPQUERY_SERVICE_CONFIG LPSCQUERY;
	LPCTSTR lpszServiceName = "InMage Scout Application Service";
	DWORD dwBytesNeeded;
	BOOL bSuccess = TRUE;
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (NULL != schSCManager)
	{
		schService = OpenService(schSCManager, lpszServiceName, SERVICE_QUERY_CONFIG);
		if(schService != NULL)
		{
			LPSCQUERY = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, 4096);
			if( LPSCQUERY != NULL )
			{
				if(::QueryServiceConfig(schService, LPSCQUERY, 4096, &dwBytesNeeded) == TRUE)
				{
					std::string strAccountName  = LPSCQUERY->lpServiceStartName;
					if( strcmp(strAccountName.c_str(), "LocalSystem") == 0 )
					{
						DebugPrintf(SV_LOG_ERROR, "App Service is running under Local System. \n" );
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, " APP Service is running under Domain Account.\n" );						
						isDomainAccount = true;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "QueryServiceConfig() Failed. Error Code: %ld \n", GetLastError());
				}
				LocalFree(LPSCQUERY);
			}
			CloseServiceHandle(schService);
			retStatus = SVS_OK;	
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open APP Service.\n");
		}
		CloseServiceHandle(schSCManager);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "OpenSCManager() Failed. Failed to query Service Manager. Error Code: %ld.\n ", GetLastError());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

bool InMLogon(const std::string& userName, const std::string& domain, const std::string& password, HANDLE &userToken_)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false;

	if(userName.empty() == false && domain.empty() == false && password.empty() == false)
	{
		DebugPrintf(SV_LOG_DEBUG, "Attempting to Logon with a User: %s, Domain: %s.\n", userName.c_str(), domain.c_str());
		if(LogonUser(userName.c_str(), domain.c_str(), password.c_str(), /*LOGON32_LOGON_INTERACTIVE*/LOGON32_LOGON_SERVICE, LOGON32_PROVIDER_DEFAULT, &userToken_) == TRUE)
		{
			if(ImpersonateLoggedOnUser(userToken_))
			{
				bRet = true;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Error occured during Impersonate LoggedOnUser.Error code : %ld \n", GetLastError());
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Error occured during LogOnUser.Error code : %ld \n",  GetLastError());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "The User name ,Domain Name and Password fields are empty.\n");
		userToken_ = NULL;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

SVERROR revertToOldLogon()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK ;
	if(RevertToSelf() == FALSE)
	{
		DebugPrintf(SV_LOG_ERROR, "RevertToSelf() Failed. Erro Code: %ld \n", GetLastError());
		retStatus = SVS_FALSE;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;	
}

std::string GetSystemDir()
{
	TCHAR szSysDrive[MAX_PATH]; 
	std::string SystemDrive ;
	DWORD dw = GetSystemDirectory(szSysDrive, MAX_PATH);
     if(dw == 0) 
	 { 
		DebugPrintf( SV_LOG_ERROR , "GetSystemDirectory failed with error: %d\n", GetLastError() ) ;
	 }
	 else
	 {
		 SystemDrive = std::string(szSysDrive);
		 size_t pos = SystemDrive.find_first_of(":");
		 if( pos  != std::string::npos )  
		 SystemDrive.assign(SystemDrive,0,pos);
	 }
	 return SystemDrive;
}