#include "Common.h"
#include "setpermissions.h"
#include "securityutils.h"
#include "genpassphrase.h"
using namespace std;

string Common::ConvertHexToDec(const string& strHexVal)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	unsigned int nDecVal;
	string strDecVal;
	stringstream stream, stream1;

	stream << std::hex << strHexVal;
	stream >> nDecVal;

	stream1 << nDecVal;
	strDecVal = stream1.str();

	DebugPrintf(SV_LOG_DEBUG,"Hex Value : %s <==> Decimal Value : %s\n", strHexVal.c_str(), strDecVal.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strDecVal;
}

int Common::getAgentInstallPath()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	char lszValue[256];
    LONG lRet, lEnumRet;
    HKEY hKey;
    DWORD dwType=REG_SZ;
	DWORD dwSize=255;
    //fisrt Lets try to open VX Agent's keys for 32 bit machine
    lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE,"SOFTWARE\\SV Systems\\VxAgent", 0L, KEY_READ , &hKey);
    if(lRet != ERROR_SUCCESS)
    {
		// Lets try to open Fx Agent's keys for 64 bit machine,If even this failed ,We'll report Error
		lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE,"SOFTWARE\\Wow6432Node\\SV Systems\\VxAgent", 0L, KEY_READ , &hKey);
		if(lRet != ERROR_SUCCESS)
		{
			m_PlaceHolder.Properties[REGISTRY_KEY] = "SOFTWARE\\Wow6432Node\\SV Systems\\VxAgent";
			DebugPrintf(SV_LOG_ERROR,"Failed to open VX Agent's registry keys\n");
			RegCloseKey(hKey);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return 1;
		}
	}
    lEnumRet = RegQueryValueEx(hKey, "InstallDirectory", NULL, &dwType,(LPBYTE)&lszValue, &dwSize);
    if(lEnumRet == ERROR_SUCCESS)
    {
        m_strInstallDirectoryPath = lszValue ;
        m_strDataFolderPath = lszValue + string("\\Failover") + string("\\data\\");
    }
	else
	{
		m_PlaceHolder.Properties[REGISTRY_KEY] = "SOFTWARE\\Wow6432Node\\SV Systems\\VxAgent\\InstallDirectory";
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the InstallDirectory value for VX agent\n");
        RegCloseKey(hKey);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
    }
	RegCloseKey(hKey);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return 0;
}
HRESULT Common::WMIQuery()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	string strPhysicalDiskName;
	string strBusId;
	string strPortId;
	string strTargetId;
	string strLunId;
	string strSCSIID;
	unsigned int uPortId = 0;
	unsigned int uTargetId = 0;
    unsigned int uLunId = 0;
	unsigned int uBusId = 0;
	

	BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"Select * FROM Win32_DiskDrive where SIZE>0");
    // Create querying object
    IWbemClassObject *pObj = NULL;
    IWbemServices *pServ = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hRet;

    if(FAILED(GetWbemService(&pServ))) 
	{
	    DebugPrintf(SV_LOG_ERROR,"Failed to initialise GetWbemService.\n");
		CoUninitialize();
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE; 
	}
    // Issue the query.
    hRet = pServ->ExecQuery(
			language,
			query,
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

    if (FAILED(hRet))
    {
        DebugPrintf(SV_LOG_ERROR,"Query for Disk's Device ID failed.Error Code %0X\n",hex);
        pServ->Release();
        CoUninitialize();
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;                // Program has failed.
    }
    SysFreeString(language);
    SysFreeString(query);
        
    // Get the disk drive information
    ULONG uTotal = 0;

    DebugPrintf(SV_LOG_INFO,"\nThe disks present in the host are - \n");

    // Retrieve the objects in the result set.
    while (pEnum)
    {
        ULONG uReturned = 0;
        hRet = pEnum->Next
				(
					WBEM_INFINITE,       // Time out
					1,                  // One object
					&pObj,
					&uReturned
				);
		if (uReturned == 0)
			break;
		VARIANT val;
		uTotal += uReturned;
		
		hRet = pObj->Get(L"DeviceID", 0, &val, 0, 0); // Get the value of the Name property
		strPhysicalDiskName = string(W2A(val.bstrVal));
		VariantClear(&val);

		hRet = pObj->Get(L"SCSIPort", 0, &val, 0, 0);        
		uPortId = val.uintVal;
        VariantClear(&val);
        strPortId   = boost::lexical_cast<std::string>(uPortId);

        hRet = pObj->Get(L"SCSITargetID", 0, &val, 0, 0);            
	    uTargetId = val.uintVal;
	    VariantClear(&val);
	    strTargetId = boost::lexical_cast<std::string>(uTargetId);

		hRet = pObj->Get(L"SCSILogicalUnit", 0, &val, 0, 0);
        uLunId = val.uintVal;
	    VariantClear(&val);
        strLunId = boost::lexical_cast<std::string>(uLunId);

        if(true == bP2Vmachine) //In case of P2V fetch LUN ID too
        {
		    strSCSIID   = strPortId + string (":") + strTargetId + string (":") + strLunId;
            DebugPrintf(SV_LOG_DEBUG,"Device ID : %s    SCSI(PortID:TargetID:LunID) : %s\n",strPhysicalDiskName.c_str(),strSCSIID.c_str());
        }
		else if(true == m_bV2P) //In case of V2P fetch all the values
        {
			hRet = pObj->Get(L"SCSIBus", 0, &val, 0, 0);        
			uBusId = val.uintVal;
			VariantClear(&val);
			strBusId = boost::lexical_cast<std::string>(uBusId);
		    strSCSIID   = strPortId + string (":") + strTargetId + string (":") + strLunId + string (":") + strBusId;
			DebugPrintf(SV_LOG_DEBUG,"Device ID : %s    SCSI(PortID:TargetID:LunID:BusId) : %s\n",strPhysicalDiskName.c_str(),strSCSIID.c_str());
        }
        else 
        {
		    strSCSIID   = strPortId  + string (":") + strTargetId;
			DebugPrintf(SV_LOG_DEBUG, "Device ID : %s    SCSI(PortID:TargetID:LunId) : %s:%s\n", strPhysicalDiskName.c_str(), strSCSIID.c_str(), strLunId.c_str());
        }

		//Insert the SCSI ID to Diskname into the Map 
		m_mapOfDeviceIdAndControllerIds.insert(make_pair(strSCSIID,strPhysicalDiskName));
		if(m_bCloudMT)
		{
			if(stricmp(m_strCloudEnv.c_str(),"azure") == 0)
			{
				if(3 == uPortId)//Azure data-disks always uses 3 as SCSI port
					m_mapOfLunIdAndDiskName.insert(make_pair(strLunId,strPhysicalDiskName));
				else
					DebugPrintf(SV_LOG_DEBUG,"Skipping %s as its SCSIPort is %d\n",strPhysicalDiskName.c_str(),uPortId);
			}
			else if(stricmp(m_strCloudEnv.c_str(),"aws") == 0)
			{
				if(2 == uPortId) //AWS disks always uses 2 as SCSI Port
					m_mapOfLunIdAndDiskName.insert(make_pair(strTargetId,strPhysicalDiskName));
				else
					DebugPrintf(SV_LOG_DEBUG, "Skipping %s as its SCSIPort is %d\n", strPhysicalDiskName.c_str(), uPortId);
			}
			else
			{
				DebugPrintf(SV_LOG_WARNING," Unsupported cloud environment : %s\n",m_strCloudEnv.c_str());
			}
		}

		VariantClear(&val);        	
		pObj->Release();    // Release objects not owned.            
    }

    // All done. Free the Resources
    pEnum->Release();
    pServ->Release();
	CoUninitialize();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;      
}
HRESULT Common::InitCOM()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED); 
	if (FAILED(hres))
    {
	   DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM library. Error Code %0X\n",hex);
       return S_FALSE;                  // Program has failed.
    }
     hres = CoInitializeSecurity(
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

	// Check if the securiy has already been initialized by someone in the same process.
	if (hres == RPC_E_TOO_LATE)
	{
		DebugPrintf(SV_LOG_DEBUG, "Security library initialization failed with error code %0X. Ignoring the error and proceeding with disk discovery.\n", hres);
		hres = S_OK;
	}

    if (FAILED(hres))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to initialize security  library. Error Code %0X\n",hres);
        CoUninitialize();
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;                    // Program has failed.
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;
}

