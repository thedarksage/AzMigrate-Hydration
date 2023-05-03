// ClusUtil.cpp : Defines the entry point for the console application.
//
//#include "stdio.h"
#include "stdafx.h"
#include <shlwapi.h>
#include "AtlConv.h"
#include<fstream>
#include<vector>
#include <stdio.h>
#include <assert.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include "WinInet.h"
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include "initguid.h"
#include "vds.h"
#define _CLUSUTIL_LOG //Generate Clusutillog file
#include "..\WinOpLib\MapDriveLetter.h"



#include "ClusUtil.h"
#include "..\WinOpLib\system.h"
#include "localconfigurator.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "terminateonheapcorruption.h"


#define VX_SERVICE							"svagents"
#define CLUSTER_SERVICE						"ClusSvc"
#define CLUSTER_DISK_DRIVER_SERVICE			"ClusDisk"
#define SVAGENT_CLUSTER_TRACKING_REG_KEY	"SOFTWARE\\SV Systems\\ClusterTracking"
#define CLEANUP_CLUSTER_DATABASE_SCRIPT		"/cleanup_cluster_database.php"
#define VIEW_CX_LOGIN_PAGE					"/ui/view_cx_login.php"

#define CLUSTER_TO_STANDALONE_TYPE			0
#define STANDALONE_TO_CLUSTER_TYPE 			1
#define ONLINE_DISK							2

#define SZ_CLUS_TO_STANDALONE				"ClusterToStandalone:"
#define SZ_STANDALONE_TO_CLUS				"StandaloneToCluster:"
#define SZ_ONLINE_DISK						"OnlineDisk"

#define REGISTER_BUFSIZE 16384
#define BUFFERSIZE MAX_PATH
#define FILESYSNAMEBUFSIZE MAX_PATH

#define RET_SUCCESS 0
#define RET_FAILURE 1

#define NUM_RETRIES 20
#define BRING_ONLINE_RETRY_INTERVAL 15000


using namespace std;
//If OS is Windows Server 2008 or above

//libraries required to link with the winsock and ping features
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")


//libraries required to link with VDS
#pragma comment(lib,"Ole32.lib")
#pragma comment(lib,"Advapi32.lib")

#ifdef _CLUSUTIL_LOG
	#define printf LogAndPrint
#endif

//Constant definitions used in Ping and VDS features
#define SHUTDOWN_WAIT_TIME 15000	//15 seconds
#define PING_TIMEOUT 5000	//5 seconds
#define NUM_PING_TRIES 100

#define CleanUpAndRelease(x) {if (NULL != x) { x->Release(); x = NULL; } }
#define WIN2K8_SERVER_VER 0x6


// Declare and initialize variables    
  HANDLE hIcmpFile      = 0;
  unsigned long ipaddr  = INADDR_NONE;
  DWORD dwRetVal        = 0;
  char SendTestData[]       = "Test Data";
  LPVOID ResponseBuf    = NULL;
  DWORD ResponseSize       = 0;

  CopyrightNotice displayCopyrightNotice;

DWORD GetCxUpStatus()
{
	DWORD dwRet = RET_SUCCESS;
	LocalConfigurator config;
	SVERROR svError = postToCx(GetCxIpAddress().c_str(),
								config.getHttp().port,
								VIEW_CX_LOGIN_PAGE,
								NULL,
								NULL,
								config.IsHttps());
	if(svError.failed())
	{
		//LogAndPrint("\nCx Server is not up and hence Converting Cluster To Standalone operation cannot be done.\n");
		dwRet = RET_FAILURE;
	}

	return dwRet;
}
//GetHostName Function
  bool GetHostName(char* pszInmHostName,UINT lenHostName)
  {
	bool bStatus = true;
	WSADATA wsaInmData;
	memset(&wsaInmData,0x00,sizeof(wsaInmData));
	if(WSAStartup(MAKEWORD(1,0),&wsaInmData))
	{
		LogAndPrint( "FAILED; Couldn't start Winsock, err %d\n", WSAGetLastError() );
	    return false;
	}	
	int iRet = gethostname(pszInmHostName,lenHostName);
	if(iRet != 0)
	{
		LogAndPrint("\n Error in getting the Hostname. WinSock Error = %d",WSAGetLastError());
		bStatus = false;
	}
	else
	{
		LogAndPrint("\n HostName = %s",pszInmHostName);
	}
	WSACleanup();
	return bStatus;
  }

