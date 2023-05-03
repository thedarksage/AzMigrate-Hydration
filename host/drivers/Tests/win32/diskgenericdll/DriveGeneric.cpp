//Some of the call uses WMI to get the desired results.
// This dynamic libray meant for run-time loading and requires export interface, As this code is written C we need to export C interface
#include <windows.h>
#include <iostream>
using namespace std;

#include <Wbemidl.h>
#include <comutil.h>
# pragma comment(lib, "wbemuuid.lib")
# pragma comment(lib, "comsuppw.lib")//for COM.

#include <string>
#include <winioctl.h>
#include <winbase.h>
#include <stdio.h>
#include <conio.h>

#include <stdlib.h>

#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")//for StrStrI function

//This include is only to cater the SCSI inquiry code using pass throug direct
#define _NTSCSI_USER_MODE_ 1 //MUST. DON't REMOVE THIS

namespace NT{
#include <ntddscsi.h>    //change this accordingly.
#include <ntdddisk.h>    
#include <ntddstor.h>
#include <devioctl.h>
#include <scsi.h>
}

#define SafeCloseHandle(h) if (INVALID_HANDLE_VALUE != h) {CloseHandle(h); h = INVALID_HANDLE_VALUE;}
#include <memory>    
using namespace std;

//only for future use. If we want to use globally available static variables across processes.
#pragma data_seg (".DiskDrive")
   int i = 0; 
   //char a[32] = "hello world";
#pragma data_seg()


typedef struct _EMK_DISK_SIGNATURE{
 
	unsigned int DriveCount;
	unsigned long **Signature; 
	wchar_t **DriveNames;
}EMK_DISK_SIGNATURE,*PEMK_DISK_SIGNATURE;
//Exported Functions;
#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)
//Intrinsic Functions.
int ShowLastError();
//EXTERN_DLL_EXPORT bool GetSCISIDiskCapacityByName(wchar_t *DeviceLink,DWORD&Sectors,DWORD&BytesPerSector);
//EXTERN_DLL_EXPORT PEMK_DISK_SIGNATURE GetAllDiskNamesAndSignatures();
//EXTERN_DLL_EXPORT BOOL GetDriveGeometry(DISK_GEOMETRY *pdg,wchar_t *DiskDeviceName);
//EXTERN_DLL_EXPORT INT64 FullDriveCloneSynchronous(wchar_t *SourceDisk, wchar_t *TargetDisk);
//#pragma argsused


BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{

	switch(dwReason){

		case DLL_PROCESS_ATTACH:
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;

	}
	return TRUE;
}