HRESULT Common::GetWbemService(IWbemServices **pWbemService)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    IWbemServices *pWbemServ = NULL;
    IWbemLocator *pLoc = 0;
    HRESULT hr;

    hr = CoCreateInstance(CLSID_WbemLocator, 0,CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc);

    if (FAILED(hr))
    {
	    DebugPrintf(SV_LOG_ERROR,"Failed to create IWbemLocator object. Error Code %0X\n",hr);
        CoUninitialize();
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return hr;     // Program has failed.
    }

    // Connect to the root\default namespace with the current user.
    hr = pLoc->ConnectServer(
            BSTR(L"ROOT\\cimv2"), 
            NULL, NULL, 0, NULL, 0, 0, pWbemService);

    if (FAILED(hr))
    {
		DebugPrintf(SV_LOG_ERROR,"Could not connect to server. Error Code %0X\n",hr);
        pLoc->Release();
        CoUninitialize();
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return hr;      // Program has failed.
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;    
}

string Common::stGetHostName()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
	string strHostName = "";
    //Find the current host 
	if(gethostname(m_currentHost, sizeof(m_currentHost )) == SOCKET_ERROR)
	{
		DebugPrintf(SV_LOG_ERROR,"GethostnameFailed Failed.Error Code %0X\n",WSAGetLastError());		
	}
	else
		strHostName = string(m_currentHost);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strHostName;
}