//Function to detect duplicate node entries
  bool IsDuplicateEntryPresent(vector<char *>& cont,_TCHAR* token)
  {
	  bool ret = false;
	  for(unsigned int i = 0; i < cont.size();i++)
	  {
		  if(!stricmp(cont[i],token))
		  {
			  ret = true;
			  break;
		  }
	  }
	  return ret;
  }

 //Function to get the number of nodes from a string where nodes are separated by ",;"
  unsigned int GetNodeCount(char* listOfNodesTobeShutdown, vector<char *> *pvNodeNames)
  {
	  _TCHAR* token;
	  _TCHAR* seps = ",;" ;
	  bool bPrint = true;

	  if(listOfNodesTobeShutdown)
	  {
		  token = _tcstok((_TCHAR*)listOfNodesTobeShutdown,seps);
		  while( token != NULL )
		  {
			  if(IsDuplicateEntryPresent(*pvNodeNames,token))
			  {
				  if(bPrint)
				  {
					  LogAndPrint("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					  bPrint = false;
				  }
				  LogAndPrint("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				  token = _tcstok( NULL, seps );
			  }
			  else
			  { 
				  pvNodeNames->push_back(token);
				  token = _tcstok( NULL, seps );
			  }
		  }
	  }
    return (unsigned int)pvNodeNames->size();
  }

//Function to perform all the required cleanup after completing the pinging of nodes to be shutdown
void DoSocknPingCleanUp()
{
  WSACleanup();
  free(ResponseBuf);
  CloseHandle(hIcmpFile);
}

  //Use this program as a module test for your ping functionality in ClusUtil.cpp
  //The Active Node will wait for the other Cluster Nodes to shutdown. It will wait 3 seconds per node.
  //It will try three times to ping each node giving a gap of SHUTDOWN_WAIT_TIME
bool CheckOtherNodesShutdownStatus(char* pShutdownNodesList)
{
	bool retVal = true;
    // Validate the parameters
    if(NULL == pShutdownNodesList)
    {
      LogAndPrint("\n No nodes available to shutdown.\n");
      return true;
    }
      
	vector<char *> vNodeNames;
	unsigned int nodes = GetNodeCount(pShutdownNodesList,&vNodeNames);

    //Initialize the WinSock Library
    WSADATA winsockData;
    memset(&winsockData,0x00,sizeof(winsockData) );
    if( WSAStartup( MAKEWORD( 1, 0 ), &winsockData ) )
    {
      LogAndPrint( "FAILED; Couldn't start Winsock, err %d\n", WSAGetLastError() );
      return false;
    }
    //Create a ICMP handle to ping the nodes for checking if they are shutdown or not!
    hIcmpFile = IcmpCreateFile();
    if(hIcmpFile == INVALID_HANDLE_VALUE)
    {
      LogAndPrint("\tUnable to open ICMP handle used for pinging other nodes.\n");
      LogAndPrint("IcmpCreatefile returned error: %ld\n", GetLastError());
      return false;
    } 
    struct hostent *ptrHostent = NULL;  
	
    for(unsigned int count = 0; count< nodes; count++)
    {
      //Get the IP address from the HostName for all the nodes
      if((ptrHostent = gethostbyname(vNodeNames[count])) == NULL)
      {
        LogAndPrint("\n %s was already shutdown \n",vNodeNames[count]);
        continue;
      }
      else 
      {
        struct in_addr internetAddress;
        internetAddress.s_addr =  *(u_long *) ptrHostent->h_addr_list[0];
        char *address = inet_ntoa(internetAddress);
        ipaddr = inet_addr((const char*)address);
        if (ipaddr == INADDR_NONE)
        { 
          LogAndPrint("\n%s was already shutdown.\n",vNodeNames[count]);
          continue;
        }
      }
      #if defined(_WIN64)
		INM_SAFE_ARITHMETIC(ResponseSize   = InmSafeInt<size_t>::Type(sizeof(ICMP_ECHO_REPLY32)) + sizeof(SendTestData), INMAGE_EX(sizeof(ICMP_ECHO_REPLY32))(sizeof(SendTestData)))
	  #else
		INM_SAFE_ARITHMETIC(ResponseSize   = InmSafeInt<size_t>::Type(sizeof(ICMP_ECHO_REPLY)) + sizeof(SendTestData), INMAGE_EX(sizeof(ICMP_ECHO_REPLY))(sizeof(SendTestData)))
	  #endif
      ResponseBuf = (VOID*) malloc(ResponseSize);
      if (ResponseBuf == NULL)
      {
        LogAndPrint("\tUnable to allocate memory\n");
        DoSocknPingCleanUp();
        return false;
      }
      bool bRetry = false;
	  LogAndPrint("\nWaiting for the node %s to shutdown...\n",vNodeNames[count]);
      for(int retryCount = 0; retryCount < NUM_PING_TRIES; retryCount++)
      { 
        dwRetVal = IcmpSendEcho(hIcmpFile,
                  ipaddr   ,
                  SendTestData ,
                  sizeof(SendTestData),
                  NULL    ,
                  ResponseBuf,
                  ResponseSize,
                  PING_TIMEOUT);
        
      //the node is still up, so wait for some time and check if the node is shutdown or not
      if (dwRetVal != 0)
      {
        bRetry = true;
        if((NUM_PING_TRIES - 1) == retryCount)
        {
          LogAndPrint("\nShutdown Failed;The node %s could not be shutdown.\n",vNodeNames[count]);
          DoSocknPingCleanUp();
          return false;
        }
        else
        {
          //LogAndPrint("\nWaiting for the node %s to shutdown...\n",vNodeNames[count]);
          Sleep(SHUTDOWN_WAIT_TIME);
        }
      }
      else//the node is shutdown
      {
        DWORD dwErr = GetLastError();
        if((dwErr == ERROR_INVALID_PARAMETER) ||(dwErr == ERROR_NOT_ENOUGH_MEMORY) ||(dwErr == ERROR_NOT_SUPPORTED))
        {
          LogAndPrint("\tCall to IcmpSendEcho failed.Error occurred when tyring to ping the node.\n");
          LogAndPrint("\tIcmpSendEcho returned error: %ld\n", GetLastError() );
          DoSocknPingCleanUp();
          return false;
        }
        else
        {
          LogAndPrint("\n The node %s is shutdown.\n",vNodeNames[count]);
          bRetry = false;
          break;
        }
      }//end of else case (node is shutdown)
      }//end of for loop used for ping retries
    }//end of for loop used for iterating the other cluster nodes to be shutdown
    DoSocknPingCleanUp();
	return retVal;
  }

  //Using Virtual Disk Service implementation of Microsoft, bring all the required
  //disks which are Offline to Online


//Enable Debug Privileges

void EnableDebugPriv( void )
{
 HANDLE hInmToken;
 LUID privValue;
 TOKEN_PRIVILEGES tokPriv;

 if ( ! OpenProcessToken( GetCurrentProcess(),
  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hInmToken ) )
 {
  wprintf( L"OpenProcessToken() failed, Error = %d privValue is not available.\n", GetLastError() );
  return;
 }

 if ( ! LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &privValue ) )
 {
  wprintf( L"LookupPrivilegeValue() failed, Error = %d privValue is not available.\n", GetLastError() );
  CloseHandle( hInmToken );
  return;
 }

 tokPriv.PrivilegeCount = 1;
 tokPriv.Privileges[0].Luid = privValue;
 tokPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

 if ( ! AdjustTokenPrivileges( hInmToken, FALSE, &tokPriv, sizeof tokPriv, NULL, NULL ) )
  wprintf( L"AdjustTokenPrivileges() failed, Error = %d privValue is not available.\n", GetLastError() );
  
 CloseHandle( hInmToken );
}


//Find the Number of Volumes and MountPoints in the system
//this info is required to wait for each Disk,Volume and MountPoint
UINT gTotalVolnMountPointsCount = 0;
vector<std::string>vMountPoints;

BOOL ProcessVolMntPntsForGivenVol (HANDLE hVol, char *volGuidBuf, int iBufSize,vector<const char *> &LogicalVolumes)
{
   BOOL bFlag								= FALSE; // result flag holding ProcessVolumeMountPoint
   BOOL bFixedDrive							= TRUE; //Flag to find the status of volume
   HANDLE hMountPoint						= NULL; // handle for Mount Point
   char volMountPointBuf[BUFFERSIZE]			= {0}; // A buffer to hold Volume Mount Point
   char volBuf[BUFFERSIZE]						= {0}; // A buffer to hold Volume
   char MtPtGuid[BUFFERSIZE]					= {0};  // target of mount at mount point
   char volGuidMtPtBufPath[BUFFERSIZE]			= {0};   // construct a complete path here
   char FileSysNameBuf[FILESYSNAMEBUFSIZE]  = {0};
   DWORD dwSysFlags							= 0;   // flags that describe the file system
   ULONG returnedLength						= 0;
   
   if ( DRIVE_FIXED != GetDriveType(volGuidBuf))
   {
	   bFixedDrive = FALSE;
   }
   if(TRUE == bFixedDrive)
   {
		// Is this volume an NTFS file system?
		// PR#10815: Long Path support
		SVGetVolumeInformation( volGuidBuf,
							  NULL, 
							  0, 
							  NULL, 
							  NULL,
							  &dwSysFlags, 
							  FileSysNameBuf,
                              ARRAYSIZE(FileSysNameBuf));

		// If Reparse Points are supported then Volume Mounts Points will be supported
		if (! (dwSysFlags & FILE_SUPPORTS_REPARSE_POINTS))
		{
		}
		else
		{
			// Start processing mount points on this volume.
			hMountPoint = FindFirstVolumeMountPoint(volGuidBuf, volMountPointBuf,BUFFERSIZE);

			if (hMountPoint == INVALID_HANDLE_VALUE)
			{
			}
			else
			{
				static UINT nMtPtIndex = 0;
				do
				{
					memset((void*)volGuidMtPtBufPath,0x00,BUFFERSIZE);
					memset((void*)MtPtGuid,0x00,BUFFERSIZE);
					memset((void*)volBuf,0x00,BUFFERSIZE);
					returnedLength = 0;
					inm_strncpy_s(volBuf,ARRAYSIZE(volBuf),volGuidBuf,BUFFERSIZE);
					inm_strcat_s(volBuf,ARRAYSIZE(volBuf), volMountPointBuf);
					bFlag = GetVolumeNameForVolumeMountPoint( volBuf,MtPtGuid,BUFFERSIZE);
					if (!bFlag)
					{	
					}
					else
					{
						if(GetVolumePathNamesForVolumeName(MtPtGuid,volGuidMtPtBufPath,BUFFERSIZE,&returnedLength))
						{
							string sMountPoint = volGuidMtPtBufPath;
							vMountPoints.push_back(sMountPoint);
							nMtPtIndex += 1;
						}
					}
					memset((void*)volMountPointBuf,0x00,BUFFERSIZE);
					bFlag = FindNextVolumeMountPoint( hMountPoint,volMountPointBuf,BUFFERSIZE);//check why only of BUFFERSIZE

				}while(bFlag);

				FindVolumeMountPointClose(hMountPoint);
			}
		}
   }
   return (bFlag);
}

BOOL GetMountPointsList(vector<const char *> &LogicalVolumes)
{
   LogAndPrint("\n Getting Mount Points List...");
   char volGuid[BUFFERSIZE]	= {0};  // buffer to hold GUID of a volume.
   HANDLE hVol				= NULL; // handle of a Volume to be scanned.
   BOOL bFlag				= FALSE;// result of Processing each Volume.

   // Scan all the volumes starting with the first volume.
   hVol = FindFirstVolume (volGuid, BUFFERSIZE );

   if (hVol == INVALID_HANDLE_VALUE)
   {
	  LogAndPrint ("\n\tNo volumes available to get the list of Mount Points!\n");
      return FALSE;
   }
   do
   {   
	   ProcessVolMntPntsForGivenVol(hVol,volGuid,BUFFERSIZE,LogicalVolumes);
	   gTotalVolnMountPointsCount++;
   }while(FindNextVolume(hVol,volGuid,BUFFERSIZE));
   
   bFlag = FindVolumeClose(hVol);

   //Now insert all the collected Mount Points into the LogicalVlumes
   for(UINT i = 0 ; i < (UINT)vMountPoints.size(); i++)
   {
	   LogicalVolumes.push_back(vMountPoints[i].c_str());
   }

   gTotalVolnMountPointsCount += (UINT)LogicalVolumes.size() + (UINT)vMountPoints.size();
   return (bFlag);
}

//*****************************************************************************
//This function cleans up the DiskProperties structure of VDS.
//*****************************************************************************
void CleanUpDiskProps(VDS_DISK_PROP &vdsInmDiskProps)
{
  if (NULL != vdsInmDiskProps.pwszDiskAddress)
  {
    CoTaskMemFree(vdsInmDiskProps.pwszDiskAddress);
	vdsInmDiskProps.pwszDiskAddress = NULL;
  }
  if(NULL != vdsInmDiskProps.pwszName)
  {
    CoTaskMemFree(vdsInmDiskProps.pwszName);
	vdsInmDiskProps.pwszName = NULL;
  }
  if(NULL != vdsInmDiskProps.pwszFriendlyName)
  {
    CoTaskMemFree(vdsInmDiskProps.pwszFriendlyName);
	vdsInmDiskProps.pwszFriendlyName = NULL;
  }
  if(NULL != vdsInmDiskProps.pwszAdaptorName)
  {
    CoTaskMemFree(vdsInmDiskProps.pwszAdaptorName);
	vdsInmDiskProps.pwszAdaptorName = NULL;
  }
  if(NULL != vdsInmDiskProps.pwszDevicePath)
  {
    CoTaskMemFree(vdsInmDiskProps.pwszDevicePath);
	vdsInmDiskProps.pwszDevicePath = NULL;
  }
  ZeroMemory(&vdsInmDiskProps, sizeof(VDS_DISK_PROP));
}

//*****************************************************************************
//This function enumerates the VDS Disks available in the list of available
//VDS packs.It then brings the required disks which are Offline to Online.
//*****************************************************************************
HRESULT ProcessVdsPacks(IN IEnumVdsObject **pEnumIVdsPackIntf)
 {
   USES_CONVERSION;
   VDS_DISK_PROP       vdsInmDiskProps;
   IEnumVdsObject      *pInmEnumVdsDisk     = NULL;
   IVdsDisk            *pInmDisk            = NULL;
   IVdsPack            *pIVdsPack        = NULL;
   IVdsDiskOnline      *pVdsDiskOnline   = NULL;
   IUnknown            *pIUnknownIntf        = NULL;
   HRESULT             hResult           = S_OK;
   ULONG               ulFetched         = 0;

   ZeroMemory(&vdsInmDiskProps, sizeof(VDS_DISK_PROP));//initialize the structure members.

  assert(NULL != pEnumIVdsPackIntf);
  while(((*pEnumIVdsPackIntf)->Next(1,&pIUnknownIntf,&ulFetched))== 0x00)
  {
    if(!pIUnknownIntf)
    {
      LogAndPrint("\n Unable to get the Pack from the VDS Software Provider.hResult = 0x%x\n",hResult);
      return -1;
    }
    hResult = pIUnknownIntf->QueryInterface(IID_IVdsPack,(void**)&pIVdsPack);
    if(!SUCCEEDED(hResult)|| (!pIVdsPack))
    {
      LogAndPrint("\n Unable to get the Vds Pack object.hResult = 0x%x\n",hResult);
      CleanUpAndRelease(pIUnknownIntf);
      return hResult;
    }
    CleanUpAndRelease(pIUnknownIntf);

    hResult = pIVdsPack->QueryDisks(&pInmEnumVdsDisk);
    if(!SUCCEEDED(hResult))
    {
      LogAndPrint("\n Failed to get the Disks from the Pack.hResult = 0x%x\n",hResult);
      CleanUpAndRelease(pIVdsPack);
      return hResult;//S_FALSE;
    }

    IUnknown *pUn = NULL;
    while(((pInmEnumVdsDisk->Next(1,&pUn,&ulFetched)) == 0x00))
    {
      assert(1 == ulFetched);
      if(pUn)
      {
        hResult = pUn->QueryInterface(IID_IVdsDisk,(void**)&pInmDisk);
        if(!SUCCEEDED(hResult) || (!pInmDisk))
        {
          LogAndPrint("\n Unable to get the Disk pointer.hResult = 0x%x\n",hResult);
          CleanUpAndRelease(pUn);
          return hResult;
        }
        CleanUpAndRelease(pUn);

        hResult = pInmDisk->GetProperties( &vdsInmDiskProps);
        if(SUCCEEDED(hResult))
        {
          if(VDS_DS_OFFLINE == vdsInmDiskProps.status)
          {
            hResult = pInmDisk->QueryInterface(IID_IVdsDiskOnline,(void**)&pVdsDiskOnline);
            if(SUCCEEDED(hResult) && (NULL != pVdsDiskOnline))
            {
              hResult = pVdsDiskOnline->Online();
              if(!SUCCEEDED(hResult))
              {
                LogAndPrint("\nError bringing the disk %s [Disk Friendly Name = %s] to online.hResult = 0x%x\n",W2A(vdsInmDiskProps.pwszDevicePath),W2A(vdsInmDiskProps.pwszFriendlyName),hResult);//MCHECK
                CleanUpAndRelease(pVdsDiskOnline);
                return hResult;
              }
              else
              {
                //Clear the ReadOnly Flags if any
                hResult = pInmDisk->ClearFlags(VDS_DF_READ_ONLY);
                if(!SUCCEEDED(hResult))
                {
                  LogAndPrint("\n Could not clear the Read-Only flag of the disk.hResult = 0x%x\n",hResult);
                }
				if(gTotalVolnMountPointsCount != 0)
				{
					Sleep(3000);//Sleep until the disk and all its volumes and mount points come online.
				}
				else
				{
					Sleep(15000);
				}
				
                LogAndPrint("\nBringing disk %s [Disk Friendly Name = %s ]to Online.",W2A(vdsInmDiskProps.pwszDevicePath),W2A(vdsInmDiskProps.pwszFriendlyName));//MCHECK
              }
              CleanUpAndRelease(pVdsDiskOnline);
            }
          }
        }
        else
        {
			LogAndPrint("\Unable to get disk properties.hResult = 0x%x\n",hResult);
          CleanUpAndRelease(pInmDisk);
          return hResult;
        }
        CleanUpAndRelease(pInmDisk);
      }
      else
      {
        hResult = E_UNEXPECTED;
        LogAndPrint("\n Failed: Pointer to IUnknown Interface could not be obtained.hResult = 0x%x\n",hResult);
        CleanUpAndRelease(pInmEnumVdsDisk);
        return hResult;
      }
    }
    CleanUpAndRelease(pInmEnumVdsDisk);
    CleanUpAndRelease(pIVdsPack);    
  }
  CleanUpDiskProps(vdsInmDiskProps);
  return hResult;
}
//*****************************************************************************
//This function first enumerates all the disks then brings the required disks
//to Online mode.
//*****************************************************************************
HRESULT BringDisksOnline()
{
	HRESULT           hResult;
	IVdsService       *pInmService         = NULL;
	IVdsServiceLoader *pInmLoader          = NULL;
	IVdsDisk          *pInmDisk            = NULL;
	IUnknown          *pIUnknownIntf        = NULL;
	IVdsSwProvider    *pIVdsSwPrvdr  = NULL;
	IEnumVdsObject    *pEnumIVdsPackIntf    = NULL;
	unsigned long     ulFetched         = 0;

	EnableDebugPriv();

	// For first get a pointer to the VDS Loader
	hResult = CoInitialize(NULL);

	if (SUCCEEDED(hResult))
	{
		LogAndPrint("\n CoInitialize Succeeded.\n");
		hResult = CoCreateInstance( CLSID_VdsLoader,
		NULL,
		CLSCTX_LOCAL_SERVER,
		IID_IVdsServiceLoader,
		(void **) &pInmLoader);

		//Launch the VDS service.;and then load VDS on the local computer.
		if (SUCCEEDED(hResult))
		{
			LogAndPrint("\n LoadService Passed.\n");
			hResult = pInmLoader->LoadService(NULL, &pInmService);
		}
		else
		{
			LogAndPrint("\n LoadService failed.hResult = 0x%x\n",hResult);
		}
		if (SUCCEEDED(hResult))
		{
			hResult = pInmService->WaitForServiceReady();
			if (SUCCEEDED(hResult))
			{
				LogAndPrint("\nVDS Service Loaded\n");
				IEnumVdsObject *pProviderEnum;
				hResult = pInmService->QueryProviders(VDS_QUERY_SOFTWARE_PROVIDERS | VDS_QUERY_HARDWARE_PROVIDERS,&pProviderEnum);
				if(!SUCCEEDED(hResult))
				{
					LogAndPrint("\n Error:Could not get the Software Providers of VDS.hResult = 0x%x\n",hResult);
					CleanUpAndRelease(pInmService);
					return hResult;
				}
				else
				{
					LogAndPrint("\n Got the Software Providers of VDS.\n");
				}
				CleanUpAndRelease(pInmService);

				while(pProviderEnum && (pProviderEnum->Next(1,(IUnknown**)&pIUnknownIntf,&ulFetched) == 0x00))
				{
					if(!pIUnknownIntf)
					{
						LogAndPrint("\nFailed:Pointer to pIUnknownIntf interface could not be obtained for the provider.Provider Count = %d",ulFetched);
						CleanUpAndRelease(pProviderEnum);
						return -1;
					}
					assert(1 == ulFetched);
					hResult = pIUnknownIntf->QueryInterface(IID_IVdsSwProvider,(void**)&pIVdsSwPrvdr);
					if(SUCCEEDED(hResult) && (NULL != pIVdsSwPrvdr))
					{
						hResult = pIVdsSwPrvdr->QueryPacks(&pEnumIVdsPackIntf);
						if(!SUCCEEDED(hResult))
						{
							LogAndPrint("\nError: Unable to get the Pack from the Provider.hResult = 0x%x\n",hResult);
							CleanUpAndRelease(pIVdsSwPrvdr);
							return hResult;
						}
						else
						{
							LogAndPrint("\nGot the Pack from the Provider.\n");
						}
						CleanUpAndRelease(pIVdsSwPrvdr);

						hResult = ProcessVdsPacks(&pEnumIVdsPackIntf);
						if(!SUCCEEDED(hResult))
						{
							LogAndPrint("\n Unable to bring some or all of the disks Online.\n. Result = 0x%x",hResult);
							CleanUpAndRelease(pEnumIVdsPackIntf);
							return hResult;
						}
						else
						{
							LogAndPrint("\n Brought some or all of the disks Online.\n");
						}
						CleanUpAndRelease(pEnumIVdsPackIntf);
					}
					else 
					{
						LogAndPrint("\n IVdsSubSystem failed. Result = 0x%x",hResult);
						CleanUpAndRelease(pIUnknownIntf);
						return hResult;
					}
					CleanUpAndRelease(pIUnknownIntf);
				}//end of while
				CleanUpAndRelease(pProviderEnum);
			}//end of checking the condition for WaitForServiceReady
			else
			{
				LogAndPrint("Failed to load VDS Service.hResult = 0x%x\n",hResult);
			}
		}
		else
		{
			LogAndPrint("VDS Service failed hr=0x%x\n",hResult);
		}
		CleanUpAndRelease(pInmLoader); 
	}
	else
	{
		LogAndPrint("Failed to create CoCreateInstance hr=0x%x\n",hResult);
	}
	return hResult;
}
//#endif //End of code specific to Windows Server 2008 and above.

int _tmain(int argc, _TCHAR* argv[])
{
	TerminateOnHeapCorruption();
	init_inm_safe_c_apis();
	//LogAndPrint("\n******************************************************************************************\n");
	//LogAndPrint("\n\t\t Start of logging in ClusUtil.exe.\n");	
	int iRet = RET_SUCCESS; //0 and RET_FAILURE = 1
	int iParsedOptions = 1;

	bool bShutdown = 0;
	bool bPrepare = 0;
	bool bForce = 0;
	bool bRebootActiveNode = true;
	bool bCleanUpCXDataBase = true;
	bool bClusToStandalone = 0;
	bool bStandaloneToClus = 0;
	int iPrepareType = -1;
	bool bAllocated = 0;

	char *pListOfSystemsTobeShutdown		= NULL;
	char *pListOfSystemsStopVxagent			= NULL;
	char *pListOfSystemsForDbClean			= NULL;
	char *pListOfSystemsStopClusterService	= NULL;
	char *pSystemTobeRestarted				= NULL;
	char *pPrepareArgs						= NULL;
			
	int OperationFlag = -1;
		
	if(argc < 3 )
	{
		ClusUtil_usage();
		CloseLogFile();//Close the Log File
		exit(1);
	}

	for (;iParsedOptions < argc; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if(stricmp((argv[iParsedOptions]+1),OPTION_PREPARE) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					LogAndPrint("Incorrect argument passed\n");
					ClusUtil_usage();
					CloseLogFile();
					exit(1);
				}
				pPrepareArgs = argv[iParsedOptions];
				if(!ParsePrepareArgs(pPrepareArgs,iPrepareType,&pSystemTobeRestarted))
				{
					ClusUtil_usage();
					CloseLogFile();
					exit(1);
				}
				bPrepare = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_SHUTDOWN) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					LogAndPrint("Incorrect argument passed\n");
					ClusUtil_usage();
					CloseLogFile();
					exit(1);
				}
				pListOfSystemsTobeShutdown = argv[iParsedOptions];
				bShutdown= true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_FORCE) == 0)
            {
				bForce = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_NO_ACTIVE_NODE_REBOOT) == 0)
			{
				bRebootActiveNode = false;
			}
			else if(stricmp((argv[iParsedOptions]+1),OPTION_NO_CX_DB_CLEANUP) == 0)
			{
				bCleanUpCXDataBase = false;
			}
			else
			{
				LogAndPrint("Incorrect argument passed\n");
				ClusUtil_usage();
				CloseLogFile();
				exit(1);
			}
		}
		else
		{
			LogAndPrint("Incorrect argument passed\n");
			ClusUtil_usage();
			CloseLogFile();
			exit(1);
		}
	}

	/*if((bForce == true) &&  (bShutdown == false))
	{
		LogAndPrint("-force can not be used without -shutdown");
		ClusUtil_usage();
	}*/
	
	if(iPrepareType == STANDALONE_TO_CLUSTER_TYPE)
	{
		LogAndPrint("\n\t Entered to Convert the Standalone Machine to Cluster...\n.");

		if(!EnableDrivers(CLUSTER_DISK_DRIVER_SERVICE, pSystemTobeRestarted, NULL,NULL,NULL))
		{
			LogAndPrint("\n Unable to Enable Cluster Disk Driver and hence stopping Conversion from Standalone To Cluster!\n");
		}
		
		if(!changeServiceType(std::string(CLUSTER_SERVICE),SVCTYPE_AUTO))
		{
			iRet = RET_FAILURE;
			LogAndPrint("\n Failed to change the Cluster Service start type. Hence exiting ClusUtil...\n");
		}

		if(!StartServices(VX_SERVICE,pSystemTobeRestarted, NULL,NULL,NULL))//Start the SV Agents before restart
		{
			LogAndPrint("\n Unable to Start the SV Agents and hence stopping Conversion from Standalone To Cluster!\n");
		}
		
		if(bRebootActiveNode)
		{
			LogAndPrint("\n\t Waiting for a few seconds before rebooting the system...\n");
			Sleep(5000);

		RestartSystems(pSystemTobeRestarted,bForce, NULL,NULL,NULL);	

			DWORD dwRestartValue = GetLastError();
			LPVOID lpInmMsgBuf;
			::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
						FORMAT_MESSAGE_FROM_SYSTEM,
					    NULL,
						dwRestartValue,
						0,
						(LPTSTR)&lpInmMsgBuf,
						0,
						NULL);
			if(dwRestartValue)
			{
				LogAndPrint("\n Restart stauts of the current node is Re-start status = %d, Description :%s \n",dwRestartValue,(LPCTSTR)lpInmMsgBuf);
			}
			LocalFree(lpInmMsgBuf);
		}
		CloseLogFile();
		
	}
	else  if(iPrepareType == CLUSTER_TO_STANDALONE_TYPE)
	{
		LogAndPrint("\nConverting Cluster Node to a Standalone Node...\n");

		//Check whether is Cx Server is up or not
		if(RET_SUCCESS != GetCxUpStatus())
		{
			LogAndPrint("\nCx Server is down and hence cannot Convert Cluster to Standalone node.\n");
			iRet = RET_FAILURE;
			return iRet;
		}
		else
		{
			LogAndPrint("\nCx Server is Up and running!\n");
		}
		char* pchCmdLine[3] = {"WinOp","MAPDRIVELETTERS","-s"};
		int nMapStatus = MapDriveLetterMain(3,pchCmdLine);
		if(-1 == nMapStatus)
		{	
			LogAndPrint("\n Error in Mapping Drive Letters");
			LogAndPrint("\nHence cannot proceed to convert Cluster Node to a Standalone Node.\nExiting...");
			iRet = RET_FAILURE;
			return iRet;
		}
		else
		{
			LogAndPrint("\n Successfully Mapped the Drive Letters\n");	
		}
		if(pListOfSystemsTobeShutdown)
		{
            size_t dbcleanlen;
            INM_SAFE_ARITHMETIC(dbcleanlen = InmSafeInt<size_t>::Type(strlen(pListOfSystemsTobeShutdown)) + strlen(pSystemTobeRestarted)+2, INMAGE_EX(strlen(pListOfSystemsTobeShutdown))(strlen(pSystemTobeRestarted)))
			pListOfSystemsForDbClean = new char[dbcleanlen];
			inm_sprintf_s(pListOfSystemsForDbClean, dbcleanlen, "%s,%s", pSystemTobeRestarted, pListOfSystemsTobeShutdown);
		}
		else
		{
            size_t dbcleanlen;
            INM_SAFE_ARITHMETIC(dbcleanlen = InmSafeInt<size_t>::Type(strlen(pSystemTobeRestarted))+2, INMAGE_EX(strlen(pSystemTobeRestarted)))
			pListOfSystemsForDbClean = new char[dbcleanlen];
			inm_sprintf_s(pListOfSystemsForDbClean, dbcleanlen,"%s", pSystemTobeRestarted);
		}
        size_t restartlen;
        INM_SAFE_ARITHMETIC(restartlen = InmSafeInt<size_t>::Type(strlen(pSystemTobeRestarted))+2, INMAGE_EX(strlen(pSystemTobeRestarted)))
		pListOfSystemsStopVxagent = new char[restartlen];
		
		inm_sprintf_s(pListOfSystemsStopVxagent, restartlen,"%s", pSystemTobeRestarted);
		
		pListOfSystemsStopClusterService = new char[restartlen];
		
		inm_sprintf_s(pListOfSystemsStopClusterService, restartlen,"%s", pSystemTobeRestarted);
		bAllocated = true;
		
		do
		{
			if(pListOfSystemsTobeShutdown != NULL)
			{	
				//Store the list of systems to be shutdown in a string
				string strListOfSystemsTobeShutdown = pListOfSystemsTobeShutdown;
				
				if(!ShutdownSystems(pListOfSystemsTobeShutdown, bForce, NULL,NULL,NULL))
				{
					LogAndPrint("Will not restart [%s] system as shutdown operation for some other node in Cluster is failed\n",pSystemTobeRestarted);
					iRet =  RET_FAILURE;
					break;
				}
				//Add code to check if the other nodes were switched off or not by pinging each node.
				vector<char*> vNodeNames;
				if(true == CheckOtherNodesShutdownStatus((char*)strListOfSystemsTobeShutdown.c_str()))
				{
					LogAndPrint("\n All the other Cluster Nodes are shutdown successfully.\n");
				}
				else
				{
					if(false == bForce)
					{
						LogAndPrint("\nEither one of the machines to be shutdown is locked or still shutting down.\n");
						continue;
					}
					else
					{
						LogAndPrint("\n Other nodes have not completed shutting down...\n");
						LogAndPrint("\n So terminating the application, not proceeding ahead.\n");
						break;
					}       
				}
			}			
			if(!StopServices(VX_SERVICE, pListOfSystemsStopVxagent, NULL,NULL,NULL))
			{
				iRet =  RET_FAILURE;
				LogAndPrint("\n Failed to stop the SV Agents. Hence exiting ClusUtil...\n");
				break;
			}
			else
			{
				LogAndPrint("\n Successfully stopped the SV Agents.\n");
			}
			//This will change the cluster service type on local system.
			if(changeServiceType(std::string(CLUSTER_SERVICE),SVCTYPE_MANUAL) == SVE_FAIL)
			{
				iRet = RET_FAILURE;
				LogAndPrint("\n Failed to change the Cluster Service start type. Hence exiting ClusUtil...\n");
				break;
			}
			//Stop the Cluster Service CLUSTER_SERVICE
			if(!StopServices(CLUSTER_SERVICE, pListOfSystemsStopClusterService, NULL,NULL,NULL))
			{
				iRet =  RET_FAILURE;
				LogAndPrint("\n Failed to stop the Cluster Service. Hence exiting ClusUtil...\n");
				break;
			}
			else
			{
				LogAndPrint("\n Successfully stopped the Cluster Service.\n");
			}
			
			//if cx cleanup option is specified
			if(bCleanUpCXDataBase)
			{
				if(!CX_CleanDb(pListOfSystemsForDbClean))
				{
					iRet =  RET_FAILURE;
					LogAndPrint("\n Failed to clean the Cx's Database. Hence exiting ClusUtil...\n");
					break;
				}
			}
	
			if(!DisableDrivers(CLUSTER_DISK_DRIVER_SERVICE, pSystemTobeRestarted, NULL,NULL,NULL))
			{
				iRet =  RET_FAILURE;
				LogAndPrint("\n Failed to disable the Cluster Disk Driver Service. Hence exiting ClusUtil...\n");
				break;
			}

			if(!RemoveRegKey(SVAGENT_CLUSTER_TRACKING_REG_KEY,pSystemTobeRestarted))
			{
				iRet =  RET_FAILURE;
				LogAndPrint("\n Failed to remove the SOFTWARE\\SV Systems\\ClusterTracking registry hive. Hence exiting ClusUtil...\n");
				break;
			}
			if(bRebootActiveNode)
			{
				LogAndPrint("\nAbout to reboot the system to make the Cluster Node as a Standalone Node...");
				Sleep(10000);//Wait for 5 seconds before rebooting the node.
				if(!RestartSystems(pSystemTobeRestarted,bForce, NULL,NULL,NULL))
				{
					iRet =  RET_FAILURE;
					LogAndPrint("\n Failed to restart this Cluster Node. Hence exiting ClusUtil...\n");
				
					DWORD dwRestartValue = GetLastError();
					LPVOID lpInmMsgBuf;
					::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
								FORMAT_MESSAGE_FROM_SYSTEM,
								NULL,
								dwRestartValue,
								0,
								(LPTSTR)&lpInmMsgBuf,
								0,
								NULL);
					if(dwRestartValue)
					{
						LogAndPrint("\n Current node Re-Start status = %d, Description :%s \n",dwRestartValue,(LPCTSTR)lpInmMsgBuf);
					}
					LocalFree(lpInmMsgBuf);
					CloseLogFile();
					break;
				}
			}			
		}while(0);
	}
	else if(iPrepareType == ONLINE_DISK)
	{
		LogAndPrint("\n Entered to bring Disks,Volumes and Mount Points Online...\n");
		//Add code to bring all the earlier cluster disks to online from offline.
		OSVERSIONINFO osVerInfo;
		ZeroMemory(&osVerInfo,sizeof(osVerInfo));
		osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
		BOOL bVer = ::GetVersionEx(&osVerInfo);
		if(bVer)//if OS is Win2K8 Server or more, the disks will be in offline mode after restart.
		{
			if(osVerInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
			{
				//Just get the number of Disks,Volumes and MountPoints count
				vector<const char*>vVolumes;
				if(TRUE ==GetMountPointsList(vVolumes))//gTotalVolnMountPointsCount variable gets filled
				{
					//Wait for gTotalVolnMountPointsCount * 10 secs per disk
				}
				else
				{
					//Wait 15 secs per disk
				}

				//Before proceeding with making Offline disks as Online,
				//Stop the SV Agents, and Cluster Service to prevent any disk I/Os (Disk Writes)
				//For this first get the hostname
				char szHostName[MAX_PATH] = {0};
				if(true == GetHostName(szHostName,MAX_PATH))
				{
					if(!StopServices(VX_SERVICE, szHostName, NULL,NULL,NULL))
					{
						iRet =  RET_FAILURE;
						LogAndPrint("\n Failed to stop the SV Agents. Hence exiting ClusUtil...\n");
						goto MEM_CLEANUP;
					}
					else
					{
						LogAndPrint("\n Successfully stopped the SV Agents.\n");
					}

					//Stop the Cluster Service CLUSTER_SERVICE
					if(!StopServices(CLUSTER_SERVICE, szHostName, NULL,NULL,NULL))
					{
						iRet =  RET_FAILURE;
						LogAndPrint("\n Failed to stop the Cluster Service. Hence exiting ClusUtil...\n");
						goto MEM_CLEANUP;
					}
					else
					{
						LogAndPrint("\n Successfully stopped the Cluster Service.\n");
					}

					//Now bring the disks to online mode
					int nRetry = 0;
					do
					{
						if(!SUCCEEDED(BringDisksOnline()))
						{
							nRetry++;
							if(nRetry < NUM_RETRIES)
							{
								LogAndPrint("\nRetry Attempt: %d.",nRetry);
								LogAndPrint("\n Will retry bringing disks online after %d seconds.\n",(BRING_ONLINE_RETRY_INTERVAL/1000));
								Sleep(BRING_ONLINE_RETRY_INTERVAL);
							}
							else
							{
								LogAndPrint("\n Failed to Bring the Offline Disks to Online Mode.\n");
								iRet = RET_FAILURE;
								break;
							}
						}
						else
						{
							break;
						}
					}while(nRetry < NUM_RETRIES);

					if(RET_FAILURE == iRet)
						goto MEM_CLEANUP;

					if(gTotalVolnMountPointsCount != 0)
					{
						LogAndPrint("\n Please Wait until all the Disks,Volumes and Mount Points come Online...\n");
						Sleep(gTotalVolnMountPointsCount * 10000);						
					}
					else
					{
						Sleep(150000);//Sleep for 150 seconds
					}
					if(iRet != RET_FAILURE)
					{
						LogAndPrint("\n Successfully brought all the Offline Disks to Online.\n");
					}

					//Now Map the drive letters as they were before by spawning MapDriveLetters.exe
					
					char* pchCmdLine[3] = {"WinOp","MAPDRIVELETTERS.exe","-t"};
					int nMapStatus = MapDriveLetterMain(3,pchCmdLine);
					if(-1 == nMapStatus)
					{
						LogAndPrint("\n Failed to assign correct drive letters.\n");
						cout<<endl<<"Failed to assign correct drive letters.\n";
						iRet = RET_FAILURE;
					}
					
					//Even if the above step fails, you can continue to restart SV Agents as below...
					//Now start the SV Agents Services and no need to start the Cluster Service
					if(!StartServices(VX_SERVICE,szHostName, NULL,NULL,NULL))
					{
						//Here failure is not returned, as the User can try re-start the service manually and succeed!
						LogAndPrint("\n Could not Start the SV Agents.\nYou can try re-starting the SV Agents Service Manually!\n");
						//iRet = RET_FAILURE;
						//goto MEM_CLEANUP;
					}
					if(bRebootActiveNode)
					{
						//Restart the system to get the Drive Letters for the Removable Disks also
						//LogAndPrint("\nAbout to reboot the system to make the Cluster Node as a Standalone Node...");
						Sleep(10000);//Wait for 10 seconds before rebooting the node.
						if(!RestartSystems(pSystemTobeRestarted,bForce, NULL,NULL,NULL))
						{
							iRet =  RET_FAILURE;
							LogAndPrint("\n Failed to restart this Cluster Node. Hence exiting ClusUtil...\n");
							
							DWORD dwRestartValue = GetLastError();
							LPVOID lpInmMsgBuf;
							::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
										FORMAT_MESSAGE_FROM_SYSTEM,
										NULL,
										dwRestartValue,
										0,
										(LPTSTR)&lpInmMsgBuf,
										0,
										NULL);
							if(dwRestartValue)
							{
								LogAndPrint("\n Restart stauts of the current node is Error = %d, Description :%s \n",dwRestartValue,(LPCTSTR)lpInmMsgBuf);
							}
							LocalFree(lpInmMsgBuf);
							CloseLogFile();
						}
					}
				}
				else
				{
					iRet = RET_FAILURE;
					LogAndPrint("\n Cannot bring offline Disks to Online.");
				}
			}
			else
			{
				LogAndPrint("\n Bringing Disks Online option is required only for W2K8 Server.");
			}
		}
	}