EXTERN_DLL_EXPORT void testsuresh(unsigned long* test)
{
	printf(" Test message from DLL\n");
	*test = 1234;
}
EXTERN_DLL_EXPORT bool GetAllDiskNamesAndSignatures(PEMK_DISK_SIGNATURE pDiskDetails)
{
	bool ret = FALSE;
    BSTR strNetworkResource;
	size_t len;
	
    //To use a WMI remote connection set localconn to false and configure the values of the pszName, pszPwd and the name of the remote machine in strNetworkResource
    strNetworkResource = L"\\\\.\\root\\CIMV2";

    COAUTHIDENTITY *userAcct =  NULL ;

    // Initialize COM. ------------------------------------------

    HRESULT hres;
    hres =  CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
		printf("1\n");
        cout << "Failed to initialize COM library. Error code = 0x" << hex << hres << endl;
        //cout << _com_error(hres).ErrorMessage() << endl;
        //cout << "press enter to exit" << endl;
        //cin.get();      
        return ret;                  // Program has failed.
    }

    // Set general COM security levels --------------------------


        hres =  CoInitializeSecurity(
            NULL,
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities
            NULL                         // Reserved
            );

    if (FAILED(hres))
    {
		printf("2\n");
		cout << "Failed to initialize security. Error code = 0x" << hex << hres << endl;
        //cout << _com_error(hres).ErrorMessage() << endl;
        CoUninitialize();
        //cout << "press enter to exit" << endl;
        //cin.get();      
        return ret;                    // Program has failed.
    }

    // Obtain the initial locator to WMI -------------------------

    IWbemLocator *pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc);

    if (FAILED(hres))
    {
		printf("3\n");
		cout << "Failed to create IWbemLocator object." << " Err code = 0x" << hex << hres << endl;
        //cout << _com_error(hres).ErrorMessage() << endl;
        CoUninitialize();       
        //cout << "press enter to exit" << endl;
        //cin.get();      
        return ret;                 // Program has failed.
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method

    IWbemServices *pSvc = NULL;

        hres = pLoc->ConnectServer(
             _bstr_t(strNetworkResource),      // Object path of WMI namespace
             NULL,                    // User name. NULL = current user
             NULL,                    // User password. NULL = current
             0,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             0,                       // Authority (e.g. Kerberos)
             0,                       // Context object
             &pSvc                    // pointer to IWbemServices proxy
             );

    if (FAILED(hres))
    {
		printf("4\n");
        cout << "Could not connect. Error code = 0x" << hex << hres << endl;    
        //cout << _com_error(hres).ErrorMessage() << endl;
        pLoc->Release();
        CoUninitialize();
        //cout << "press enter to exit" << endl;
        //cin.get();          
        return ret;                // Program has failed.
    }

    cout << "Connected to root\\CIMV2 WMI namespace" << endl;

    // Set security levels on the proxy -------------------------
        hres = CoSetProxyBlanket(
           pSvc,                        // Indicates the proxy to set
           RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
           RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
           NULL,                        // Server principal name
           RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
           RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
           NULL,                        // client identity
           EOAC_NONE                    // proxy capabilities
        );

		
	//printf(" security level : %x \n", hres);
    if (FAILED(hres))
    {
		printf("5\n");
		cout << "Could not set proxy blanket. Error code = 0x" << hex << hres << endl;
        //cout << _com_error(hres).ErrorMessage() << endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        //cout << "press enter to exit" << endl;
        //cin.get();      
        return ret;               // Program has failed.
    }

	//printf(" 6\n");
    // Use the IWbemServices pointer to make requests of WMI ----
	
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery( L"WQL", L"SELECT * FROM Win32_DiskDrive",
    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

    if (FAILED(hres))
    {
		printf("7\n");
        cout << "ExecQuery failed" << " Error code = 0x"    << hex << hres << endl;
        //cout << _com_error(hres).ErrorMessage() << endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        //cout << "press enter to exit" << endl;
        //cin.get();      
        return ret;               // Program has failed.
    }

    // Secure the enumerator proxy

	 //printf("8\n");
        hres = CoSetProxyBlanket(
            pEnumerator,                    // Indicates the proxy to set
            RPC_C_AUTHN_DEFAULT,            // RPC_C_AUTHN_xxx
            RPC_C_AUTHZ_DEFAULT,            // RPC_C_AUTHZ_xxx
            COLE_DEFAULT_PRINCIPAL,         // Server principal name
            RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  // RPC_C_AUTHN_LEVEL_xxx
            RPC_C_IMP_LEVEL_IMPERSONATE,    // RPC_C_IMP_LEVEL_xxx
            userAcct,                       // client identity
            EOAC_NONE                       // proxy capabilities
            );



    // Get the data from the WQL sentence
    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;
	UINT count = 0;

//#define MAX_DRIVES 10
//
//	DiskDetail = (PEMK_DISK_SIGNATURE)malloc(sizeof(EMK_DISK_SIGNATURE));
//	
//
//	DiskDetail->DriveNames = (wchar_t **)malloc(MAX_DRIVES * sizeof(wchar_t*));
//	DiskDetail->Signature = (unsigned long **)malloc(MAX_DRIVES * sizeof(unsigned long*));
//	DiskDetail->DriveCount = 0;
	/*if (NULL == pDiskDetails)
	{
		printf("failed to allocate memory \n");
		return ret;
	}*/
	//printf(" Before - DriveCount : %d \n", pDiskDetails->DriveCount);
	while ((pEnumerator) && (pDiskDetails->DriveCount < 100))
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if(0 == uReturn || FAILED(hr))
          break;

        VARIANT vtProp;

                hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);// String
                if (!FAILED(hr))
                {
					if ((vtProp.vt==VT_NULL) || (vtProp.vt==VT_EMPTY)){
                    //wcout << "Name : " << ((vtProp.vt==VT_NULL) ? "NULL" : "EMPTY") << endl;
					}
                  else
					  if ((vtProp.vt & VT_ARRAY)){
                    //wcout << "Name : " << "Array types not supported (yet)" << endl;
					  }
				  else{
                   // wcout << "DLL_Name : " << vtProp.bstrVal << endl;
					len = (wcslen(vtProp.bstrVal) * sizeof(WCHAR)) + sizeof(WCHAR);
					pDiskDetails->DriveNames[count] = (wchar_t *)malloc(len);
					memset(pDiskDetails->DriveNames[count], '\0', len);
					memcpy_s(pDiskDetails->DriveNames[count], len, vtProp.bstrVal, len - sizeof(WCHAR));
					pDiskDetails->DriveCount++;
				  }
                }
                VariantClear(&vtProp);

                hr = pclsObj->Get(L"Signature", 0, &vtProp, 0, 0);// Uint32
                if (!FAILED(hr))
                {
					if ((vtProp.vt==VT_NULL) || (vtProp.vt==VT_EMPTY)){
                    //wcout << "Signature : " << ((vtProp.vt==VT_NULL) ? "NULL" : "EMPTY") << endl;
					}
                  else
					  if ((vtProp.vt & VT_ARRAY)){
                    //wcout << "Signature : " << "Array types not supported (yet)" << endl;
					  }
				  else{
                    //wcout << "DLL_Signature : " << hex << vtProp.uintVal << endl;
					pDiskDetails->Signature[count] = (unsigned long *)malloc(sizeof(unsigned long));
					*pDiskDetails->Signature[count] = vtProp.uintVal;
				  }
                }
                VariantClear(&vtProp);

        count++;
        pclsObj->Release();
        pclsObj=NULL;
    }//end of while

    // Cleanup

    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    if (pclsObj!=NULL)
     pclsObj->Release();

    CoUninitialize();
    //cout << "press enter to exit" << endl;
	
	//printf(" After - DriveCount : %d \n", pDiskDetails->DriveCount);
	//for (unsigned int i = 0; i < pDiskDetails->DriveCount; i++){
	//	printf("\nDrive ID[%d]:%s", i, pDiskDetails->DriveNames[i]);
	//	if (NULL != pDiskDetails->Signature[i])
	//		printf("\nDrive Signature[%d]:%x\n", i, *pDiskDetails->Signature[i]);
	//	else
	//		printf("\n Might be a GPT disk \n");
	//}
	//printf("returning GetAllDiskSign \n");
	ret = true;
    return ret;   // Program successfully completed.
}
//
//Display to the command liie the last Error and associated message
int ShowLastError()
{
	LPVOID lpMsgBuf;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
	wprintf(L"ERROR - %ws\n", lpMsgBuf );
	LocalFree( lpMsgBuf );
	return(0);
}
//
//Get physical device parameters
//Important. IOCTL_DISK_GET_DRIVE_GEOMETRY gives you the disk geometry held
//by the Disk Class driver and not the one reported by SCSI port driver even
//though we query raw disk. For this we have another Function GetSCISIDiskCapacityByName()
//which basically uses SCSIOP_READ_CAPACITY and is the most reliabe.
//Note that IOCTL_DISK_GET_DRIVE_GEOMETRY might give you lesser sectors on disk
//than the real one. This always matter when we image or clone entire disk. So
//alway for clone or image rely on SCSIOP_READ_CAPACITY feature.
EXTERN_DLL_EXPORT BOOL GetDriveGeometry(DISK_GEOMETRY *pdg,wchar_t *DiskDeviceName)
{
  HANDLE hDevice;               // handle to the drive to be examined 
  DWORD BytesReturned;                   // discard results
  BOOL bResult;

  hDevice = CreateFileW(DiskDeviceName,          // drive to open
                    0,                // no access to the drive
                    FILE_SHARE_READ | // share mode
                    FILE_SHARE_WRITE, 
                    NULL,             // default security attributes
                    OPEN_EXISTING,    // disposition
                    0,                // file attributes
                    NULL);            // do not copy file attributes

  if (INVALID_HANDLE_VALUE == hDevice) // cannot open the drive
  {
    return (FALSE);
  }

// FSCTL_READ_FROM_PLEX 
// IOCTL_DISK_READ
// IOCTL_DISK_WRITE
  bResult = DeviceIoControl(hDevice,		// device to be queried
			IOCTL_DISK_GET_DRIVE_GEOMETRY,  // operation to perform
			NULL, 0, 		// no input buffer
			pdg, sizeof(*pdg),	// output buffer
			&BytesReturned,			// # bytes returned
			(LPOVERLAPPED) NULL);	// synchronous I/O

  CloseHandle(hDevice);

  return (bResult);
}
//
#define SENSEBUFSIZE 32    
#define DATABUFSIZE 0xFC    
#pragma pack(push,4)    
typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS {   
    NT::SCSI_PASS_THROUGH spt;   
    ULONG             Filler;      // realign buffers to double word boundary    
	UCHAR             ucSenseBuf[SENSEBUFSIZE];
	UCHAR             ucDataBuf[DATABUFSIZE];
} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;   
   
typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {   
    NT::SCSI_PASS_THROUGH_DIRECT sptd;   
    ULONG             Filler;      // realign buffer to double word boundary    
    UCHAR             ucSenseBuf[SENSEBUFSIZE];   
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;   
#pragma pack(pop)    

EXTERN_DLL_EXPORT bool GetSCISIDiskCapacityByName(wchar_t *DeviceLink,DWORD&Sectors,DWORD&BytesPerSector){   
    bool success=false;Sectors=BytesPerSector=0;
    
    HANDLE const hDrive=CreateFileW(DeviceLink,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);   
    DWORD returned;   
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;   
    NT::CDB&Cdb=(NT::CDB&)sptwb.spt.Cdb; 

	if(!hDrive){
		ShowLastError();
		return false;
	}

    memset(&sptwb,0,sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));   
    sptwb.spt.Length = sizeof(NT::SCSI_PASS_THROUGH);   
    //sptwb.spt.PathId = 0;    
    //sptwb.spt.TargetId = 0;    
    //sptwb.spt.Lun = 0;    
    sptwb.spt.CdbLength = CDB10GENERIC_LENGTH;   
    sptwb.spt.SenseInfoLength = SENSEBUFSIZE;   
    sptwb.spt.DataIn = SCSI_IOCTL_DATA_IN;   
    sptwb.spt.DataTransferLength = DATABUFSIZE;   
    sptwb.spt.TimeOutValue = 10;   
    sptwb.spt.DataBufferOffset =offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf);   
    sptwb.spt.SenseInfoOffset =offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucSenseBuf);   
    sptwb.spt.Cdb[0] = SCSIOP_READ_CAPACITY;   
    DWORD length=offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf)+sptwb.spt.DataTransferLength;   
    BOOL status=DeviceIoControl(hDrive,IOCTL_SCSI_PASS_THROUGH,&sptwb,sizeof(NT::SCSI_PASS_THROUGH),&sptwb,length,&returned,NULL);   
    if(status&&!sptwb.spt.ScsiStatus){   
        if((sptwb.spt.DataTransferLength>7)&&(returned==sptwb.spt.DataTransferLength+sptwb.spt.DataBufferOffset)){   
            union{   
                DWORD u;   
                BYTE by[4];   
            }tmp;   
            tmp.by[3]=sptwb.ucDataBuf[0];   
            tmp.by[2]=sptwb.ucDataBuf[1];   
            tmp.by[1]=sptwb.ucDataBuf[2];   
            tmp.by[0]=sptwb.ucDataBuf[3];   
            if(++tmp.u){   
                Sectors=tmp.u;   
                tmp.by[3]=sptwb.ucDataBuf[4];   
                tmp.by[2]=sptwb.ucDataBuf[5];   
                tmp.by[1]=sptwb.ucDataBuf[6];   
                tmp.by[0]=sptwb.ucDataBuf[7];   
                BytesPerSector=tmp.u;   
                success=true;
				printf("\nPhysical Drive Capacity (Reported by STORPORT DRIVER) \n");
				printf("TotalSectors (including hidden): %lu \nBytesPerSector %lu\n",Sectors,BytesPerSector);
            }   
        }   
    }   
    if(hDrive)CloseHandle(hDrive);
	if(!success)ShowLastError();
    return success;   
}
//
//This routine will copy the sectors from one physical device directly to a spot on another physical device.
//Opens Disk driver created Raw partition Device object for Reading & Writing sectors.
EXTERN_DLL_EXPORT INT64 FullDriveCloneSynchronous(wchar_t *SourceDisk, wchar_t *TargetDisk)
{
  DWORD dwBytes;
  DWORD SourceDriveBytesPerSector, SourceDriveTotalSectors;
  DWORD dwWritten = 0;
  DWORD dwRead = 0;
  DWORD bytesRet = 0;
  LARGE_INTEGER offset;
  INT64 isectorF=(INT64)0;
  INT64 isectorT=(INT64)0;
  INT64 sectorCnt;
  INT64 bytesLeft2Write;
  int quitLoop=0;
  int loopCnt=0;
  int sectorsWritten;
  INT64 SourceDiskSize, TargetDiskSize;
  DISK_GEOMETRY sourcePdg, targetPdg;

  if( (!SourceDisk) || (!TargetDisk) ){
	  printf("\nFullDriveCloneSynchronous: Invalid Parameters Passed");
	  return -1;
  }

  //Check if Source drive and Target disk drive both are same, if yes Nah!
  if(0 == wcscmp(SourceDisk, TargetDisk)){
	  printf("\nFullDriveCloneSynchronous: Both Source And Target Disk cannot be the Same- FATAL\n");
  }
  //Mostly root drives will have a symbolic linke with 0th index; Need not be always true.
	if(wcschr(TargetDisk, '0')){
	  printf("\nFullDriveCloneSynchronous: Target Disk Drive looks like a Root Drive-Could be FATAL- Exiting\n");
	  return (INT64)-1;
	}

	if(!StrStrI(SourceDisk, L"\\\\.\\PhysicalDrive")){
	  printf("\nFullDriveCloneSynchronous: Source Disk Drive Name is not Valid- Exiting\n");
	  return (INT64)-1;
	}

    if(!StrStrI(TargetDisk, L"\\\\.\\PhysicalDrive")){
	  printf("\nFullDriveCloneSynchronous: Target Disk Drive Name is not Valid- Exiting\n");
	  return (INT64)-1;
	}

  if(!GetDriveGeometry(&sourcePdg, SourceDisk)){
    //Getting Drive Geometry Failed.
	  printf("\nFullDriveCloneSynchronous: Getting Drive Geometry For Source Disk Failed!");
    return (INT64)-1;
  }

  if(!GetDriveGeometry(&targetPdg, TargetDisk)){
    //Getting Drive Geometry Failed.
	  printf("\nFullDriveCloneSynchronous: Getting Drive Geometry For Target Disk Failed!");
    return (INT64)-1;
  }

  SourceDiskSize = sourcePdg.Cylinders.QuadPart * (ULONG)sourcePdg.TracksPerCylinder *
					(ULONG)sourcePdg.SectorsPerTrack * (ULONG)sourcePdg.BytesPerSector;

  TargetDiskSize = targetPdg.Cylinders.QuadPart * (ULONG)targetPdg.TracksPerCylinder *
					(ULONG)targetPdg.SectorsPerTrack * (ULONG)targetPdg.BytesPerSector;

  if(TargetDiskSize < SourceDiskSize){
	  printf("\nFullDriveCloneSynchronous: Target Drive Capacity Is Lesser Than Source Drive. Failed Copy!");
	  return (INT64)-1;
  }

  //We will get the Total Sectors and Bytes per sector directly from storage port driver and
  //will not rely on Disk for this, since disk hides some sectors purposefully.
  if(!GetSCISIDiskCapacityByName(SourceDisk,SourceDriveTotalSectors,SourceDriveBytesPerSector)){
	  printf("\nFullDriveCloneSynchronous: Fetching Source Drive Capacity Reported By PORT driver resulted in ERROR. Failed Copy!");
	  return (INT64)-1;
  }

  sectorCnt = (INT64)SourceDriveTotalSectors;
  dwBytes = SourceDriveBytesPerSector*2048;	//Read 2048 sectors at a time (if 512 bps then 1 MB)
  bytesLeft2Write=sectorCnt*SourceDriveBytesPerSector;

  BYTE *pTemp = new BYTE[dwBytes];	//Allocate 2048 sectors of memory to store read buffer

	HANDLE hF = CreateFileW(SourceDisk, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if(INVALID_HANDLE_VALUE == hF) {printf("\nERROR - Unable to open device %s for read\n"); return(-1);}

	HANDLE hT = CreateFileW(TargetDisk, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if(INVALID_HANDLE_VALUE == hT) {printf("\nERROR - Unable to open device %s for write\n"); return(-2);}

	if(!DeviceIoControl(hT, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, &bytesRet, NULL) ){
		printf("\nThe Media Seems to be Write Protected! Error!");
		ShowLastError();
	}

	printf("\nFullDriveCloneSynchronous: Copying Sectors %d through %I64d", loopCnt, bytesLeft2Write-1);
	wprintf(L"\nFullDriveCloneSynchronous: Source Disk: %ws   TargetDisk: %ws\n\n",SourceDisk, TargetDisk);



		// Read/Write loop
		while(bytesLeft2Write>0 && quitLoop==0)
		{
			// Truncate buffer to match last (undersized) chunk to copy
			if(bytesLeft2Write < (INT64)dwBytes) dwBytes=(int)bytesLeft2Write;

			// Read Sectors to Memory Buffer
			offset.QuadPart = (isectorF * (INT64)SourceDriveBytesPerSector);
			if(0 == SetFilePointerEx(hF, offset, NULL, FILE_BEGIN) && NO_ERROR != GetLastError()) 
				{
					wprintf(L"Error setting file pointer to sector %I64d GLE:%d\n", isectorF, GetLastError());
					quitLoop=1;
			    }
			if(0 == ReadFile(hF, pTemp, dwBytes, &dwRead, NULL)) 
				{
					wprintf(L"Error reading sector %I64d GLE:%d\n", isectorF, GetLastError());
					quitLoop=1;
			    }

			// Write memory buffer to destination sectors
			offset.QuadPart = (isectorT * (INT64)SourceDriveBytesPerSector);
			if(0 == SetFilePointerEx(hT, offset, NULL, FILE_BEGIN) && NO_ERROR != GetLastError()) 
				{
					wprintf(L"WRITE:Error setting file pointer to sector %I64d\n", isectorT); ShowLastError();
					quitLoop=1;
			    }
			if(0 == WriteFile(hT, pTemp, dwBytes, &dwWritten, NULL)) 
				{
					wprintf(L"WRITE:Error writing sector %I64d \n", isectorT); ShowLastError();
					quitLoop=1;
			    }

			bytesLeft2Write -= dwWritten;
			sectorsWritten=dwWritten/SourceDriveBytesPerSector;
			isectorF += (INT64)sectorsWritten;
			isectorT += (INT64)sectorsWritten;
			// Status dots to screen for every x MB
			++loopCnt;
			if(loopCnt%100==0) printf("."); // print a . for every 100MB

	}
	printf("\nClone Finished.\n\n");
	CloseHandle(hT);
	CloseHandle(hF);

	return((sectorCnt*SourceDriveBytesPerSector)-bytesLeft2Write);

}
// RDiskSectorsWriteFile()
//Read sectors from physical device to file
//if ignoreErrors == 0 then on encountering bad sector error quit
EXTERN_DLL_EXPORT INT64 ReadSectorsFromDiskDriveAndWriteToFile \
(wchar_t *SourceDiskDrive, INT64 SourceDiskStartReadSector, INT64 NumberOfSectorsToReadAndWrite,\
 wchar_t *TargetFileName, int ignoreErrors)
{
  INT64 sectorsRead=0;
  int clipIt=0;
  DWORD SourceDriveTotalSectors, SourceDriveBytesPerSector;
  INT64 dwBytes = 0;
  BYTE *pTemp;
  DWORD dwRead = 0;
  LARGE_INTEGER offset;
  HANDLE fout;
  INT64 isector=SourceDiskStartReadSector;
  int quitLoop=0;
  INT64 sectorsLeft, TotalSectorsWritten = 0;
  int loopCnt=0;
  DWORD bytesWritten;

  printf("Enter - ReadSectorsFromDiskDriveAndWriteToFile\n ");

  printf("Disk name %S  ", SourceDiskDrive);
  printf("TargetFileName %S", TargetFileName);

  if( (!SourceDiskDrive) || (!TargetFileName) ){
	  printf("\nReadSectorsFromDiskDriveAndWriteToFile: Invalid Parameters Passed");
	  return (INT64)-1;
  }

  	if(!StrStrI(SourceDiskDrive, L"\\\\.\\PhysicalDrive")){
	  printf("\nReadSectorsFromDiskDriveAndWriteToFile: Source Disk Drive Name is not Valid- Exiting\n");
	  return (INT64)-1;
	}
	

   //We will get the Total Sectors and Bytes per sector directly from storage port driver and
  //will not rely on Disk for this, since disk hides some sectors purposefully.
  if(!GetSCISIDiskCapacityByName(SourceDiskDrive,SourceDriveTotalSectors,SourceDriveBytesPerSector)){
	  printf("\nReadSectorsFromDiskDriveAndWriteToFile: Fetching Source Drive Capacity Reported By PORT driver resulted in ERROR. Failed Copy!");
	  return (INT64)-1;
  }
  dwBytes = SourceDriveBytesPerSector*2048;	//Read 2048 sectors at a time (if 512 bps then 1 MB)
  pTemp = new BYTE[(int)dwBytes];	//Allocate 2048 sectors of memory to store read buffer

  if (!pTemp)
	  return -1;

	wprintf(L"Reading sectors %I64d thru %I64d from device %ws to file %ws\n",SourceDiskStartReadSector,SourceDiskStartReadSector+NumberOfSectorsToReadAndWrite-1,SourceDiskDrive,TargetFileName);

	if(ignoreErrors!=0) printf("ReadSectorsFromDiskDriveAndWriteToFile: Ignoring errors\n");

	HANDLE h = CreateFileW(SourceDiskDrive, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if(INVALID_HANDLE_VALUE != h)
	{
		//Write to file
		fout=CreateFileW(TargetFileName,FILE_ALL_ACCESS,FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if(fout!=INVALID_HANDLE_VALUE)
		{
			//Read disk and save to file. (2048 sectors at a time)
			while(isector<(SourceDiskStartReadSector+NumberOfSectorsToReadAndWrite) && quitLoop==0)
			{
				//Clip if only need to read less than 2048 sectors (dwBytes)
				sectorsLeft = NumberOfSectorsToReadAndWrite-(isector-SourceDiskStartReadSector);
				if((sectorsLeft*SourceDriveBytesPerSector) < dwBytes) dwBytes=sectorsLeft*SourceDriveBytesPerSector;

				offset.QuadPart = (isector * SourceDriveBytesPerSector);
				if(0 == SetFilePointerEx(h, offset, NULL, FILE_BEGIN) && NO_ERROR != GetLastError()) 
				{
					wprintf(L"Error setting file pointer to sector %I64d GLE:%d\n", isector, GetLastError());
					quitLoop=1;
				}
				if(0==ReadFile(h, pTemp, (int)dwBytes, &dwRead, NULL)) 
				{
					wprintf(L"Error reading sector %I64d GLE:%d\n", isector, GetLastError());
					if(ignoreErrors==0) quitLoop=1;
					else dwRead=(int)dwBytes; //Force to continue
				}

				if (0 == WriteFile(fout, pTemp, dwRead, &bytesWritten, 0)) {
					printf(" Err : Write to target VHD file\n");
				}

				sectorsRead=(INT64)dwRead/SourceDriveBytesPerSector;
				isector += (INT64)sectorsRead;
				TotalSectorsWritten += sectorsRead;
				//Status dots to screen for every x MB
				++loopCnt;
				if (loopCnt % 100 == 0) {
					printf("."); //print a . for every 100MB					
				}
			}

			CloseHandle(fout);
		}
		else
			wprintf(L"\n\nERROR - Unable to open file %ws\n",TargetFileName);
	}
	else
		wprintf(L"Unable to open %ws (GLE:%d)\n", SourceDiskDrive, GetLastError());

	if(INVALID_HANDLE_VALUE != h)   CloseHandle(h);
	if(pTemp)delete pTemp;
	
	printf("\nExit - ReadSectorsFromDiskDriveAndWriteToFile\n ");
	return(TotalSectorsWritten);
}
//
//Read Sectors from file and write directly to a physical device
//Return: Number of sectors written. Bad sectors ignored.
EXTERN_DLL_EXPORT INT64 ReadSectorsFromFileAndWriteToDiskDrive \
(wchar_t *SourceFileName, wchar_t *TargetDiskDrive, INT64 SourceFileStartReadSector, INT64 NumberOfSectorsToWrite)
{
  INT64 dwBytes;		//Read 2048 sectors at a time (if 512 bps then 1 MB)
  DWORD dwWritten = 0;
  DWORD TargetDriveTotalSectors, TargetDriveBytesPerSector;
  BYTE *pTemp;		//Allocate 2048 sectors of memory to store read buffer
  HANDLE fin;
  LARGE_INTEGER offset;
  INT64 isector=SourceFileStartReadSector;
  DWORD lastFileReadCnt=0;
  INT64 sectorsWritten=0;
  int quitLoop=0;
  int loopCnt=0;
  int fileCnt=1;
  DWORD bytesWrittenToFile=0;

    if( (!SourceFileName) || (!TargetDiskDrive) ){
	  printf("\nReadSectorsFromFileAndWriteToDiskDrive: Invalid Parameters Passed");
	  return (INT64)-1;
    }

	if(!StrStrI(TargetDiskDrive, L"\\\\.\\PhysicalDrive")){
	  printf("\nReadSectorsFromFileAndWriteToDiskDrive: Target Disk Drive Name is not Valid- Exiting\n");
	  return (INT64)-1;
	}

   //We will get the Total Sectors and Bytes per sector directly from storage port driver and
   //will not rely on Disk for this, since disk hides some sectors purposefully.
   if(!GetSCISIDiskCapacityByName(TargetDiskDrive,TargetDriveTotalSectors,TargetDriveBytesPerSector)){
	  printf("\nReadSectorsFromFileAndWriteToDiskDrive: Fetching Target Drive Capacity Reported By PORT driver resulted in ERROR. Failed Copy!");
	  return (INT64)-1;
   }
   
   if(NumberOfSectorsToWrite > TargetDriveTotalSectors){
	  printf("\nReadSectorsFromFileAndWriteToDiskDrive: Sectors to be written cannot be GT Target Drive capacity!-FATAL");
	  return (INT64)-1;
  }

  dwBytes = TargetDriveBytesPerSector*2048;	//Read 2048 sectors at a time (if 512 bps then 1 MB)
  pTemp = new BYTE[(int)dwBytes];	//Allocate 2048 sectors of memory to store read buffer
  
  if(!pTemp)return -1;

	fin=CreateFileW(SourceFileName,FILE_ALL_ACCESS,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(fin==INVALID_HANDLE_VALUE) {
	wprintf(L"\n\nERROR - Unable to open file %ws\n",SourceFileName);
	return(-1);
	}

	wprintf(L"Writing sectors %I64d thru %I64d\n        to device %ws from file %ws\n\n",\
	SourceFileStartReadSector,SourceFileStartReadSector+NumberOfSectorsToWrite-1,TargetDiskDrive,SourceFileName);

	HANDLE h = CreateFileW(TargetDiskDrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if(INVALID_HANDLE_VALUE != h)
	{
		ReadFile(fin,pTemp,(DWORD)dwBytes,&lastFileReadCnt,NULL);
		while( (isector < (SourceFileStartReadSector+NumberOfSectorsToWrite)) && lastFileReadCnt>0 && quitLoop==0)
		{
			if(lastFileReadCnt<(DWORD)dwBytes) dwBytes=lastFileReadCnt; //Last piece truncated to actual size

			offset.QuadPart = (isector * TargetDriveBytesPerSector);
			if(0 == SetFilePointerEx(h, offset, NULL, FILE_BEGIN) && NO_ERROR != GetLastError()) 
			{
				wprintf(L"WRITE:Error setting file pointer to sector %I64d\n", isector);
				ShowLastError();
				quitLoop=1;
			}
			if(0==WriteFile(h, pTemp, (int)dwBytes, &dwWritten, NULL)) 
			{
				wprintf(L"WRITE:Error writing sector %I64d \n", isector);
				ShowLastError();
				quitLoop=1;
			}
			sectorsWritten=(INT64)dwWritten/TargetDriveBytesPerSector;
			isector += sectorsWritten;

			//Status dots to screen for every x MB
			++loopCnt;
			if(loopCnt%100==0) printf("."); //print a . for every 100MB

			ReadFile(fin,pTemp,(int)dwBytes,&lastFileReadCnt,NULL);
		}
		//Last write of remaining sectors
	}

	CloseHandle(fin);
	if(INVALID_HANDLE_VALUE != h)   CloseHandle(h);
	if(pTemp)delete pTemp;

	return(NumberOfSectorsToWrite);

}

typedef struct _DISK_OFFSET
{
	INT64 Offset; //in bytes
	INT64 Length;
}DISK_OFFSET, *PDISK_OFFSET;
//Read an array of disk offset values (passed as argument with length) to a file
//at same offset; File path should be absolute.
//where DISK_OFFSET_ARRAY_SIZE is the size of the PDISK_OFFSET array, second parameter.
EXTERN_DLL_EXPORT INT64 ReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous\
(wchar_t *SourceDisk, wchar_t *TargetFile, PDISK_OFFSET *DO, unsigned int DISK_OFFSET_ARRAY_SIZE )
{
  LARGE_INTEGER SourceOffset, TargetOffset;
  DWORD SourceDriveTotalSectors,SourceDriveBytesPerSector;
  INT64 SourceDiskSize, TotalBytesWritten = 0;
  unsigned int NumOffsets = DISK_OFFSET_ARRAY_SIZE;
  LARGE_INTEGER TargetFileSize;
  WIN32_FILE_ATTRIBUTE_DATA Fa;

  printf("\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: ReadPhysicalDiskDriveOffsetAndWriteToFileSynchronous ++\n");

  if( (!SourceDisk) || (!TargetFile) || (!DISK_OFFSET_ARRAY_SIZE) ||(!DO)){
	  printf("\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Invalid Parameters");
	  return TotalBytesWritten;
  }

  if(!StrStrI(SourceDisk, L"\\\\.\\PhysicalDrive")){
	  printf("\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Source Disk Drive Name is not Valid- Exiting\n");
	  return TotalBytesWritten;
	}

  //We will get the Total Sectors and Bytes per sector directly from storage port driver and
   //will not rely on Disk for this, since disk hides some sectors purposefully.
   if(!GetSCISIDiskCapacityByName(SourceDisk,SourceDriveTotalSectors,SourceDriveBytesPerSector)){
	  printf("\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Fetching Target Source Drive Capacity Reported By PORT driver resulted in ERROR. Failed Copy!");
	  return TotalBytesWritten;
   }

   if(!GetFileAttributesEx(TargetFile,GetFileExInfoStandard,&Fa) ){
	   wprintf(L"\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Error Getting Attributes for file %ws",TargetFile);
		return TotalBytesWritten;
   }

   SourceDiskSize = SourceDriveTotalSectors * SourceDriveBytesPerSector;
   TargetFileSize.HighPart = Fa.nFileSizeHigh;
   TargetFileSize.LowPart = Fa.nFileSizeLow;

   if(SourceDiskSize > TargetFileSize.QuadPart){
	   printf("\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Source disk size %I64d and Target file size %I64d MISMATCH",SourceDiskSize,TargetFileSize.QuadPart);
	   return TotalBytesWritten;
   }

   for(unsigned int j = 0; j < NumOffsets; j++){
	   if( (DO[j]->Offset + DO[j]->Length) > SourceDiskSize ){
		   wprintf(L"ReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: FATAL ERROR: Offset %I6d with Length %I64d at Index %lu GT Size of Source Disk %ws",\
			   DO[j]->Offset, DO[j]->Length, j, SourceDisk);
		   return TotalBytesWritten;
	   }

	   }
   

	// Open the handle on source disk."\\\\.\\PhysicalDriveX
    HANDLE hFileSource = CreateFile ((LPCTSTR)SourceDisk,
            GENERIC_READ,           // Open for reading
            FILE_SHARE_READ,        // Do not share
            NULL,                   // No security
            OPEN_EXISTING,          // Existing file only
            FILE_FLAG_NO_BUFFERING,  // Normal file
            NULL);                  // No template file
    if (INVALID_HANDLE_VALUE == hFileSource) {
		wprintf(L"\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Error opening source disk %ws",SourceDisk);
		return TotalBytesWritten;
    }

    // Open the handle on dest volume
    HANDLE hFileDest = CreateFile ((LPCTSTR)TargetFile,
            GENERIC_READ | GENERIC_WRITE,           // Open for reading
            FILE_SHARE_READ | FILE_SHARE_WRITE,        // Do not share
            NULL,                   // No security
            OPEN_EXISTING,          // Existing file only
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,  // Normal file
            NULL);                  // No template file
    if (INVALID_HANDLE_VALUE == hFileDest) {
		wprintf(L"\nReadPhysicalDiskDriveOffsetAndWriteToDiskImageFileSynchronous: Error opening target file %ws",TargetFile );
		return TotalBytesWritten;
    }

	INT64 dwBytes = SourceDriveBytesPerSector * 2048; //1MB if bytes persector is 512.
	DWORD lastFileReadCnt = 0;
	DWORD lastFileWriteCnt = 0;
	DWORD bytesWrittenToFile = 0;
	INT64 reminder = 0;
	INT64 innerLoops = 0;
	INT64 TotalBytesWrittenPerOffset = 0;
	BYTE *pTemp = new BYTE[(int)dwBytes];

 if(!pTemp)return 0;

 for (unsigned int i = 0; i < NumOffsets; i++){
  //Read And Write starts here.
   TotalBytesWrittenPerOffset = 0;

	  SourceOffset.QuadPart = DO[i]->Offset;
        SourceOffset.LowPart = SetFilePointer(
              hFileSource,
              SourceOffset.LowPart,
              &SourceOffset.HighPart,
              FILE_BEGIN);
        if (INVALID_SET_FILE_POINTER == SourceOffset.LowPart) {
            DWORD dwErrorVal = GetLastError();
            if (NO_ERROR != dwErrorVal) {
                 printf("Error in SetFilePointer Source  0x%x\n",dwErrorVal);
                return TotalBytesWritten;
            }
        }

	 TargetOffset.QuadPart = DO[i]->Offset;
        TargetOffset.LowPart = SetFilePointer(
              hFileDest,
              TargetOffset.LowPart,
              &TargetOffset.HighPart,
              FILE_BEGIN);
        if (INVALID_SET_FILE_POINTER == TargetOffset.LowPart) {
            DWORD dwErrorVal = GetLastError();
            if (NO_ERROR != dwErrorVal) {
                 printf("Error in SetFilePointer TargetOffset  0x%x\n",dwErrorVal);
                return TotalBytesWritten;
            }
        }

		if(DO[i]->Length > dwBytes){
			reminder = DO[i]->Length % dwBytes;
			innerLoops = DO[i]->Length / dwBytes; 
		}
		else if(DO[i]->Length > 0){
			reminder = 0;
			innerLoops = 1;
		}
		else{
			reminder = 0;
			innerLoops = 0;
		}

		for(int j = 0; j < innerLoops; j++){

			if(!reminder && 1 == innerLoops){
             ReadFile(hFileSource, pTemp, (DWORD)DO[i]->Length, &lastFileReadCnt, NULL);
			}
			else{
		     ReadFile(hFileSource, pTemp, (DWORD)dwBytes, &lastFileReadCnt, NULL);
			}

		 if(lastFileReadCnt <= 0){
			 printf("\nRead Error - FATAL");
			 goto pack;
		 }

		 WriteFile(hFileDest, pTemp, (DWORD)lastFileReadCnt, &lastFileWriteCnt, NULL);

		 if(lastFileReadCnt != lastFileWriteCnt){
			 printf("\nWrite Error - FATAL");
			 goto pack;
		 }

		 if((lastFileReadCnt != dwBytes) && (innerLoops > 1)){
			 //Should not happend normally, if so we should retry.
			 printf("\nRequest size %I64d not Read",dwBytes);
		 }
          TotalBytesWrittenPerOffset += lastFileWriteCnt;
		}//for(int j = 0;

		if(reminder){
          ReadFile(hFileSource, pTemp, (DWORD)reminder, &lastFileReadCnt, NULL);
		  if(lastFileReadCnt <= 0){
			 printf("\nRead Error - FATAL");
			 goto pack;
		 }
		  WriteFile(hFileDest, pTemp, (DWORD)lastFileReadCnt, &lastFileWriteCnt, NULL);

		 if(lastFileReadCnt != lastFileWriteCnt){
			 printf("\nWrite Error - FATAL");
			 goto pack;
		 }
         TotalBytesWrittenPerOffset += lastFileWriteCnt; 
		}//if reminder.

		if(TotalBytesWrittenPerOffset != DO[i]->Length){
			//Inform about the error- Could be fatal.
			printf("\nFor index %lu and offset %I64d total writes mismatch",i,TargetOffset.QuadPart);
		}

  TotalBytesWritten += TotalBytesWrittenPerOffset;	  
}//end of for(int i = 0;
   
pack:
    if(hFileSource)CloseHandle(hFileSource);
	if(hFileDest)CloseHandle(hFileDest);
	if(pTemp)delete pTemp;

	printf("\nExit: ReadPhysicalDiskDriveOffsetAndWriteToFileSynchronous --\n");
	return TotalBytesWritten;
}