string Common::strConvetIntegetToString(const int iSomeInteger)
{
	string strRetValue;
	stringstream  sStream;

	sStream<<iSomeInteger;
	strRetValue = sStream.str();  
	sStream.str("");
	sStream.clear();
	
	return strRetValue;
}
BOOL Common::ExecuteProcess(const std::string &FullPathToExe, const std::string &Parameters)
{
	string strFullCommand = FullPathToExe + Parameters;
	//cout<<"\n exported command " <<strFullCommand<<endl;
    STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	memset(&StartupInfo, 0x00, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	memset(&ProcessInformation, 0x00, sizeof(PROCESS_INFORMATION));
	if(!::CreateProcess(NULL,const_cast<char*>(strFullCommand.c_str()),NULL,NULL,FALSE,NULL/*CREATE_DEFAULT_ERROR_MODE|CREATE_SUSPENDED*/,NULL, NULL, &StartupInfo, &ProcessInformation))
	{
        DebugPrintf(SV_LOG_ERROR,"CreateProcess failed with Error [%lu]\n",GetLastError());
		return FALSE;
	}
	else
	{
		//cout<<"Exported MBR file successfully."<<endl;
		//::ResumeThread(ProcessInformation.hThread);
	}
	//Sleep(100000);
	DWORD retValue = WaitForSingleObject( ProcessInformation.hProcess, INFINITE );
	if(retValue == WAIT_FAILED)
	{
		DebugPrintf(SV_LOG_ERROR,"WaitForSingleObject has failed.\n");
		::CloseHandle(ProcessInformation.hProcess);
		::CloseHandle(ProcessInformation.hThread);
		return FALSE;
	}
	::CloseHandle(ProcessInformation.hProcess);
	::CloseHandle(ProcessInformation.hThread);
    return TRUE;
}

int Common::DumpMbrInFile(unsigned int iDiskNumber,LARGE_INTEGER iBytesToSkip,const string& strOutPutMbrFileName,unsigned int noOfBytesToDeal)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HANDLE diskHandle;
	unsigned char *buf;
	unsigned long bytes,bread;
	char driveName [256];

    buf = (unsigned char *)malloc(noOfBytesToDeal * (sizeof(unsigned char)));
    if (buf == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"\n memory allocation failed ");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

	inm_sprintf_s(driveName, ARRAYSIZE(driveName), "\\\\.\\PhysicalDrive%d", iDiskNumber);
	DebugPrintf(SV_LOG_INFO,"Drive name is %s\n",driveName);
	// PR#10815: Long Path support
	diskHandle = SVCreateFile(driveName,GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	if(diskHandle == INVALID_HANDLE_VALUE)
	{
		DebugPrintf(SV_LOG_ERROR,"\n Error in opening device.Error Code: %d\n",GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

    bytes = noOfBytesToDeal;

	// Try to move hFile file pointer some distance  
	DWORD dwPtr = SetFilePointerEx(  diskHandle, 
									iBytesToSkip, 
                                    NULL, 
                                    FILE_BEGIN ); 
	if(0 == dwPtr)
	{
		DebugPrintf(SV_LOG_ERROR,"\n SetFilePointerEx() failed in DumpMbrInFile () with Error code:%d\n",GetLastError());
		CloseHandle(diskHandle);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

    if(0 == ReadFile(diskHandle, buf, bytes, &bread, NULL))
	{
		DebugPrintf(SV_LOG_ERROR,"\n ReadFile failed in DumpMbrInFile () with Error code::%d\n",GetLastError());
		CloseHandle(diskHandle);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

	CloseHandle(diskHandle);

	FILE *fp;
	fp = fopen(strOutPutMbrFileName.c_str(),"wb");
	if(!fp)
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open file : %s",strOutPutMbrFileName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}
    int inumberOfBytesWritten = fwrite (buf , 1 , noOfBytesToDeal , fp );

    if(noOfBytesToDeal != inumberOfBytesWritten)
	{
        DebugPrintf(SV_LOG_ERROR,"\n Failed to write in file : %s",strOutPutMbrFileName.c_str());
        fclose(fp);
        free(buf);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return -1;
	}
    fclose(fp);
    free(buf);

	//Assign secure permission to the file
	AssignSecureFilePermission(strOutPutMbrFileName);

    DebugPrintf(SV_LOG_INFO,"Dumped the MBR for Disk = %u into the file - %s\n",iDiskNumber,strOutPutMbrFileName.c_str());
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return 0;
}

//Check if File exists and return true
bool Common::checkIfFileExists(string strFileName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool blnReturn = true;
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    hFind = FindFirstFile(strFileName.c_str(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) 
    {
        DebugPrintf(SV_LOG_DEBUG,"Failed to find the file : %s\n",strFileName.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Invalid File Handle.GetLastError reports [Error %lu ]\n",GetLastError());
        blnReturn = false;
    } 
    else 
    {
        DebugPrintf(SV_LOG_DEBUG,"Found the file : %s\n",strFileName.c_str());
    }
    FindClose(hFind);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return blnReturn;
}


//Check if Dir exists and return true
bool Common::checkIfDirExists(string strDirName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool blnReturn = false;

	DWORD type = GetFileAttributes(strDirName.c_str());
	if (type != INVALID_FILE_ATTRIBUTES)
	{
		if (type & FILE_ATTRIBUTE_DIRECTORY)
			blnReturn = true;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return blnReturn;
}

//check if a given string is a unsigned integer or not
bool Common::isAnInteger(const char *pcInt)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bReturnValue = true;
    DebugPrintf(SV_LOG_INFO,"Input String - %s\n",pcInt);
    for(unsigned int i=0; i<strlen(pcInt); i++)
    {
        if(!isdigit(pcInt[i]))
        {
            DebugPrintf(SV_LOG_INFO,"%c is not a digit in input string %s\n",pcInt[i],pcInt);
            bReturnValue = false;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bReturnValue;
}

// --------------------------------------------------------------------------
// format volume name the way MS expects for volume mount point APIs. 
// final format should be one of 
//  <drive>:\
//  <drive>:\<mntpoint>\
// where: <drive> is the drive letter and <mntpoint> is the mount point name
// e.g. 
//  A:\ for a drive letter
//  B:\mnt\data\ for a mount point
// --------------------------------------------------------------------------
void Common::FormatVolumeNameForMountPoint(std::string & volumeName)
{   
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    std::string::size_type len = volumeName.length();
   
    if (1 == len) {
        // assume it is a drive letter
        volumeName += ":\\";
    } else {
        // need to make sure it ends with a '\\'
        if ('\\' != volumeName[len - 1]) {
            volumeName += '\\';
        } 
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

// --------------------------------------------------------------------------
// format volume name the way the CX wants it
// final format should be one of
//   <drive>
//   <drive>:\[<mntpoint>]
// where: <drive> is the drive letter and [<mntpoint>] is optional
//        and <mntpoint> is the mount point name
// e.g.
//   A for a drive letter
//   B:\mnt\data for a mount point
// --------------------------------------------------------------------------
void Common::FormatVolumeNameForCxReporting(std::string& volumeName)
{
    // we need to strip off any leading \, ., ? if they exists
    std::string::size_type idx = volumeName.find_first_not_of("\\.?");
    if (std::string::npos != idx) {
        volumeName.erase(0, idx);
    }

    // strip off trailing :\ or : if they exist
    std::string::size_type len = volumeName.length();
	//if we get the len as 0 we are not proceeding and simply returning
	//this is as per the bug 10377
	if(!len)
		return;
    idx = len;
    if ('\\' == volumeName[len - 1] || ':' == volumeName[len - 1]) {
        --idx;
    }

    if ((len > 2) && (':' == volumeName[len - 2]) )
    {
            --idx;
    }

    if (idx < len) {
        volumeName.erase(idx);
    }
}

bool Common::ExecuteCommandUsingAce(const std::string FullPathToExe, const std::string Parameters)
{
    bool rv = false;

    //create an ACE process to run the generated script
	ACE_Process_Options options; 
    options.handle_inheritance(false);

    options.command_line("\"%s\" %s",FullPathToExe.c_str(),Parameters.c_str());
    
    ACE_Process_Manager* pm = ACE_Process_Manager::instance();
	if (pm == NULL) {
		DebugPrintf(SV_LOG_ERROR,"Failed to get process manager instance. Error - [%d].\n\n", ACE_OS::last_error());     
		rv = false;
	}

    pid_t pid = pm->spawn (options);
    if (pid == ACE_INVALID_PID) {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn a process for executing the command. Error - [%d].\n\n", ACE_OS::last_error());          
        rv = false;
    }

	// wait for the process to finish
	ACE_exitcode status = 0;
	pid_t rc = pm->wait(pid, &status);
	
	if (status != 0)
    {
        DebugPrintf(SV_LOG_ERROR,"Command failed with exit code [%d]. Error - [%d]\n\n", status, ACE_OS::last_error());
        rv = false;
	}
	else
    {		
        DebugPrintf(SV_LOG_INFO,"Successfully executed the command.\n\n");
        rv = true;        
	}
    options.release_handles();

    return rv;
}

// This function reads the data from disk directly (like dd)
// Output   - Data read from disk
int Common::ReadDiskData(const char *disk_name, ACE_LOFF_T bytes_to_skip, size_t bytes_to_read, unsigned char *disk_data)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    ACE_HANDLE hdl;    
    int bytes_read = 0;

    hdl = ACE_OS::open(disk_name,O_RDONLY,ACE_DEFAULT_OPEN_PERMS);
    if(ACE_INVALID_HANDLE == hdl)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::open failed: disk %s, err = %d\n ", disk_name, ACE_OS::last_error());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
    }

    if(ACE_OS::lseek(hdl, bytes_to_skip, SEEK_SET) < 0)
    {   
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::lseek failed: disk %s, offset 0, err = %d\n", disk_name, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
    }

    bytes_read = ACE_OS::read(hdl, disk_data, bytes_to_read);
	if(bytes_read != bytes_to_read)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::read failed: disk %s, err = %d\n", disk_name, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
	}   
    ACE_OS::close(hdl);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return 0;
}

// This function writes the data to the file in binary format.
int Common::WriteToFileInBinary(const unsigned char *buffer, size_t bytes_to_write, const char *file_name)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int bytes_write = 0;
    FILE *fp;

	fp = fopen(file_name,"wb");
	if(!fp)
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fopen failed: file %s\n",file_name);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}

    bytes_write = fwrite (buffer, 1, bytes_to_write, fp);
    if(bytes_to_write != bytes_write)
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fwrite failed: file %s\n",file_name);
        fclose(fp);        
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
	}
    fclose(fp);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return 0;
}

// This function returns specified contigous set of bytes from given input mbr in 
// proper format i.e. MBRs will be in little endian format. so consider that too.
// Example - get 4 bytes starting from 474(i.e 474-477) bytes from a string of size 512 bytes.
// MBR is in Little Endian format - so store in reverse order.
void Common::GetBytesFromMBR(unsigned char *buffer, size_t start_index, size_t bytes_to_fetch, unsigned char *out_data)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(size_t i=0; i<bytes_to_fetch; i++)
    {
        out_data[i] = buffer[start_index + bytes_to_fetch - 1 - i]; //ex: 474 + 4 - 1 - 0(i)
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

// Convert the hex array to decimal number
// Format of Hex Array - a[0]=1A , a[1]=2B, a[2]=3C, a[3]=4D
// 1. Merge it and make it a hex number 1A2B3C4D
// 2. Convert to Decimal number (= 439041101)
void Common::ConvertHexArrayToDec(unsigned char *hex_array, size_t n, ACE_LOFF_T & number)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    number = 0;
    for(size_t i=0; i<n; i++)
    {
        number = number<<8;
        number = number | ((hex_array[i] & 0xF0) | (hex_array[i] & 0x0F)); 
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

// This function writes the data from disk directly (like dd)
// Output   - Data read from disk
int Common::WriteDiskData(const char *disk_name, ACE_LOFF_T bytes_to_skip, size_t bytes_to_write, unsigned char *disk_data)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    ACE_HANDLE hdl;    
    int bytes_write = 0;

    hdl = ACE_OS::open(disk_name,O_RDWR,ACE_DEFAULT_OPEN_PERMS);
    if(ACE_INVALID_HANDLE == hdl)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::open failed: disk %s, err = %d\n ", disk_name, ACE_OS::last_error());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
    }

    if(ACE_OS::lseek(hdl, bytes_to_skip, SEEK_SET) < 0)
    {   
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::lseek failed: disk %s, offset 0, err = %d\n", disk_name, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
    }

    bytes_write = ACE_OS::write(hdl, disk_data, bytes_to_write);
	if(bytes_write != bytes_to_write)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::write failed: disk %s, err = %d\n", disk_name, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
	}   
    ACE_OS::close(hdl);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return 0;
}

// This function read the data from the file in binary format.
int Common::ReadFromFileInBinary(unsigned char *out_buffer, size_t bytes_to_read, const char *file_name)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int bytes_read = 0;
    FILE *fp;

	fp = fopen(file_name,"rb");
	if(!fp)
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fopen failed: file %s\n",file_name);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}

    bytes_read = fread(out_buffer, 1, bytes_to_read, fp);
    if(bytes_to_read != bytes_read)
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fread failed: file %s\n",file_name);
        fclose(fp);        
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return 1;
	}
    fclose(fp);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return 0;
}

// This func will generate unique time stamp string
std::string Common::GeneratreUniqueTimeStampString()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strUniqueTimeStampString;
	SYSTEMTIME  lt;
    GetLocalTime(&lt);
	string strTemp;

	strTemp = boost::lexical_cast<string>(lt.wYear);
	strUniqueTimeStampString += strTemp;
	strTemp.clear();
	
    strTemp = boost::lexical_cast<std::string>(lt.wMonth);
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wDay);
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wHour);
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMinute) ;
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wSecond) ;
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMilliseconds);
    if(strTemp.size() == 1)
        strTemp = string("00") + strTemp;
    else if(strTemp.size() == 2)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

    DebugPrintf(SV_LOG_DEBUG,"strUniqueTimeStampString = %s\n",strUniqueTimeStampString.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strUniqueTimeStampString;
}