MEM_CLEANUP:
	if(bAllocated && pListOfSystemsStopVxagent)
	{
		delete[] pListOfSystemsStopVxagent;
	}	
	if(bAllocated && pListOfSystemsForDbClean)
	{
		delete[] pListOfSystemsForDbClean;
	}
	CloseLogFile();
	return iRet;
}


DWORD RemoveRegKey(char* szSubKey,char* pSystemTobeRestarted)
{
	DWORD dwRet = true;

	HKEY hKey = NULL;
	HKEY hResult = NULL;
	char szSystemName[MAX_PATH+2] = {0};
	ZeroMemory(szSystemName,MAX_PATH);
	inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", pSystemTobeRestarted);

	if(RegConnectRegistry(szSystemName,HKEY_LOCAL_MACHINE,&hKey) == ERROR_SUCCESS)
	{
		if(SHDeleteKey(hKey,szSubKey)!= ERROR_SUCCESS)
		{
			dwRet = GetLastError();

			LogAndPrint("Failed to delete HKLM\\%s key\n",szSubKey);

			LPVOID lpInmMsgBuf;
			::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwRet,
			0,
			(LPTSTR)&lpInmMsgBuf,
			0,
			NULL);

			if(dwRet)
			{
				LogAndPrint("Error:[%d] %s\n",dwRet,(LPCTSTR)lpInmMsgBuf);
			}
		
			LocalFree(lpInmMsgBuf);
		}
	}
	else
	{
		LogAndPrint("\nFailed to connect [%s] registry\n",szSystemName);
		dwRet = false;
	}

	return dwRet;
}


DWORD CX_CleanDb(char* pListOfSystems)
{
	DWORD dwRet = true;
	LocalConfigurator config;
	char pszURL[32] = {0};
	inm_strcpy_s( pszURL, ARRAYSIZE(pszURL),CLEANUP_CLUSTER_DATABASE_SCRIPT );

	do
	{
		if(!pListOfSystems)
		{
			LogAndPrint("No system name found for cleandb\n");
			dwRet = false;
			break;
		}
		
		char szBuffer[ REGISTER_BUFSIZE ];

		// construct the registration string
		
		memset(szBuffer,0,REGISTER_BUFSIZE);
		
		inm_sprintf_s(szBuffer, ARRAYSIZE(szBuffer), "node_list_to_clean=%s", pListOfSystems);
		szBuffer[strlen(szBuffer)] = '\0';

		LogAndPrint("Send Buffer=%s\n",szBuffer);

		if (strlen(szBuffer) > 0 ) 
		{
            if (postToCx(GetCxIpAddress().c_str(), config.getHttp().port, pszURL, szBuffer, NULL, config.IsHttps()).failed())
			{
                LogAndPrint("Failed to post to CX server: %s", GetCxIpAddress().c_str());
				dwRet = false;
				break;
			}
		}
	}while(0);
	
	return dwRet;
}