bool Common::CdpcliOperations(const string& cdpcli_cmd)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool result = true;

	string cmdName = m_strInstallDirectoryPath + "\\" + cdpcli_cmd ;

	DebugPrintf(SV_LOG_DEBUG,"Command to execute =  %s\n",cmdName.c_str());

	ACE_Process_Options options; 
    options.handle_inheritance(false);

	options.command_line ("%s %s", cmdName.c_str(), "");
	ACE_Process_Manager* pm = ACE_Process_Manager::instance();
	if(pm == NULL)
	{
		// need to continue with other volumes even if there was a problem generating an ACE process
		DebugPrintf(SV_LOG_ERROR,"Failed to Get ACE Process Manager instance\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	pid_t pid = pm->spawn (options);
	if(pid == ACE_INVALID_PID)
	{
		// need to continue with other volumes even if there was a problem generating an ACE process
		DebugPrintf(SV_LOG_ERROR,"Failed to create ACE Process for executing command :  %s \n",cmdName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	// wait for the process to finish
	ACE_exitcode status = 0;
	pid_t rc = pm->wait(pid, &status);
	if (status == 0)
	{
		result = true;
	}
	else 
	{
		DebugPrintf(SV_LOG_ERROR,"[Warning] : Encountered an error while executing command : %s\n", cmdName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return result;
}

//This will clean if any stale mount points are exist
//Coomand - mountvol /R
//It will clean the stale mounted directories and registry entries.
bool Common::MountVolCleanup()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	std::string strExeName = "mountvol";
	std::string strParameter = " /R";

	if (FALSE == ExecuteProcess(strExeName, strParameter))
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "Failed to run mountvol command to clean up stale entries.\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}


bool Common::RegisterHostUsingCdpCli()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string CdpcliCmd = string("cdpcli.exe --registerhost");

	DebugPrintf(SV_LOG_DEBUG,"\n");
    DebugPrintf(SV_LOG_DEBUG,"CDPCLI command : %s\n", CdpcliCmd.c_str());

    if(false == CdpcliOperations(CdpcliCmd))
    {
        DebugPrintf(SV_LOG_ERROR,"CDPCLI register host operation got failed\n");
        rv = false;
    }

	ACE_OS::sleep(15);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}



bool Common::ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG, "Command     = %s\n", Command.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Output File = %s\n", OutputFile.c_str());

    ACE_Process_Options options;        
    options.command_line("%s",Command.c_str());

	ACE_Time_Value timeout;
	timeout = 900;
    
	ACE_HANDLE handle = ACE_OS::open(OutputFile.c_str(), Modes, securitylib::defaultFilePermissions(), securitylib::defaultSecurityAttributes());
    if(handle == ACE_INVALID_HANDLE)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create a handle to open the file - %s.\n",OutputFile.c_str());
        rv = false;
    } 
    else
    {
        if(options.set_handles(ACE_STDIN, handle, handle) == -1)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to set handles for STDOUT and STDERR.\n");            
            rv = false; 
        } 
        else
        {        
            ACE_Process_Manager* pm = ACE_Process_Manager::instance();
	        if (pm == NULL)
            {
		        DebugPrintf(SV_LOG_ERROR,"Failed to get process manager instance. Error - [%d - %s].\n", ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error())); 
		        rv = false;
	        } 
            else
            {
                pid_t pid = pm->spawn(options);
                if (pid == ACE_INVALID_PID)
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed to spawn a process for executing the command. Error - [%d - %s].\n", ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error())); 
                    rv = false;
                } 
                else
                {
	                ACE_exitcode status = 0;	                
	                pid_t rc = pm->wait(pid, timeout, &status); // wait for the process to finish
					if(ACE_INVALID_PID == rc)
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to wait for the process. PID: %d.\n", pid);
						DebugPrintf(SV_LOG_ERROR, "Command failed with exit status - %d\n", status);
						rv = false;
					}
                    else if ( 0  == rc)
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to wait for the process. PID: %d. A timeout occurred.\n", pid);
						DebugPrintf(SV_LOG_ERROR, "Command failed to execute, terminating the process %d\n", pid);
						pm->terminate(pid);
						rv = false;
					}
					else if ( rc > 0 )
					{
						DebugPrintf(SV_LOG_DEBUG, "Command succeesfully executed having PID: %d.\n", rc);
						rv = true;
					}
                }
            }
            options.release_handles();
        }
        ACE_OS::close(handle);
    }   

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