void ClusUtil_usage()
{
	LogAndPrint("Usage:\n");
	OSVERSIONINFO osVerInfo;
	ZeroMemory(&osVerInfo,sizeof(osVerInfo));
	osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
	BOOL bVer = ::GetVersionEx(&osVerInfo);
	if(bVer)//if OS is Win2K8 Server or more, the disks will be in offline mode after restart.
	{
		if(osVerInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
		{
			LogAndPrint("ClusUtil.exe -prepare { ClusterToStandalone:<System1> [[-shutdown <System2,System3,...>] [-force][-noCxDbCleanup]]  | StandaloneToCluster:<System1>   | OnlineDisk }  \n");
		}
		else
		{
			LogAndPrint("ClusUtil.exe -prepare { ClusterToStandalone:<System1> [[-shutdown <System2,System3,...>] [-force] [-noCxDbCleanup]]  | StandaloneToCluster:<System1> }  \n");
		}
	}	
}


DWORD ParsePrepareArgs(char* pPrepareArgs,int& iPrepareType, char** ppSystemName)
{
	DWORD dwRet = TRUE;

	do
	{
		if(!_strnicmp(SZ_CLUS_TO_STANDALONE,pPrepareArgs, strlen(SZ_CLUS_TO_STANDALONE)))
		{
			iPrepareType = CLUSTER_TO_STANDALONE_TYPE;
		}
		else if(!_strnicmp(SZ_STANDALONE_TO_CLUS,pPrepareArgs, strlen(SZ_STANDALONE_TO_CLUS)))
		{
			iPrepareType = STANDALONE_TO_CLUSTER_TYPE;
		}
		else if(!_strnicmp(SZ_ONLINE_DISK,pPrepareArgs, strlen(SZ_ONLINE_DISK)))
		{
				iPrepareType = ONLINE_DISK;
		}
		else
		{
			LogAndPrint("Invalid prepare type. It has to be either ClusterToStandalone or StandaloneToCluster type");
			dwRet = false;
			break;
		}

		if(strnicmp(SZ_ONLINE_DISK,pPrepareArgs,strlen(SZ_ONLINE_DISK)))
		{
			char* pNodeName = strchr(pPrepareArgs,':');
			if(strlen(pNodeName) < 2)
			{
				LogAndPrint("No node name is provided. Invalid argument.\n");
				dwRet =  false;
				break;
			}
		
			pNodeName++;
			*ppSystemName = pNodeName;
		}
		

	}while(0);

	return dwRet;
		
}