/*****************************************************************************************
This function is to download the mbrdata_<timestamp> file from CX.
If successfully downloads then returns true, else false.
1st Input: CX URL with path where the file exist.
2nd input: Local path where the file need to be downloaded.
*****************************************************************************************/
bool Common::DownloadFileFromCx(string strCxPath, string strLocalPath/*, bool bMbr*/)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = false;

	//Increasing the retry to 30 times in each 30 secs gap in case of failure. Found a internal bug on downloading the file so increased the waiting time from 30 secs to 15 mins
	int nRetry = 30;

	do
	{
		if (m_bCloudMT)
		{
			try
			{
				LocalConfigurator config;
				HttpFileTransfer http_ft(config.IsHttps(), getCxIpAddress(), boost::lexical_cast<std::string>(config.getHttp().port));
				if (http_ft.Init() && http_ft.Download(strCxPath, strLocalPath))
				{
					DebugPrintf(SV_LOG_DEBUG, "Download successful.\nRemote-File : %s\nLocal-copy: %s\n", strCxPath.c_str(), strLocalPath.c_str());
					bRetStatus = true;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Download Failure. Remote-File : %s\n", strCxPath.c_str());
				}
			}
			catch (...)
			{
				DebugPrintf(SV_LOG_ERROR, "Cought exception. Download Failure. Remote-File : %s\n", strCxPath.c_str());
			}
		}
		else
		{
			Configurator* TheConfigurator = NULL;
			if (!GetConfigurator(&TheConfigurator))
			{
				DebugPrintf(SV_LOG_ERROR, "Unable to obtain configurator %s %d \n", FUNCTION_NAME, LINE_NO);
				break;
			}
			std::string strCxIp = TheConfigurator->getHttpSettings().ipAddress;

			string CxCmd = m_strInstallDirectoryPath + string("\\cxpsclient.exe -i ") + strCxIp + string(" --cscreds --get \"") + strCxPath + string("\" -L \"") + strLocalPath + string("\" --secure -c \"") + m_strInstallDirectoryPath + string("\\transport\\client.pem\"");

			DebugPrintf(SV_LOG_DEBUG, "\n");
			DebugPrintf(SV_LOG_DEBUG, "CX File Download command : %s\n", CxCmd.c_str());

			string strOutputFile;
			if (m_strDirPath.empty())
				strOutputFile = m_strDataFolderPath + "cxpsclient_output";
			else
				strOutputFile = m_strDirPath + string("\\") + "cxpsclient_output";

			if (false == ExecuteCmdWithOutputToFileWithPermModes(CxCmd, strOutputFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR, "File download command failed\n");
			}

			//Even though cxpsclient fails to download it creates a temporary file. Need validation on the file.
			if (false == checkIfFileExists(strLocalPath))
			{
				DebugPrintf(SV_LOG_ERROR, "File download failed.\n");
			}
			else
			{
				DWORD dFileSize = 0;
				HANDLE hFile = SVCreateFile(strLocalPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile == INVALID_HANDLE_VALUE)
				{
					DebugPrintf(SV_LOG_ERROR, "File handle open failed, File download might have failed.\n");
				}
				else
				{
					dFileSize = GetFileSize(hFile, NULL);
					if (0 == dFileSize)
					{
						DebugPrintf(SV_LOG_ERROR, "File is emtpty, download might have failed.\n");
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG, "Download successful.\nRemote-File : %s\nLocal-copy: %s\n", strCxPath.c_str(), strLocalPath.c_str());
						bRetStatus = true;
					}
				}
			}
		}
		if (!bRetStatus)
		{
			nRetry--;
			DebugPrintf(SV_LOG_WARNING, "Retrying downloading of file %s after waiting for 30 secs.\n", strLocalPath.c_str());
			if(nRetry > 0)
				Sleep(30000);
		}
		else
			break;
	} while (nRetry > 0);

	if (bRetStatus)
		AssignSecureFilePermission(strLocalPath);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


/************************************************************************
This function renames the the input file name to a new filename.
It takes two arguments .
input -- First agument is the old filename
input -- second argument is the new file name.
************************************************************************/
bool Common::RenameDiskInfoFile(string strOldFile, string strNewFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	if(!checkIfFileExists(strOldFile))
	{
		DebugPrintf(SV_LOG_ERROR, "File (%s) does not exist.\n",strOldFile.c_str());
		return false;
	}
	if(false==SVCopyFile(strOldFile.c_str(),strNewFile.c_str(),false))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s to %s, Error: [%lu].\n",strOldFile.c_str(),strNewFile.c_str(),GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	if(-1 == remove(strOldFile.c_str()))
	{
		DebugPrintf(SV_LOG_WARNING, "Failed to delete the file - %s\n",strOldFile.c_str());			
		DebugPrintf(SV_LOG_WARNING, "Delete the file manually \n");
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


//*************************************************************************************************
// Fetches the disk size for given disk name
//*************************************************************************************************
bool Common::GetDiskSize(const char *DiskName, SV_LONGLONG & DiskSize)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool bFlag = true;

    GET_LENGTH_INFORMATION dInfo;
    DWORD dInfoSize = sizeof(GET_LENGTH_INFORMATION);
    DWORD dwValue = 0;
    do
    {
        HANDLE hPhysicalDriveIOCTL;
        hPhysicalDriveIOCTL = CreateFile (DiskName, 
                                          GENERIC_WRITE |GENERIC_READ, 
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                          NULL, OPEN_EXISTING, 
                                          FILE_ATTRIBUTE_NORMAL, NULL);

        if(hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
        {
	        DebugPrintf(SV_LOG_ERROR,"Failed to get the disk handle for disk %s with Error code: %lu\n", DiskName, GetLastError());
	        bFlag = false;
	        break;
        }

        bFlag = DeviceIoControl(hPhysicalDriveIOCTL,
                                IOCTL_DISK_GET_LENGTH_INFO,
                                NULL, 0,	
                                &dInfo, dInfoSize,
                                &dwValue, NULL);
        if (bFlag) 
        {
            DiskSize = dInfo.Length.QuadPart;
	        CloseHandle(hPhysicalDriveIOCTL);
        }
        else 
        {
	        DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_GET_LENGTH_INFO failed for disk %s with Error code: %lu\n", DiskName, GetLastError());
	        CloseHandle(hPhysicalDriveIOCTL);
	        bFlag = false;
	        break;
        }        
        DebugPrintf(SV_LOG_DEBUG, "Disk(%s) : Size = %llu\n", DiskName, DiskSize);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);  
    return bFlag;
}


/************************************************************
Get the vcon Version from Esx.xml file
************************************************************/
bool Common::xGetvConVersion(string FileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(FileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"ConfigFileVersion");
	if (attr_value_temp != NULL)
	{
		string strVConVersion = string((char *)attr_value_temp);
		stringstream temp;
		temp << strVConVersion;
		temp >> m_vConVersion;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml file is not in expected format : <mbrpath> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


bool Common::DisableAutoMount()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	string strDiskPartFile = m_strDataFolderPath + DISABLE_MOUNTPOINT_FILE_PATH;

	DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

	ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
	if(! outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
		return false;
	}

	string strAutoMntDisable = string("automount disable"); 
	outfile << strAutoMntDisable << endl;
	outfile << "Exit";
	outfile.close();

	AssignSecureFilePermission(strDiskPartFile);

	string strExeName = string("diskpart.exe");
	string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

	DebugPrintf(SV_LOG_DEBUG,"Command to Disable automount is : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());
	strExeName = strExeName + strParameterToDiskPartExe;
	string strOurPutFile = m_strDataFolderPath + string("\\automount_output.txt");
	if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to disable the automount\n");
		bRet = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Successfully disabled the automount\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

void Common::AssignSecureFilePermission(const string& strFile, bool bDir)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_DEBUG, "Assigning permission to file : %s\n", strFile.c_str());

	try
	{
		if (bDir)
		{
			if (checkIfDirExists(strFile))
				securitylib::setPermissions(strFile, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
			else
				DebugPrintf(SV_LOG_ERROR, "Dir does not exist : %s\n", strFile.c_str());
		}
		else
		{
			if (checkIfFileExists(strFile))
				securitylib::setPermissions(strFile);
			else
				DebugPrintf(SV_LOG_ERROR, "File does not exist : %s\n", strFile.c_str());
		}
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, "[Exception] Failed to set permission : %s\n", strFile.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

string Common::GetAccessSignature(const string& strRqstMethod, const string& strAccessFile, const string& strFuncName, const string& strRqstId, const string& strVersion)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strPassPhrase =  securitylib::getPassphrase();
	string strPassPhraseHash = securitylib::genSha256Mac(strPassPhrase.c_str(), strPassPhrase.size(), true);  //Gets SHA256 mac of passphrase

	string strFingerPrint = GetFingerPrint();
	string strFingerPrintHmac;
	if(!strFingerPrint.empty())
		strFingerPrintHmac = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strFingerPrint);

	string strStringToSign = strRqstMethod + ":" + strAccessFile + ":" + strFuncName + ":" + strRqstId + ":" + strVersion;
	string strSignHmac = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strStringToSign);

	string strAccessSignature = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strFingerPrintHmac + ":" + strSignHmac);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strAccessSignature;
}

string Common::GetFingerPrint()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	//Read fingerprint from file
	string fingerprintFileName(securitylib::getFingerprintDir());
    fingerprintFileName = fingerprintFileName + "\\" + getCxIpAddress() + "_" + getCxHttpPortNumber() + ".fingerprint";
       
    std::string key;
    std::ifstream file(fingerprintFileName.c_str());
    if (file.good()) {
		file >> key;
        boost::algorithm::trim(key);
    } 

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return key;
}

string Common::GetRequestId()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strRqstId;

	strRqstId = GeneratreUniqueTimeStampString();

	DWORD nRandomNum = rand() % 100000000 + 10000001;
	stringstream ss;
	ss << nRandomNum;
	strRqstId = strRqstId + "_" + ss.str();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strRqstId;
}


//Find CX Ip from Local Configurtaor
string Common::getCxIpAddress()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return GetCxIpAddress();
 
}

//Find CX Ip from the host. Read from registry
string Common::getCxHttpPortNumber()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	LocalConfigurator obj;
	HTTP_CONNECTION_SETTINGS objHttpConSettings = obj.getHttp();
    char cPortNumber[15];
    itoa(objHttpConSettings.port,cPortNumber,10);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return cPortNumber; 
}

//Find Inmage host id
string Common::getInMageHostId()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	LocalConfigurator obj;
	string hostid = obj.getHostId();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hostid;
 
}


CVdsHelper::CVdsHelper(void)
:m_pIVdsService(NULL)
{
}

CVdsHelper::~CVdsHelper(void)
{
	INM_SAFE_RELEASE(m_pIVdsService);
}

bool CVdsHelper::InitVDS()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bInitSuccess = false;
	HRESULT hResult;
	IVdsServiceLoader *pVdsSrvLoader = NULL;

	hResult = CoInitializeEx(NULL,COINIT_MULTITHREADED);
	if(SUCCEEDED(hResult))
	{
		hResult = CoCreateInstance(CLSID_VdsLoader,
			NULL,
			CLSCTX_LOCAL_SERVER,
			IID_IVdsServiceLoader,
			(LPVOID*)&pVdsSrvLoader);

		if(SUCCEEDED(hResult))
		{
			//Release if we have already loaded the VDS Service
			INM_SAFE_RELEASE(m_pIVdsService);
			hResult = pVdsSrvLoader->LoadService(NULL,&m_pIVdsService);
		}

		INM_SAFE_RELEASE(pVdsSrvLoader);

		if(SUCCEEDED(hResult))
		{
			hResult = m_pIVdsService->WaitForServiceReady();
			if(SUCCEEDED(hResult))
			{
				DebugPrintf(SV_LOG_DEBUG, "VDS Initializaton successful\n");
				bInitSuccess = true;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"VDS Service ready failure %ld\n", hResult);
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"VDS Initialization failure %ld\n", hResult);
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"COM initialization failed %ld \n", hResult);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bInitSuccess;
}

HRESULT CVdsHelper::ProcessProviders()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pEnumSwProvider = NULL;
	IUnknown *pIUnkown = NULL;
	IVdsSwProvider *pVdsSwProvider = NULL;
	ULONG ulFetched = 0;

	hr = m_pIVdsService->QueryProviders(
						VDS_QUERY_SOFTWARE_PROVIDERS,
						&pEnumSwProvider);
	if(SUCCEEDED(hr)&&(!pEnumSwProvider))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			hr = pEnumSwProvider->Next(1,&pIUnkown,&ulFetched);
			if(0 == ulFetched)
			{
				break;
			}
			if(SUCCEEDED(hr) && (!pIUnkown))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				//Get software provider interface pointer for pack details.
				hr = pIUnkown->QueryInterface(IID_IVdsSwProvider,
					(void**)&pVdsSwProvider);
			}
			if(SUCCEEDED(hr) && (!pVdsSwProvider))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				ProcessPacks(pVdsSwProvider);
			}
			INM_SAFE_RELEASE(pVdsSwProvider);
			INM_SAFE_RELEASE(pIUnkown);
		}
	}
	INM_SAFE_RELEASE(pEnumSwProvider);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

HRESULT CVdsHelper::ProcessPacks(IVdsSwProvider *pSwProvider)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject  *pEnumPacks = NULL;
	IUnknown		*pIUnkown = NULL;
	IVdsPack		*pPack = NULL;
	ULONG ulFetched = 0;

	hr = pSwProvider->QueryPacks(&pEnumPacks);
	if(SUCCEEDED(hr)&&(!pEnumPacks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			hr = pEnumPacks->Next(1,&pIUnkown,&ulFetched);
			if(0 == ulFetched)
			{
				break;
			}
			if(SUCCEEDED(hr)&&(!pIUnkown))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				hr = pIUnkown->QueryInterface(IID_IVdsPack,
					(void**)&pPack);
			}
			if(SUCCEEDED(hr)&&(!pPack))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				switch(m_objType)
				{
				case INM_OBJ_UNKNOWN:
					CheckUnKnownDisks(pPack);
					break;
				case INM_OBJ_MISSING_DISK:
					RemoveMissingDisks(pPack);
					break;
				case INM_OBJ_DISK_ONLINE:
					CollectOnlineDisks(pPack);
					break;
				case INM_OBJ_FAILED_VOL:
					DeleteFailedVolumes(pPack);
					break;
				case INM_OBJ_DISK_SCSI_ID:
					CollectDiskScsiIDs(pPack);
					break;
				case INM_OBJ_DISK_SIGNATURE:
					CollectDiskSignatures(pPack);
					break;
				case INM_OBJ_HIDDEN_VOL:
					ClearFlagsOfVolume(pPack);
					break;
				default:
					DebugPrintf(SV_LOG_ERROR,"Error: Object type not set for vds helper.\n");
					hr = E_UNEXPECTED;
					INM_SAFE_RELEASE(pPack);
					INM_SAFE_RELEASE(pIUnkown);
					INM_SAFE_RELEASE(pEnumPacks);
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return hr;
				}
			}
			INM_SAFE_RELEASE(pPack);
			INM_SAFE_RELEASE(pIUnkown);
		}
	}
	INM_SAFE_RELEASE(pEnumPacks);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::ClearFlagsOfVolume(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr = S_OK;
USES_CONVERSION;
	IEnumVdsObject *pIEnumVoluems = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsVolume *pIVolume = NULL;
	ULONG ulFetched = 0;

	std::list<VDS_OBJECT_ID> lstFailedVols;

	hr = pPack->QueryVolumes(&pIEnumVoluems);
	if(SUCCEEDED(hr) && (!pIEnumVoluems))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumVoluems->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched || hr == S_FALSE)
				break;
			if(SUCCEEDED(hr) && (!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsVolume,
					(void**)&pIVolume);
				if(SUCCEEDED(hr) && (!pIVolume))
					hr = E_UNEXPECTED;
				if(SUCCEEDED(hr))
				{
					VDS_VOLUME_PROP volProps;
					ZeroMemory(&volProps,sizeof(volProps));
					hr = pIVolume->GetProperties(&volProps);
					if(SUCCEEDED(hr))
					{
						if(volProps.ulFlags & VDS_VF_HIDDEN)
						{
							DebugPrintf(SV_LOG_DEBUG, "Found Hidden volume, Name = %s\n", W2A(volProps.pwszName));
							pIVolume->ClearFlags(VDS_VF_HIDDEN);
						}
						
						if(volProps.ulFlags & VDS_VF_READONLY)
						{
							DebugPrintf(SV_LOG_DEBUG, "Found Read-Only volume, Name = %s\n", W2A(volProps.pwszName));
							pIVolume->ClearFlags(VDS_VF_READONLY);
						}

						if(volProps.ulFlags & VDS_VF_SHADOW_COPY)
						{
							DebugPrintf(SV_LOG_DEBUG, "Found shadow copy flag enbaled volume, Name = %s\n", W2A(volProps.pwszName));
							pIVolume->ClearFlags(VDS_VF_SHADOW_COPY);
						}
					}
					CoTaskMemFree(volProps.pwszName);
				}
				INM_SAFE_RELEASE(pIVolume);
			}
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumVoluems);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::CheckUnKnownDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	bool isDynProvider = false;
	IVdsProvider *pProvider = NULL;
	hr = pPack->GetProvider(&pProvider);
	if(SUCCEEDED(hr) && (!pProvider))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_PROVIDER_PROP ProviderProps;
		ZeroMemory(&ProviderProps,sizeof(ProviderProps));
		hr = pProvider->GetProperties(&ProviderProps);
		if(SUCCEEDED(hr))
		{
			if(ProviderProps.ulFlags & VDS_PF_DYNAMIC)
				isDynProvider = true;
		}
		CoTaskMemFree(ProviderProps.pwszName);
		CoTaskMemFree(ProviderProps.pwszVersion);
	}
	INM_SAFE_RELEASE(pProvider);

	if(!isDynProvider)	//missing disks related to only dynamic providers
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{					
					if(VDS_DS_UNKNOWN == diskProps.status)
					{
						DebugPrintf(SV_LOG_DEBUG,"Unknown disk found : %s\n", GUIDString(diskProps.id).c_str());
						m_bAvailable = true;
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::CollectOnlineDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	bool isDynProvider = false;
	IVdsProvider *pProvider = NULL;
	hr = pPack->GetProvider(&pProvider);
	if(SUCCEEDED(hr) && (!pProvider))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_PROVIDER_PROP ProviderProps;
		ZeroMemory(&ProviderProps,sizeof(ProviderProps));
		hr = pProvider->GetProperties(&ProviderProps);
		if(SUCCEEDED(hr))
		{
			if(ProviderProps.ulFlags & VDS_PF_DYNAMIC)
				isDynProvider = true;
		}
		CoTaskMemFree(ProviderProps.pwszName);
		CoTaskMemFree(ProviderProps.pwszVersion);
	}
	INM_SAFE_RELEASE(pProvider);

	if(!isDynProvider)	//missing disks related to only dynamic providers
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				string strDisk;
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					if(VDS_DS_ONLINE == diskProps.status)
					{
						m_mapDiskToStatus.insert(make_pair(strDisk, true));
					}
					else
					{
						m_mapDiskToStatus.insert(make_pair(strDisk, false));
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::RemoveMissingDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	list<VDS_OBJECT_ID> lstMissingDisks;

	bool isDynProvider = false;
	IVdsProvider *pProvider = NULL;
	hr = pPack->GetProvider(&pProvider);
	if(SUCCEEDED(hr) && (!pProvider))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_PROVIDER_PROP ProviderProps;
		ZeroMemory(&ProviderProps,sizeof(ProviderProps));
		hr = pProvider->GetProperties(&ProviderProps);
		if(SUCCEEDED(hr))
		{
			if(ProviderProps.ulFlags & VDS_PF_DYNAMIC)
				isDynProvider = true;
		}
		CoTaskMemFree(ProviderProps.pwszName);
		CoTaskMemFree(ProviderProps.pwszVersion);
	}
	INM_SAFE_RELEASE(pProvider);

	if(!isDynProvider)	//missing disks related to only dynamic providers
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					
					if(VDS_DS_MISSING == diskProps.status)
					{
						lstMissingDisks.push_back(diskProps.id);
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);

	//Remove missing disks
	list<VDS_OBJECT_ID>::const_iterator iterDiskId = lstMissingDisks.begin();
	while(lstMissingDisks.end() != iterDiskId)
	{
		hr = pPack->RemoveMissingDisk(*iterDiskId);
		if(S_OK == hr)
		{
			DebugPrintf(SV_LOG_DEBUG, "\t%s Success\n", GUIDString(*iterDiskId).c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "\t%s Failed, ERROR: %ld\n", GUIDString(*iterDiskId).c_str(), hr);
		}
		iterDiskId++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::DeleteVolume(const VDS_OBJECT_ID & objId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IUnknown *pIUnknown = NULL;
	IVdsVolume *pIVdsVol = NULL;
	hr = m_pIVdsService->GetObject(objId,VDS_OT_VOLUME,&pIUnknown);
	if(hr == VDS_E_OBJECT_NOT_FOUND)
	{
		DebugPrintf(SV_LOG_DEBUG, "\t%s  Object not found. Can not remove volume.\n", GUIDString(objId).c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return hr;
	}
	hr = pIUnknown->QueryInterface(IID_IVdsVolume,(void**)&pIVdsVol);
	if(SUCCEEDED(hr) && (!pIVdsVol))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		hr = pIVdsVol->Delete(TRUE);
		if(hr == S_OK || hr == VDS_S_ACCESS_PATH_NOT_DELETED)
			DebugPrintf(SV_LOG_DEBUG, "\t%s Success\n", GUIDString(objId).c_str());
		else
			DebugPrintf(SV_LOG_ERROR,"\t%s Failed, ERROR %ld\n", GUIDString(objId).c_str(), hr);
	}
	INM_SAFE_RELEASE(pIVdsVol);
	INM_SAFE_RELEASE(pIUnknown);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

bool CVdsHelper::IsDiskMissing(const VDS_OBJECT_ID & diskId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = false;
	HRESULT hr;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk		= NULL;
	hr = m_pIVdsService->GetObject(diskId,VDS_OT_DISK,&pIUnknown);
	if(hr == VDS_E_OBJECT_NOT_FOUND)
	{
		DebugPrintf(SV_LOG_DEBUG, "\t%s Object not found. Can not get disk porps\n", GUIDString(diskId).c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bRet;
	}
	hr = pIUnknown->QueryInterface(IID_IVdsDisk,(void**)&pDisk);
	if(SUCCEEDED(hr) && (!pDisk))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_DISK_PROP diskProps;
		ZeroMemory(&diskProps,sizeof(diskProps));
		hr = pDisk->GetProperties(&diskProps);
		if(SUCCEEDED(hr))
		{
			
			if(VDS_DS_MISSING == diskProps.status)
			{
				bRet = true;
			}
			
			CoTaskMemFree(diskProps.pwszDiskAddress);
			CoTaskMemFree(diskProps.pwszName);
			CoTaskMemFree(diskProps.pwszDevicePath);
			CoTaskMemFree(diskProps.pwszFriendlyName);
			CoTaskMemFree(diskProps.pwszAdaptorName);
		}
	}
	INM_SAFE_RELEASE(pDisk);
	INM_SAFE_RELEASE(pIUnknown);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool CVdsHelper::AllDisksMissig(std::list<VDS_DISK_EXTENT> & lstExtents)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	std::list<VDS_DISK_EXTENT>::const_iterator iterExtent = lstExtents.begin();
	while(iterExtent != lstExtents.end())
	{
		if(!IsDiskMissing(iterExtent->diskId))
		{
			bRet = false;
			break;
		}
		iterExtent++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

HRESULT CVdsHelper::ProcessPlexes(IVdsVolume *pIVolume, std::list<VDS_DISK_EXTENT> & lstExtents)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pIEnumPlex = NULL;
	IVdsVolumePlex *pPlex = NULL;
	IUnknown *pIUnknown = NULL;
	ULONG ulFetched = 0;
	hr = pIVolume->QueryPlexes(&pIEnumPlex);
	if(SUCCEEDED(hr) && (!pIEnumPlex))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		lstExtents.clear();
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumPlex->Next(1,&pIUnknown,&ulFetched);
			if(0==ulFetched || S_FALSE == hr)
				break;
			if(SUCCEEDED(hr) && (!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsVolumePlex, (void**)&pPlex);
				if(SUCCEEDED(hr) && (!pPlex))
					hr = E_UNEXPECTED;
				if(SUCCEEDED(hr))
				{
					VDS_DISK_EXTENT *pDiskExtnts;
					LONG lNumExtents = 0;
					hr = pPlex->QueryExtents(&pDiskExtnts,&lNumExtents);
					if(SUCCEEDED(hr))
					{
						for(LONG l=0;l<lNumExtents;l++)
						{
							lstExtents.push_back(pDiskExtnts[l]);
						}
					}
					CoTaskMemFree(pDiskExtnts);
				}
				INM_SAFE_RELEASE(pPlex);
			}
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumPlex);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

HRESULT CVdsHelper::DeleteFailedVolumes(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr = S_OK;
	IEnumVdsObject *pIEnumVoluems = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsVolume *pIVolume = NULL;
	ULONG ulFetched = 0;

	std::list<VDS_OBJECT_ID> lstFailedVols;

	hr = pPack->QueryVolumes(&pIEnumVoluems);
	if(SUCCEEDED(hr) && (!pIEnumVoluems))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumVoluems->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched || hr == S_FALSE)
				break;
			if(SUCCEEDED(hr) && (!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsVolume,
					(void**)&pIVolume);
				if(SUCCEEDED(hr) && (!pIVolume))
					hr = E_UNEXPECTED;
				if(SUCCEEDED(hr))
				{
					VDS_VOLUME_PROP volProps;
					ZeroMemory(&volProps,sizeof(volProps));
					hr = pIVolume->GetProperties(&volProps);
					if(SUCCEEDED(hr))
					{
						//Extents
						std::list<VDS_DISK_EXTENT> lstExtents;
						ProcessPlexes(pIVolume,lstExtents);
						if(AllDisksMissig(lstExtents))
							lstFailedVols.push_back(volProps.id);
					}
					CoTaskMemFree(volProps.pwszName);
				}
				INM_SAFE_RELEASE(pIVolume);
			}
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumVoluems);

	//Delete filed volumes
	DeleteFailedVolumes(lstFailedVols);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

void CVdsHelper::DeleteFailedVolumes(std::list<VDS_OBJECT_ID> & lstFailedVols)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::list<VDS_OBJECT_ID>::const_iterator iterVol = lstFailedVols.begin();
	while(iterVol != lstFailedVols.end())
	{
		DeleteVolume(*iterVol);
		iterVol++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void CVdsHelper::CollectDiskScsiIDs(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				string strDisk , strScsiID;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);

					VDS_LUN_INFORMATION diskLunInfo;
					ZeroMemory(&diskLunInfo,sizeof(diskLunInfo));
					hr = pDisk->GetIdentificationData(&diskLunInfo);
					if(SUCCEEDED(hr))
					{
						if(diskLunInfo.m_szSerialNumber)
						{
							strScsiID = diskLunInfo.m_szSerialNumber;
						}
						
						CoTaskMemFree(diskLunInfo.m_szProductId);
						CoTaskMemFree(diskLunInfo.m_szProductRevision);
						CoTaskMemFree(diskLunInfo.m_szSerialNumber);
						CoTaskMemFree(diskLunInfo.m_szVendorId);
						for(ULONG n=0; n<diskLunInfo.m_cInterconnects; n++)
						{
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbAddress);
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbPort);
						}
						if(!strDisk.empty() && !strScsiID.empty())
						{
							m_helperMap[strDisk]	= strScsiID;
						}
					}
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

void CVdsHelper::CollectDiskSignatures(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);

					VDS_LUN_INFORMATION diskLunInfo;
					ZeroMemory(&diskLunInfo,sizeof(diskLunInfo));
					hr = pDisk->GetIdentificationData(&diskLunInfo);
					if(SUCCEEDED(hr))
					{
						if((FILE_DEVICE_DISK == diskProps.dwDeviceType) && !strDisk.empty())
						{
							m_mapDiskToSignature[strDisk]	= diskLunInfo.m_diskSignature;
						}
						
						CoTaskMemFree(diskLunInfo.m_szProductId);
						CoTaskMemFree(diskLunInfo.m_szProductRevision);
						CoTaskMemFree(diskLunInfo.m_szSerialNumber);
						CoTaskMemFree(diskLunInfo.m_szVendorId);
						for(ULONG n=0; n<diskLunInfo.m_cInterconnects; n++)
						{
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbAddress);
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbPort);
						}
					}
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}