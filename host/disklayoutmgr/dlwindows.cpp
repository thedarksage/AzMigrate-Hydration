#ifdef WIN32

#include <boost/algorithm/string.hpp>

#include "deviceidinformer.h"
#include "diskpartwrapper.h"
#include "dlwindows.h"
#include "inmsafeint.h"
#include "inmageex.h"

extern void GetDiskAttributes(const std::string &diskname, std::string &storagetype, VolumeSummary::FormatLabel &fmt, 
                       VolumeSummary::Vendor &volumegroupvendor, std::string &volumegroupname, std::string &diskGuid);

//*************************************************************************************************
// This function exists in both windows and linux (and implementation is platform specific).
// This function collects disks info from the machine
// Windows Platform Flow
// 1. First fetch map of disks and SCSIIDs
// 2. Fetch Multimap of disknames to volumes(VOLUME_INFO)
// 3. For each disk from the map in step 1, set all variables of structure for this disk.
// Output   - DisksInfoMap
// Returns DLM_ERR_SUCCESS on success
//*************************************************************************************************
DLM_ERROR_CODE DisksInfoCollector::GetDiskInfoMapFromSystem(DisksInfoMap_t & DisksInfoMapFromSys,
															DlmPartitionInfoSubMap_t & PartitionsInfoMapFromSys,
                                                            std::list<std::string>& erraticVolumeGuids,
															bool& bPartitionExist,
															const std::string & BinaryFile,
															const std::string & dlmFileFlag)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    do
    {
		DeviceIDInformer objDevId;
        std::map<std::string, SCSIID> mapDiskNamesToScsiIDs;
        if(false == GetDiskNamesAndScsiIds(mapDiskNamesToScsiIDs))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get disk names and corresponding SCSI Ids.\n");
            RetVal = DLM_ERR_WMI;
            break;
        }

        VolumesInfoMultiMap_t mmapDiskToVolInfo;
        if(false == GetVolumesInfo(mmapDiskToVolInfo, erraticVolumeGuids))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get volumes info from this machine.\n");
            RetVal = DLM_ERR_FETCH_VOLUMES;
            break;
        }
        DebugPrintf(SV_LOG_INFO,"Total number of volumes - %d.\n", mmapDiskToVolInfo.size());

        //Update diskinfo map all structure.
        std::map<std::string, SCSIID>::iterator iter = mapDiskNamesToScsiIDs.begin();
        for( ; iter != mapDiskNamesToScsiIDs.end(); iter++ )
        {
            //iter->first is disk name
            DISK_INFO d;

            //Update disk name
            SetDiskName(d, iter->first.c_str());

            //Update SCSI ID
            SetDiskScsiId(d, iter->second);

            //Update VOLUME_INFO vector and Volume Count
            SetDiskVolInfo(d, iter->first.c_str(), mmapDiskToVolInfo);

            //Update Disk Size
            SV_LONGLONG ds = 0;
            if(false == GetDiskSize(iter->first.c_str(), ds))
                SetDiskFlag(d, FLAG_NO_DISKSIZE);
            else
                SetDiskSize(d, ds);

			std::string strDiskSignature;
			std::string volumegroupname;
			std::string strDiskType;
			VolumeSummary::Vendor volumegroupvendor;
			VolumeSummary::FormatLabel strFmtType;
			GetDiskAttributes(iter->first, strDiskType, strFmtType, volumegroupvendor, volumegroupname, strDiskSignature); //To get the disk signature\GUID

			if(strDiskSignature.empty())
                SetDiskFlag(d, FLAG_NO_DISKSIGNATURE);
            else
			{
				DebugPrintf(SV_LOG_INFO, "Disk Signature is : %s\n", strDiskSignature.c_str());
				SetDiskSignature(d, strDiskSignature.c_str());
			}

			std::string strDeviceId = objDevId.GetID(iter->first);
			if (strDeviceId.empty())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed while trying to get the device id : %s\n", objDevId.GetErrorMessage().c_str());
				SetDiskFlag(d, FLAG_NO_DEVICEID);
			}                
            else
			{
				DebugPrintf(SV_LOG_INFO, "Disk Device id is : %s\n", strDeviceId.c_str());
				SetDiskDeviceId(d, strDeviceId.c_str());
			}

            //Update Disk Bytes Per Sector
			SV_ULONG bytesPerSector = 0;
			if (false == GetDiskBytesPerSector(iter->first.c_str(), bytesPerSector))
                SetDiskFlag(d, FLAG_NO_DISK_BYTESPERSECTOR);
            else
				SetDiskBytesPerSector(d, (SV_ULONGLONG)bytesPerSector);

			DebugPrintf(SV_LOG_INFO, "Disk type is : %s\n", strDiskType.c_str());
            //Update remaining disk info (mbr,gpt,type,formattype,ebr info)
            ProcessDiskAndUpdateDiskInfo(d, iter->first.c_str(), strDiskType);

			//collecting the required special partition informations if exists
			DlmPartitionInfoSubVec_t vecDiskPartition;
			std::string PartitionFileName;

			if((false == GetPartitionFileNameFromReg(PartitionFileName)) || (DLM_FILE_ALL == dlmFileFlag))
			{
				GetPartitionInfo(iter->first, vecDiskPartition);		//Collects all the required partitions information for that disk
				if(!vecDiskPartition.empty())
				{
					PartitionFileName = BinaryFile + DLM_DISK_PARTITION_FILE;
					DebugPrintf(SV_LOG_INFO,"Updated file name %s in DiskInfo structure\n", PartitionFileName.c_str());
					PARTITION_FILE pFile;
					inm_strcpy_s(pFile.Name, ARRAYSIZE(pFile.Name), PartitionFileName.c_str());
					d.vecPartitionFilePath.push_back(pFile);
					bPartitionExist = true;
					PartitionsInfoMapFromSys.insert(make_pair(iter->first, vecDiskPartition));
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"No Special partitions available for disk %s\n", iter->first.c_str());
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Found Partiton file name %s from Registry\n", PartitionFileName.c_str());
				PARTITION_FILE pFile;
				inm_strcpy_s(pFile.Name, ARRAYSIZE(pFile.Name), PartitionFileName.c_str());
				d.vecPartitionFilePath.push_back(pFile);
			}

            DisksInfoMapFromSys[iter->first] = d;
        }
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Function to get the map of disk names to SCSI IDs (Bus:Port:Target:Lun)
// Uses WMI to get this map
//*************************************************************************************************
bool DisksInfoCollector::GetDiskNamesAndScsiIds(std::map<std::string, SCSIID> & mapDiskNamesToScsiIDs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    mapDiskNamesToScsiIDs.clear();

    if(S_FALSE == InitCOM())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	bool bret = false;
    USES_CONVERSION;
    SCSIID ScsiId;

	BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"Select * FROM Win32_DiskDrive where SIZE>0");
    IWbemClassObject *pObj = NULL;
    IWbemServices *pServ = NULL;
    IEnumWbemClassObject *pEnum = NULL;
	IWbemLocator *pLoc = NULL;
    HRESULT hRet;

    if(!FAILED(GetWbemService(&pServ, &pLoc)))
	{
		hRet = pServ->ExecQuery(
			language,
			query,
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

		if (!FAILED(hRet))
		{
			// Get the disk drive information
			ULONG uTotal = 0;

			DebugPrintf(SV_LOG_INFO,"\nThe disks present in the host are - \n");
			bret = true ;
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

				hRet = pObj->Get(L"DeviceID", 0, &val, 0, 0);
				std::string strPhysicalDiskName = std::string(W2A(val.bstrVal));
				VariantClear(&val);

				hRet = pObj->Get(L"SCSIBus", 0, &val, 0, 0);
				ScsiId.Host = val.uintVal;
				VariantClear(&val);
				hRet = pObj->Get(L"SCSIPort", 0, &val, 0, 0);
				ScsiId.Channel = val.uintVal;
				VariantClear(&val);
				hRet = pObj->Get(L"SCSITargetID", 0, &val, 0, 0);
				ScsiId.Target = val.uintVal;
				VariantClear(&val);
				hRet = pObj->Get(L"SCSILogicalUnit", 0, &val, 0, 0);
				ScsiId.Lun = val.uintVal;
				VariantClear(&val);

				DebugPrintf(SV_LOG_INFO,"Device ID : %s    SCSI ID(Bus:Port:Target:Lun) : %u:%u:%u:%u\n",
					strPhysicalDiskName.c_str(),
					ScsiId.Host, ScsiId.Channel, ScsiId.Target, ScsiId.Lun);

				mapDiskNamesToScsiIDs[strPhysicalDiskName] = ScsiId;

				VariantClear(&val);
				pObj->Release();    // Release objects not owned.
			}
			DebugPrintf(SV_LOG_INFO,"--------------------------------\n");
			pEnum->Release();
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Query for Disk's Device ID failed.Error Code %0X\n",hRet);
		}
		pServ->Release();
	}
	else
	{
	    DebugPrintf(SV_LOG_ERROR,"Failed to initialise GetWbemService.\n");
	}

	SysFreeString(language);
	SysFreeString(query);

	if(pLoc != NULL)
		pLoc->Release();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bret ;
}

HRESULT DisksInfoCollector::InitCOM()
{
#if 0
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres))
    {
	   DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM library. Error Code %0X\n",hres);
       return S_FALSE;
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

     if (hres == RPC_E_TOO_LATE)
     {
         hres = S_OK;
     }
    if (FAILED(hres))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to initialize security  library. Error Code %0X\n",hres);
//        CoUninitialize();
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;                    // Program has failed.
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
#endif
    return S_OK;
}

HRESULT DisksInfoCollector::GetWbemService(IWbemServices **pWbemService, IWbemLocator **pLoc)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HRESULT hr;

    hr = CoCreateInstance(CLSID_WbemLocator, 0,CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) pLoc);

    if (FAILED(hr))
    {
	    DebugPrintf(SV_LOG_ERROR,"Failed to create IWbemLocator object. Error Code %0X\n",hr);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return hr;
    }

    // Connect to the root\default namespace with the current user.
    hr = (*pLoc)->ConnectServer(
            BSTR(L"ROOT\\cimv2"),
            NULL, NULL, 0, NULL, 0, 0, pWbemService);

    if (FAILED(hr))
    {
		DebugPrintf(SV_LOG_ERROR,"Could not connect to server. Error Code %0X\n",hr);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return hr;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;
}

//*************************************************************************************************
// Fetches the disk size for given disk name
//*************************************************************************************************
bool DisksInfoCollector::GetDiskSize(const char *DiskName, SV_LONGLONG & DiskSize)
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

//*************************************************************************************************
// Gets the map of disk names to volumes info (This is multi map)
//*************************************************************************************************
bool DisksInfoCollector::GetVolumesInfo(VolumesInfoMultiMap_t & mmapDiskToVolInfo,
                                        std::list<std::string>& erraticVolumeGuids)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bFlag = true;
    do
    {
        std::map<std::string,std::string> mapOfGuidsWithLogicalNames;
        if( FALSE == GetVolumesPresentInSystem(mapOfGuidsWithLogicalNames, erraticVolumeGuids) )
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the list of volumes from this machine.\n");
			bFlag = false;
			break;
        }

        if( false == GetMapOfDiskToVolInfo(mapOfGuidsWithLogicalNames, mmapDiskToVolInfo))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the multimap of volumes on disks.\n");
			bFlag = false;
			break;
        }
    }while(0);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bFlag;
}

//*************************************************************************************************
// In this function, we are filling volumes guids "mapOfGuidsWithLogicalNames"
// by using the "EnumerateVolumes" function call.
//*************************************************************************************************
BOOL DisksInfoCollector::GetVolumesPresentInSystem(std::map<std::string,std::string> & mapOfGuidsWithLogicalNames,
                                                   std::list<std::string>& erraticVolumeGuids)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HANDLE volumeHandle;

    TCHAR buf[SV_MAX_PATH];
    BOOL bFlag;

    volumeHandle = FindFirstVolume(buf, SV_MAX_PATH);
    if (volumeHandle == INVALID_HANDLE_VALUE)
    {
        DebugPrintf(SV_LOG_ERROR,"No volume found in entire sytem.\n");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return FALSE;
    }
    //bFlag = EnumerateVolumes(mapOfGuidsWithLogicalNames, volumeHandle, buf, MAX_PATH_COMMON);
    SV_INT volCounter = 1; // volume counter to name the volumes with no volume name.

    while ( true )
    {
        bFlag = EnumerateVolumes(mapOfGuidsWithLogicalNames, volumeHandle, buf, SV_MAX_PATH, volCounter,
                            erraticVolumeGuids);
        if( bFlag == TRUE )
        {
            bFlag = FindNextVolume(volumeHandle, buf, SV_MAX_PATH);

            if( bFlag == FALSE )
            {
                SV_ULONG dwErrorCode = GetLastError();
                if( dwErrorCode == ERROR_NO_MORE_FILES /*|| dwErrorCode == ERROR_FILE_NOT_FOUND*/ )
                {
                    bFlag = TRUE ;
                }
                else
                    DebugPrintf(SV_LOG_ERROR,"FindNextVolume failed with Error Code: %lu\n",dwErrorCode ) ;
                break ;
            }
        }
        else
            break ;
    }
    FindVolumeClose(volumeHandle);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bFlag;
}

//*************************************************************************************************
// This is a helper function of "GetVolumesPresentInSystem".
// Process each volume.
// Discard the fix drives.
// Enumerate volumes till FindNextVolume() returns "FALSE".
//*************************************************************************************************
BOOL DisksInfoCollector::EnumerateVolumes(std::map<std::string,std::string> & mapOfGuidsWithLogicalNames,
                                          HANDLE volumeHandle, TCHAR *szBuffer, SV_ULONG iBufSize,
                                          SV_INT & volCounter, std::list<std::string>& erraticVolumeGuids)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    BOOL bFlag = TRUE ;

    if(DRIVE_FIXED == GetDriveType(szBuffer))
    {
        std::string strVolumeGuid = std::string(szBuffer);
        char szVolumePath[SV_MAX_PATH + 1] = {0};
        DWORD dwVolPathSize = 0;
		char *pszVolumePath = NULL;//new char(MAX_PATH);
		TCHAR DeviceName[SV_MAX_PATH];

		std::string strTemp = "\\\\?\\";
		size_t len = strTemp.length();
		size_t lastPos = strVolumeGuid.find_last_of("\\");
		std::string strQueryDosDev = strVolumeGuid.substr(len, lastPos-len);

		DWORD CharCount = 0;
		CharCount = QueryDosDevice(strQueryDosDev.c_str(), DeviceName, SV_MAX_PATH);

		if(CharCount != 0)
		{
#ifndef SCOUT_WIN2K_SUPPORT  //hiding the GetVolumePathNamesForVolumeName for Win2K
			if( GetVolumePathNamesForVolumeName(strVolumeGuid.c_str(), szVolumePath, SV_MAX_PATH, &dwVolPathSize))
			{
				if(strlen(szVolumePath) == 0)
				{
					inm_sprintf_s(szVolumePath, ARRAYSIZE(szVolumePath), "%s%d", VOLUME_DUMMYNAME_PREFIX, volCounter++);
				}
				if( false == isVirtualVolume(std::string(szVolumePath)))
				{
					mapOfGuidsWithLogicalNames[strVolumeGuid] = std::string(szVolumePath);
				}
			}
			else
			{
				if(GetLastError() == ERROR_MORE_DATA)
				{
					pszVolumePath = new char[dwVolPathSize];
					if(!pszVolumePath)
					{
						DebugPrintf(SV_LOG_ERROR,"\n Failed to allocate required memory to get the list of Mount Point Paths.");
					}
					else
					{
						if(GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(), pszVolumePath, dwVolPathSize, &dwVolPathSize))
						{
							if(strlen(szVolumePath) == 0)
							{
								inm_sprintf_s(szVolumePath, ARRAYSIZE(szVolumePath), "%s%d", VOLUME_DUMMYNAME_PREFIX, volCounter++);
							}
							if( false == isVirtualVolume(std::string(szVolumePath)))
							{
								mapOfGuidsWithLogicalNames[strVolumeGuid] = std::string(szVolumePath);
							}
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName failed for %s, %s with Error Code: %lu\n",strVolumeGuid.c_str(), szVolumePath, GetLastError());
							erraticVolumeGuids.push_back( strVolumeGuid ) ;
						}
						delete [] pszVolumePath ;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName failed for %s, %s with Error Code: %lu\n",strVolumeGuid.c_str(), szVolumePath, GetLastError());
					erraticVolumeGuids.push_back( strVolumeGuid ) ;
				}
			}
#else
			//Need to write new code to get the volume names. Fornow skipping it.
			DebugPrintf(SV_LOG_INFO,"WiN2K macro is enabled, skipping GetVolumePathNamesForVolumeName\n");
			DebugPrintf(SV_LOG_INFO,"Volumepath not found for volume GUID : %s\n", strVolumeGuid.c_str());
#endif
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"QueryDosDevice Failed with ErrorCode : [%lu] for Volume GUID %s\n",GetLastError(), strVolumeGuid.c_str());
			erraticVolumeGuids.push_back( strVolumeGuid ) ;
		}
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

    return (bFlag);
}

//*************************************************************************************************
// Dumps the extents for the specified volume.
// Finding the extent of logical volume on the Disks.
// Here we're Filling the volInfo structure with the corresponding volume information and
// Then Using the "mmapDiskToVolInfo" to store the volInfo struct with the corresponding DiskName.
// In mmapDiskToVolInfo,key is DiskName and corresponding value is VolInfo structure.
//*************************************************************************************************
bool DisksInfoCollector::GetMapOfDiskToVolInfo( std::map<std::string,std::string> mapOfGuidsWithLogicalNames,
                                               VolumesInfoMultiMap_t & mmapDiskToVolInfo
                                               )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	std::map<std::string,std::string>::iterator iter_beg = mapOfGuidsWithLogicalNames.begin();
	std::map<std::string,std::string>::iterator iter_end = mapOfGuidsWithLogicalNames.end();
	HANDLE	hVolume ;
	while(iter_beg != iter_end)
	{
        // Remove the trailing "\" from volume GUID
        std::string strTemp = iter_beg->first;
        std::string::size_type pos = strTemp.find_last_of("\\");
        if(pos != std::string::npos)
        {
            strTemp.erase(pos);
        }

        DebugPrintf(SV_LOG_INFO, "%s ==> %s\n", strTemp.c_str(), iter_beg->second.c_str());

        hVolume = INVALID_HANDLE_VALUE ;
		hVolume = CreateFile(strTemp.c_str(),GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT  , NULL
                             );
		if( hVolume == INVALID_HANDLE_VALUE )
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open handle for volume-%s with Error Code - %d \n", strTemp.c_str(), GetLastError());
			bResult = false;
			break;
		}

		ULONG	bytesWritten;
        UCHAR	DiskExtentsBuffer[0x400];
		DWORD returnBufferSize = sizeof(DiskExtentsBuffer);
        PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
		if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                            NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer),
                            &bytesWritten, NULL) )
		{
			for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ )
			{
				SV_ULONG uDiskNumber;
				VOLUME_INFO obj;
				uDiskNumber			= DiskExtents->Extents[extent].DiskNumber;
                obj.StartingOffset  = DiskExtents->Extents[extent].StartingOffset.QuadPart;
                obj.VolumeLength    = DiskExtents->Extents[extent].ExtentLength.QuadPart;
				obj.EndingOffset    = obj.StartingOffset + obj.VolumeLength;

				char DiskName[MAX_PATH_COMMON + 1];
				inm_sprintf_s(DiskName, ARRAYSIZE(DiskName), "%s%lu", WIN_DISKNAME_PREFIX, uDiskNumber);
				inm_strncpy_s(obj.VolumeGuid, ARRAYSIZE(obj.VolumeGuid), iter_beg->first.c_str(), MAX_PATH_COMMON);
				obj.VolumeGuid[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1
				inm_strncpy_s(obj.VolumeName, ARRAYSIZE(obj.VolumeName), iter_beg->second.c_str(), MAX_PATH_COMMON);
				obj.VolumeName[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1
				mmapDiskToVolInfo.insert(std::make_pair(std::string(DiskName),obj));
				DebugPrintf(SV_LOG_INFO,"DiskName        = %s\n",DiskName);
				DebugPrintf(SV_LOG_INFO,"VolumeName      = %s\n",obj.VolumeName);
				DebugPrintf(SV_LOG_INFO,"VolumeLength    = %lld\n",obj.VolumeLength);
				DebugPrintf(SV_LOG_INFO,"StartingOffset  = %lld\n",obj.StartingOffset);
				DebugPrintf(SV_LOG_INFO,"EndingOffset    = %lld\n",obj.EndingOffset);
				DebugPrintf(SV_LOG_INFO,"--------------------------------\n");
			}
		}
		else
		{
			DWORD error = GetLastError();
            DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed for volume-%s with Error code - %lu.\n", strTemp.c_str(), error);
			if(error == ERROR_MORE_DATA)
			{
                DWORD nde;
                INM_SAFE_ARITHMETIC(nde = InmSafeInt<DWORD>::Type(DiskExtents->NumberOfDiskExtents) - 1, INMAGE_EX(DiskExtents->NumberOfDiskExtents))
				INM_SAFE_ARITHMETIC(returnBufferSize = sizeof(VOLUME_DISK_EXTENTS) + (nde * InmSafeInt<size_t>::Type(sizeof(DISK_EXTENT))), INMAGE_EX(sizeof(VOLUME_DISK_EXTENTS))(nde)(sizeof(DISK_EXTENT)))
				DiskExtents = (PVOLUME_DISK_EXTENTS) new UCHAR[returnBufferSize];

				if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                            NULL, 0, DiskExtents, returnBufferSize,
                            &bytesWritten, NULL) )
				{

					for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ )
					{
						SV_ULONG uDiskNumber;
						VOLUME_INFO obj;
						uDiskNumber			= DiskExtents->Extents[extent].DiskNumber;
						obj.StartingOffset  = DiskExtents->Extents[extent].StartingOffset.QuadPart;
						obj.VolumeLength    = DiskExtents->Extents[extent].ExtentLength.QuadPart;
						obj.EndingOffset    = obj.StartingOffset + obj.VolumeLength;

						char DiskName [MAX_PATH_COMMON + 1];
						inm_sprintf_s(DiskName, ARRAYSIZE(DiskName), "%s%lu", WIN_DISKNAME_PREFIX, uDiskNumber);
						inm_strncpy_s(obj.VolumeGuid, ARRAYSIZE(obj.VolumeGuid), iter_beg->first.c_str(), MAX_PATH_COMMON);
						obj.VolumeGuid[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1
						inm_strncpy_s(obj.VolumeName, ARRAYSIZE(obj.VolumeName), iter_beg->second.c_str(), MAX_PATH_COMMON);
						obj.VolumeName[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1
						mmapDiskToVolInfo.insert(std::make_pair(std::string(DiskName),obj));
						DebugPrintf(SV_LOG_INFO,"DiskName        = %s\n",DiskName);
						DebugPrintf(SV_LOG_INFO,"VolumeName      = %s\n",obj.VolumeName);
						DebugPrintf(SV_LOG_INFO,"VolumeLength    = %lld\n",obj.VolumeLength);
						DebugPrintf(SV_LOG_INFO,"StartingOffset  = %lld\n",obj.StartingOffset);
						DebugPrintf(SV_LOG_INFO,"EndingOffset    = %lld\n",obj.EndingOffset);
						DebugPrintf(SV_LOG_INFO,"--------------------------------\n");
					}
					delete [] DiskExtents ;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed for Volume: %s with Error code : [%lu].\n",(iter_beg->first).c_str(), GetLastError());
					bResult = false;
					delete [] DiskExtents ;
					break;
				}
			}
			else if(error == ERROR_INVALID_FUNCTION)
			{
#ifndef SCOUT_WIN2K_SUPPORT  //hiding the GetVolumePathNamesForVolumeName for W2K
				char szVolumePath[MAX_PATH+1] = {0};
				DWORD dwVolPathSize = 0;
				if(GetVolumePathNamesForVolumeName (iter_beg->first.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
				{
					DebugPrintf(SV_LOG_INFO,"GetVolumePathNamesForVolumeName succeed on getting the Volume Name: %s  for Volume GUID %s\n", szVolumePath, iter_beg->first.c_str());
					DebugPrintf(SV_LOG_INFO,"Might Be Some issue on enumerating the Volumes...\n");
					bResult = false;
					break;
				}
				else
				{
					if(GetLastError() == ERROR_MORE_DATA)
					{
						DebugPrintf(SV_LOG_INFO,"GetVolumePathNamesForVolumeName succeed on getting the Volume Name: %s  for Volume GUID %s\n", szVolumePath, iter_beg->first.c_str());
						DebugPrintf(SV_LOG_INFO,"Might Be Some issue on enumerating the Volumes...\n");
						bResult = false;
						break;
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : [%lu] for Volume GUID %s\n",GetLastError(), iter_beg->first.c_str());
						DebugPrintf(SV_LOG_INFO, "The Volume GUID: %s is not exist, So skipping it and proceeding with other Volumes\n",iter_beg->first.c_str());
					}
				}
#else
				//Need to write new code to get the volume names. Fornow skipping it.
				DebugPrintf(SV_LOG_INFO,"WIN2K macro is enabled, skipping GetVolumePathNamesForVolumeName\n");
				bResult = false;
				break;
#endif
			}
			else if(error == ERROR_NOT_READY)
			{
				DebugPrintf(SV_LOG_INFO,"Checking for Cluster configured or not\n");
				DWORD nodeStatus;
				if(GetNodeClusterState(NULL, &nodeStatus) == ERROR_SUCCESS)
				{
					if((nodeStatus == 0) || (nodeStatus == 1))
					{
						DebugPrintf(SV_LOG_ERROR,"Current node is not cluster, Failing over here\n");
						DebugPrintf(SV_LOG_ERROR, "Assuming this as an error and failing to collect the volume to disk mapping\n");
						bResult = false;
						break;
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Current node is a cluster, So skipping to get the disk extents for volume %s\n", strTemp.c_str());
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the node state by calling API GetNodeClusterState(), Error : [%lu]\n", GetLastError());
					DebugPrintf(SV_LOG_ERROR, "Assuming this as an error and failing to collect the volume to disk mapping\n");
					bResult = false;
					break;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Failed to get the volume to disk mapping\n");
				bResult = false;
				break;
			}
		}
		CloseHandle(hVolume);
		hVolume = INVALID_HANDLE_VALUE ;
		iter_beg++;
	}
	if( hVolume != INVALID_HANDLE_VALUE )
	{
		CloseHandle(hVolume);
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//*************************************************************************************************
// Fetches the Bytes Per Sector in given disk
//*************************************************************************************************
bool DisksInfoCollector::GetDiskBytesPerSector(const char * DiskName, SV_ULONG & BytesPerSector)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    //for now, return 512 bytes always. 4k disks yet to come in the market.
    //for now, we can return 512 bytes always. since 4k disks are yet to come in the market.
    //Anyways keep code ready.
    BytesPerSector = DISK_BYTES_PER_SECTOR;

    bool bFlag = true;
    DISK_GEOMETRY dInfo;
    DWORD dInfoSize = sizeof(DISK_GEOMETRY);
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
                                IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                NULL, 0,
                                &dInfo, dInfoSize,
                                &dwValue, NULL);
        if (bFlag)
        {
            BytesPerSector = dInfo.BytesPerSector;
	        CloseHandle(hPhysicalDriveIOCTL);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_GET_DRIVE_GEOMETRY failed for disk %s with Error code: %lu\n", DiskName, GetLastError());
	        CloseHandle(hPhysicalDriveIOCTL);
	        bFlag = false;
	        break;
        }
        DebugPrintf(SV_LOG_DEBUG, "Disk(%s) : ByterPerSector = %lu\n", DiskName, BytesPerSector);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bFlag;
}

//*************************************************************************************************
// This function will refresh the given disk. This will be called after updating MBRs/GPTs on a disk.
// Disk is refreshed using the ioctl - IOCTL_DISK_UPDATE_PROPERTIES
//*************************************************************************************************
bool DisksInfoCollector::RefreshDisk(const char * DiskName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool RetVal = false;

    do
    {
        HANDLE diskHandle;
        diskHandle = CreateFile(DiskName,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL
                                );
        if(INVALID_HANDLE_VALUE == diskHandle)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the disk handle for disk(%s) with Error code: %lu\n", DiskName, GetLastError());
	        RetVal = false;
	        break;
        }

        DWORD lpBytesReturned;
        RetVal = DeviceIoControl((HANDLE) diskHandle, // handle to device
                                IOCTL_DISK_UPDATE_PROPERTIES,// dwIoControlCode
                                NULL, // lpInBuffer
                                0, // nInBufferSize
                                NULL, // lpOutBuffer
                                0, // nOutBufferSize
                                (LPDWORD)&lpBytesReturned, // lpBytesReturned
                                (LPOVERLAPPED)NULL // lpOverlapped
                                );
        if(false == RetVal)
        {
	        DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_UPDATE_PROPERTIES failed for the disk(%s) with Error Code : %lu\n", DiskName, GetLastError());
	        CloseHandle(diskHandle);
	        RetVal = false;
            break;
        }
        CloseHandle(diskHandle);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Get New Target Disk Info Map from the system.
// This is required because this will be called after restore disk structure call.
// Compare the volumes offsets of source map and target map of corresponding disks
// and get the map of corresponding source and target volumes.
//*************************************************************************************************
DLM_ERROR_CODE DisksInfoCollector::RestoreMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
                                                      std::list<std::string>& erraticVolumeGuids,
                                                      int mountNameType)
{
	//Need to work on this

    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    //Get the target disk info map.
    DisksInfoMap_t  TgtMapDiskNamesToDiskInfo;
	DlmPartitionInfoSubMap_t TgtMapDiskToPartitionInfo;
	bool bPartitionExist;
    RetVal = GetDiskInfoMapFromSystem(TgtMapDiskNamesToDiskInfo, TgtMapDiskToPartitionInfo, erraticVolumeGuids, bPartitionExist);
    if(DLM_ERR_SUCCESS != RetVal)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to collect the disks info from the system. Error Code - %u\n", RetVal);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return RetVal;
    }

    //For each source and target disk
    for(StringArrMapIter_t iterDisks = MapSrcToTgtDiskNames.begin();
        iterDisks != MapSrcToTgtDiskNames.end();
        iterDisks++)
    {
        DebugPrintf(SV_LOG_INFO,"SrcDisk = %s <==> TgtDisk = %s\n", iterDisks->first.c_str(), iterDisks->second.c_str());

        //Get the corresponding diskinfoiter from the diskinfomap
        DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find(iterDisks->first);
        if(iterSrc == SrcMapDiskNamesToDiskInfo.end()) //Src
        {
            DebugPrintf(SV_LOG_ERROR,"The input Source disk(%s) is not found in Source DiskInfo Map.\n",iterDisks->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }
        DisksInfoMapIter_t iterTgt = TgtMapDiskNamesToDiskInfo.find(iterDisks->second);
        if(iterTgt == TgtMapDiskNamesToDiskInfo.end()) //Tgt
        {
            DebugPrintf(SV_LOG_ERROR,"The input Target disk(%s) is not found in Target DiskInfo Map.\n",iterDisks->second.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }

        //Get the VolumesInfo of these disks.
        if(0 == iterSrc->second.DiskInfoSub.VolumeCount) //Src
        {
            //This is fine as a source disk can be empty.
            DebugPrintf(SV_LOG_INFO,"No volumes are present on the Source disk(%s).\n",iterSrc->first.c_str());
            continue;
        }
        VolumesInfoVec_t vecSrcVolInfo = iterSrc->second.VolumesInfo;
        if(0 == iterTgt->second.DiskInfoSub.VolumeCount) //Tgt
        {
            //This means few source volumes will not have corresponding target volumes.
            //So not fine. hence return DLM_ERR_INCOMPLETE
            DebugPrintf(SV_LOG_ERROR,"No volumes are present on the Target disk(%s).\n",iterTgt->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }
        VolumesInfoVec_t vecTgtVolInfo = iterTgt->second.VolumesInfo;

        //Compare the VolumesInfo vectors and generate the map of volumes.
        VolumesInfoVecIter_t iterSrcVol = vecSrcVolInfo.begin(); //Src
        VolumesInfoVecIter_t iterSrcVolLast = vecSrcVolInfo.end(); //Src
        for(; iterSrcVol != iterSrcVolLast; iterSrcVol++) //Src
        {
            DebugPrintf(SV_LOG_DEBUG,"SourceVolumeName = %s\n",iterSrcVol->VolumeName);
            bool bMatchExist = false;
            VolumesInfoVecIter_t iterTgtVol = vecTgtVolInfo.begin(); //Tgt
            VolumesInfoVecIter_t iterTgtVolLast = vecTgtVolInfo.end(); //Tgt
            for(; iterTgtVol != iterTgtVolLast; iterTgtVol++) //Tgt
            {
                DebugPrintf(SV_LOG_DEBUG,"TargetVolumeName = %s\n",iterTgtVol->VolumeName);
                if((iterSrcVol->StartingOffset == iterTgtVol->StartingOffset)
                    && (iterSrcVol->EndingOffset == iterTgtVol->EndingOffset)
                    && (iterSrcVol->VolumeLength == iterTgtVol->VolumeLength))
                {
                    DebugPrintf(SV_LOG_DEBUG,"Found the match.\n");
                    if(strlen(iterSrcVol->VolumeName) > 0)
                    {
                         //if(0 == strlen(iterTgtVol->VolumeName))
                        if( 0 == strncmp(iterTgtVol->VolumeName, VOLUME_DUMMYNAME_PREFIX, sizeof(VOLUME_DUMMYNAME_PREFIX)-1) )
                        {
							bool bMount = true;
							std::string volName = "";
							char szVolumePath[SV_MAX_PATH + 1] = {0};
							DWORD dwVolPathSize = 0;
							char *pszVolumePath = NULL;//new char(MAX_PATH);

#ifndef SCOUT_WIN2K_SUPPORT  //hiding the GetVolumePathNamesForVolumeName for Win2K
							//Check the volume is mounted with proper volumename as source
							//If having different volume name as pf source then delete it ans assign source mount point
							if(GetVolumePathNamesForVolumeName(iterTgtVol->VolumeGuid, szVolumePath, SV_MAX_PATH, &dwVolPathSize))
							{
								volName = std::string(szVolumePath);
								DebugPrintf(SV_LOG_INFO,"Found the volume name (%s) for vomue guid %s\n", volName.c_str(), iterTgtVol->VolumeGuid);
							}
							else
							{
								if(GetLastError() == ERROR_MORE_DATA)
								{
									pszVolumePath = new char[dwVolPathSize];
									if(!pszVolumePath)
									{
										DebugPrintf(SV_LOG_WARNING,"\n Failed to allocate required memory to get the list of Mount Point Paths.");
									}
									else
									{
										if(GetVolumePathNamesForVolumeName (iterTgtVol->VolumeGuid, pszVolumePath, dwVolPathSize, &dwVolPathSize))
										{
											DebugPrintf(SV_LOG_INFO,"Found the volume name (%s) for vomue guid %s\n", volName.c_str(), iterTgtVol->VolumeGuid);
											volName = std::string(szVolumePath);
										}
										else
										{
											DebugPrintf(SV_LOG_WARNING,"GetVolumePathNamesForVolumeName failed for %s, %s with Error Code: %lu\n",iterTgtVol->VolumeGuid, szVolumePath, GetLastError());
										}
										delete [] pszVolumePath ;
									}
								}
								else
								{
									DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName failed for %s, %s with Error Code: %lu\n",iterTgtVol->VolumeGuid, szVolumePath, GetLastError());
								}
							}
#else
							//Need to write new code to get the volume names. Fornow skipping it.
							DebugPrintf(SV_LOG_INFO,"WiN2K macro is enabled, skipping GetVolumePathNamesForVolumeName\n");
							DebugPrintf(SV_LOG_INFO,"Volumepath not found for volume GUID : %s\n", iterTgtVol->VolumeGuid);
#endif
							if(!volName.empty())
							{
								if(volName.compare(std::string(iterSrcVol->VolumeName)) != 0)
								{
									//delete the existing mount point C:\inmage_part\_0
									if(0 == DeleteVolumeMountPoint(volName.c_str()))
									{
										DebugPrintf(SV_LOG_WARNING,"DeleteVolumeMountPoint failed with ErrorCode : [%lu].\n",GetLastError());
									}
								}
								else
									bMount = false;
							}

							if(bMount)
							{
								// only mount if the mount name type was set. for now assuems single char is drive letter and more then 1 char is mount point
								int volumeNameLen = strlen(iterSrcVol->VolumeName);
								if( ( ( mountNameType & (MOUNT_NAME_TYPE_DRIVE_LETTER) ) > 0 && volumeNameLen < 4 )     // drive letter can be "C" "C:" or even "C:\"
									|| ( ( mountNameType & (MOUNT_NAME_TYPE_MOUNT_POINT) ) > 0 && volumeNameLen > 3 ) ) // mount point must be at least 4 chars on windows
								{
									DebugPrintf(SV_LOG_INFO,"Target Volume Name is not mounted for Source Volume(%s)\n",iterSrcVol->VolumeName);
									if(SetVolumeMountPoint(iterSrcVol->VolumeName, iterTgtVol->VolumeGuid))
									{
										DebugPrintf(SV_LOG_INFO,"Assigned the same mount point to matched volume(GUID- %s)\n", iterTgtVol->VolumeGuid);
									}
									else
									{
										DebugPrintf(SV_LOG_ERROR,"Failed to assign the same mount point to the matched volume(GUID- %s)\n", iterTgtVol->VolumeGuid);
										DebugPrintf(SV_LOG_ERROR,"Error Code - %lu\n",GetLastError());
										RetVal = DLM_ERR_UNABLE_TO_ASSIGN_SAME_MOUNT_POINT;
									}
								}
							}
                        }
                    }
                    //MapSrcToTgtVolumeNames[iterSrcVol->VolumeName] = iterTgtVol->VolumeName;
                    bMatchExist = true;
                    break;
                }
            }
            if(false == bMatchExist)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the corresponding Target volume for the Source volume(%s).\n",iterSrcVol->VolumeName);
                RetVal = DLM_ERR_INCOMPLETE;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}


DLM_ERROR_CODE DisksInfoCollector::RestoreMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
													  DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
													  std::map<std::string, std::string> mapOfSrcVolToTgtVol)
{
	//Need to work on this

    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__); 
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    //For each source and target disk
    for(StringArrMapIter_t iterDisks = MapSrcToTgtDiskNames.begin(); 
        iterDisks != MapSrcToTgtDiskNames.end();
        iterDisks++)
    {
        DebugPrintf(SV_LOG_INFO,"SrcDisk = %s <==> TgtDisk = %s\n", iterDisks->first.c_str(), iterDisks->second.c_str());

        //Get the corresponding diskinfoiter from the diskinfomap
        DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find(iterDisks->first);
        if(iterSrc == SrcMapDiskNamesToDiskInfo.end()) //Src
        {
            DebugPrintf(SV_LOG_ERROR,"The input Source disk(%s) is not found in Source DiskInfo Map.\n",iterDisks->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }
        DisksInfoMapIter_t iterTgt = TgtMapDiskNamesToDiskInfo.find(iterDisks->second);
        if(iterTgt == TgtMapDiskNamesToDiskInfo.end()) //Tgt
        {
            DebugPrintf(SV_LOG_ERROR,"The input Target disk(%s) is not found in Target DiskInfo Map.\n",iterDisks->second.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        } 

        //Get the VolumesInfo of these disks.
        if(0 == iterSrc->second.DiskInfoSub.VolumeCount) //Src
        {
            //This is fine as a source disk can be empty.
            DebugPrintf(SV_LOG_INFO,"No volumes are present on the Source disk(%s).\n",iterSrc->first.c_str());
            continue;
        }
        VolumesInfoVec_t vecSrcVolInfo = iterSrc->second.VolumesInfo;
        if(0 == iterTgt->second.DiskInfoSub.VolumeCount) //Tgt
        {
            //This means few source volumes will not have corresponding target volumes.
            //So not fine. hence return DLM_ERR_INCOMPLETE
            DebugPrintf(SV_LOG_ERROR,"No volumes are present on the Target disk(%s).\n",iterTgt->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }
        VolumesInfoVec_t vecTgtVolInfo = iterTgt->second.VolumesInfo;

        //Compare the VolumesInfo vectors and generate the map of volumes.
        VolumesInfoVecIter_t iterSrcVol = vecSrcVolInfo.begin(); //Src
        VolumesInfoVecIter_t iterSrcVolLast = vecSrcVolInfo.end(); //Src       
        for(; iterSrcVol != iterSrcVolLast; iterSrcVol++) //Src
        {
            DebugPrintf(SV_LOG_DEBUG,"SourceVolumeName = %s\n",iterSrcVol->VolumeName);
            bool bMatchExist = false;
            VolumesInfoVecIter_t iterTgtVol = vecTgtVolInfo.begin(); //Tgt
            VolumesInfoVecIter_t iterTgtVolLast = vecTgtVolInfo.end(); //Tgt
            for(; iterTgtVol != iterTgtVolLast; iterTgtVol++) //Tgt
            {
                DebugPrintf(SV_LOG_DEBUG,"TargetVolumeName = %s\n",iterTgtVol->VolumeName);
                if((iterSrcVol->StartingOffset == iterTgtVol->StartingOffset)
                    && (iterSrcVol->EndingOffset == iterTgtVol->EndingOffset)
                    && (iterSrcVol->VolumeLength == iterTgtVol->VolumeLength))
                {
                    DebugPrintf(SV_LOG_DEBUG,"Found the match.\n");

					//This is different because in case of vCon we use target mount points name different
					std::string strSrcVolumeName = iterSrcVol->VolumeName;
					std::string strTgtVolGuid = iterTgtVol->VolumeGuid;
					std::string strTgtVolName = iterTgtVol->VolumeName;
					std::map<std::string, std::string>::iterator iterVolMap = mapOfSrcVolToTgtVol.find(strSrcVolumeName);
					if(iterVolMap != mapOfSrcVolToTgtVol.end())
					{
						DebugPrintf(SV_LOG_INFO,"Source Volume = %s <====> Target Volume = %s\n",strSrcVolumeName.c_str(),iterVolMap->second.c_str());
						if(FALSE == CreateDirectory(iterVolMap->second.c_str(),NULL))
						{
							if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
							{
								DebugPrintf(SV_LOG_INFO,"Directory already exists. Going to reuse the directory.\n");            				
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to create Mount Directory - %s. ErrorCode : [%lu].\n",iterVolMap->second.c_str(),GetLastError());
								continue;
							}
						}

						if(strSrcVolumeName.compare(iterTgtVol->VolumeName) != 0)
						{
							if(!strTgtVolName.empty())
							{
								if(0 == DeleteVolumeMountPoint(strTgtVolName.c_str()))
								{
									DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint for target volume [%s] failed with ErrorCode : [%lu].\n",strTgtVolName.c_str(), GetLastError());
								}
							}
							if(0 == SetVolumeMountPoint(iterVolMap->second.c_str(),iterTgtVol->VolumeGuid))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to mount %s at %s with Error Code : [%lu].\n",iterTgtVol->VolumeGuid, iterVolMap->second.c_str(), GetLastError());
							}
							else
								DebugPrintf(SV_LOG_ERROR,"Successfully assigned the mount point [%s] to the volume(GUID- %s)\n", iterVolMap->second.c_str(), iterTgtVol->VolumeGuid);
						}
					}
					else
						DebugPrintf(SV_LOG_INFO,"Source Volume(%s) does not exist in source to target volume map\n",iterSrcVol->VolumeName);

                    //MapSrcToTgtVolumeNames[iterSrcVol->VolumeName] = iterTgtVol->VolumeName;
                    bMatchExist = true;
                    break;
                }
            }
            if(false == bMatchExist)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the corresponding Target volume for the Source volume(%s).\n",iterSrcVol->VolumeName);                
                RetVal = DLM_ERR_INCOMPLETE;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__); 
    return RetVal;
}

//*************************************************************************************************
//  This function verifies whether the volume is virtual or not
//  It verifies by issuing inmage ioctl (IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT)
//  This ioctl succeeds only it is a virtual volume. Hence return true if succeeds else return false.
//*************************************************************************************************
bool DisksInfoCollector::isVirtualVolume(std::string sVolumeName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = false;

    DebugPrintf(SV_LOG_DEBUG,"Volume Name - %s\n",sVolumeName.c_str());

    do
    {
        HANDLE       VVCtrlDevice;
	    DWORD        BytesReturned = 0;
        std::string  UniqueId;
        char         VolumeLink[256];

        memset((void*) VolumeLink, 0, 256*sizeof(char)); //Init to null.

        VVCtrlDevice = CreateFile ( VV_CONTROL_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                    NULL, NULL);

        if(INVALID_HANDLE_VALUE != VVCtrlDevice)
	    {
            UniqueId = VSNAP_UNIQUE_ID_PREFIX;

            if (sVolumeName.size() > 3)
            {
	            UniqueId += _strlwr(const_cast<char*>(sVolumeName.c_str()));
            }
            else
            {
                UniqueId += _strupr(const_cast<char*>(sVolumeName.c_str()));
            }

            DebugPrintf(SV_LOG_DEBUG,"Volume Name(after processing) - %s\n",UniqueId.c_str());

            bool bResult = DeviceIoControl(VVCtrlDevice,
				            IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
				            (void *)UniqueId.c_str(),
				            (SV_ULONG) (UniqueId.size() + 1),
				            VolumeLink,
				            (SV_ULONG) sizeof(VolumeLink),
				            &BytesReturned,
				            NULL);
            //This ioctl succeeds if the volume is virtual.
            if ( bResult )
	        {
                DebugPrintf(SV_LOG_DEBUG,"Virtual Volume\n");
		        rv = true;
	        }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Normal Volume\n");
		        rv = false;
            }

            if ( INVALID_HANDLE_VALUE != VVCtrlDevice )
                CloseHandle(VVCtrlDevice);
        }
    } while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


//Writes checksum details to a file
DLM_ERROR_CODE DisksInfoCollector::WriteDlmInfoIntoReg(const std::string& strRegValue, const std::string& strRegKey)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DLM_ERROR_CODE dlmRet = DLM_ERR_SUCCESS;

	HKEY hKey;
	std::string strDlmChecksum = "";
	do
	{
		std::string RegPathToOpen = DLM_SOFTWARE_HIVE + std::string("\\Wow6432Node\\SV Systems");

		long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_SET_VALUE, &hKey);

		if( lRV != ERROR_SUCCESS )
		{
			DebugPrintf(SV_LOG_WARNING,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", RegPathToOpen.c_str(), lRV);

			RegPathToOpen = DLM_SOFTWARE_HIVE + std::string("\\SV Systems");
			lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_SET_VALUE, &hKey);
			if( lRV != ERROR_SUCCESS )
			{
				DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
				dlmRet = DLM_ERR_FAILURE;
				break;
			}
		}

		if(false == ChangeREG_SZ(hKey, strRegKey, strRegValue))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get value for InstallDirectory at Path - [%s].\n", RegPathToOpen.c_str());
			dlmRet = DLM_ERR_FAILURE;
            RegCloseKey(hKey);
			break;
        }

        RegCloseKey(hKey);
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmRet;
}


bool DisksInfoCollector::GetDlmInfoFromReg(const std::string& strRegKey, std::string& strRegValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    bool bRet = false;

	HKEY hKey;
	do
	{
		std::string RegPathToOpen = DLM_SOFTWARE_HIVE + std::string("\\Wow6432Node\\SV Systems");

		long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);

        if( lRV != ERROR_SUCCESS )
		{
			DebugPrintf(SV_LOG_WARNING,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", RegPathToOpen.c_str(), lRV);

			RegPathToOpen = DLM_SOFTWARE_HIVE + std::string("\\SV Systems");
			lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
			if( lRV != ERROR_SUCCESS )
			{
				DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
				break;
			}
		}

		if(false == RegGetValueOfTypeSzAndMultiSz(hKey, strRegKey, strRegValue))
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to get value for InstallDirectory at Path - [%s].\n", RegPathToOpen.c_str());
            RegCloseKey(hKey);
			break;
        }

		bRet = true;

        RegCloseKey(hKey);
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRet;
}


//Writes checksum details to a file
DLM_ERROR_CODE DisksInfoCollector::DeleteDlmInfoFromReg(const std::string& strRegKey, const std::string& strRegValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DLM_ERROR_CODE dlmRet = DLM_ERR_SUCCESS;

	HKEY hKey;
	do
	{
		long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strRegKey.c_str(), 0, KEY_ALL_ACCESS, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed and return code - [%lu].\n", lRV);
			dlmRet = DLM_ERR_FAILURE;
			break;
        }

		DebugPrintf(SV_LOG_INFO, "Deleting registry Key[%s %s].\n", strRegKey.c_str(), strRegValue.c_str());

		lRV = RegDeleteValue(hKey, strRegValue.c_str());
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_WARNING,"RegDeleteValue failed for Value - %s and return code - [%lu].\n", strRegValue.c_str(), lRV);
        }
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully deleted the key %s %s \n", strRegKey.c_str(), strRegValue.c_str());
		}
        RegCloseKey(hKey);
		break;
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmRet;
}



bool DisksInfoCollector::RegGetValueOfTypeSzAndMultiSz(HKEY hKey, const std::string& KeyToFetch, std::string & Value)
{
    bool rv = true;

    do
    {
        char * cValue = 0;

        DWORD dwType, dwSize;
        long lRV;

        lRV = RegQueryValueEx(hKey, KeyToFetch.c_str(), NULL, &dwType, 0, &dwSize);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_WARNING,"RegQueryValueEx failed to fetch the type and size for Key - [%s] and return code - [%lu].\n", KeyToFetch.c_str(), lRV);
            rv = false; break;
        }

        cValue = new char [dwSize/sizeof(char)];

        lRV = RegQueryValueEx(hKey, KeyToFetch.c_str(), NULL, &dwType, (PBYTE)cValue, &dwSize);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_WARNING,"RegQueryValueEx failed to fetch the value for Key - [%s] and return code - [%lu].\n", KeyToFetch.c_str(), lRV);
            rv = false; break;
        }
        DebugPrintf(SV_LOG_INFO,"cValue = %s\n",cValue);

        Value = std::string(cValue);
        if (cValue)
        {
            delete cValue;
        }
    }while(0);

    return rv;
}


/*
description: Changes the REG_SZ type values in the registry. when ever you want to change the REG_SZ value in registry just call this function
Input: the key to the registry,the value name,the value.
Output : Successful set of the value in the given key.
*/
bool DisksInfoCollector::ChangeREG_SZ(HKEY hKey, const std::string & strName, const std::string& strTempRegValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFlag = true;
	if(ERROR_SUCCESS == RegSetValueEx(hKey, strName.c_str() , NULL, REG_SZ, (LPBYTE) strTempRegValue.c_str(), strTempRegValue.size()+1))
	{
        DebugPrintf(SV_LOG_INFO, "Successfully set  \"%s\"=\"%s\"\n",strName.c_str(), strTempRegValue.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to set value for \"%s\"=\"%s\" and return code - [%lu].\n",strName.c_str(), strTempRegValue.c_str(), GetLastError());
		bFlag = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFlag;
}


bool DisksInfoCollector::SetDlmRegPrivileges(LPCSTR privilege, bool set)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	TOKEN_PRIVILEGES tp;
    HANDLE hToken;
    LUID luid;
    bool bhTokenOpen = false;

    do
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	    {
            DebugPrintf(SV_LOG_ERROR, "OpenProcessToken failed - %lu\n", GetLastError());
		    rv = false; break;
	    }
        bhTokenOpen = true;

        if(!LookupPrivilegeValue(NULL, privilege, &luid))
        {
		    DebugPrintf(SV_LOG_ERROR, "LookupPrivilegeValue failed - %lu\n", GetLastError());
            rv = false; break;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;

        if (set)
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        else
            tp.Privileges[0].Attributes = 0;

        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        if (GetLastError() != ERROR_SUCCESS)
        {
		    DebugPrintf(SV_LOG_ERROR, "AdjustTokenPrivileges failed - %lu\n", GetLastError());
            rv = false; break;
        }
    }while(0);

    if(bhTokenOpen)
        CloseHandle(hToken);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool DisksInfoCollector::GetPartitionFileNameFromReg(std::string & strFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	do
	{
		if(false == SetDlmRegPrivileges(SE_BACKUP_NAME, true))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to set SE_BACKUP_NAME privileges.\n");
			rv = false;
			break;
		}
		if(false == SetDlmRegPrivileges(SE_RESTORE_NAME, true))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to set SE_RESTORE_NAME privileges.\n");
			rv = false;
			break;
		}

		if(GetDlmInfoFromReg(DLM_PARTITION_FILE_REG, strFileName))		//Finds out the Partition file path from registry
		{
			DebugPrintf(SV_LOG_INFO, "Found the partition file name : %s\n", strFileName.c_str());
			rv =true;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find the partition file name from registry\n");
			rv = false;
		}
		break;
	}while(0);

	SetDlmRegPrivileges(SE_BACKUP_NAME, false);
	SetDlmRegPrivileges(SE_RESTORE_NAME, false);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool DisksInfoCollector::WritePartitionFileNameInReg(const std::string& strBinaryFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	do
	{
		if(false == SetDlmRegPrivileges(SE_BACKUP_NAME, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set SE_BACKUP_NAME privileges.\n");
			rv = false;
			break;
		}
		if(false == SetDlmRegPrivileges(SE_RESTORE_NAME, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set SE_RESTORE_NAME privileges.\n");
			rv = false;
			break;
		}

		if(DLM_ERR_SUCCESS != WriteDlmInfoIntoReg(strBinaryFileName, DLM_PARTITION_FILE_REG))
			rv = false;
		break;
	}while(0);

	SetDlmRegPrivileges(SE_BACKUP_NAME, false);
	SetDlmRegPrivileges(SE_RESTORE_NAME, false);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool DisksInfoCollector::WriteDlmMbrInfoInReg(const std::string& strBinaryFileName, const std::string& Checksum)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	do
	{
		if(false == SetDlmRegPrivileges(SE_BACKUP_NAME, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set SE_BACKUP_NAME privileges.\n");
			rv = false;
			break;
		}
		if(false == SetDlmRegPrivileges(SE_RESTORE_NAME, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set SE_RESTORE_NAME privileges.\n");
			rv = false;
			break;
		}

		if(DLM_ERR_SUCCESS != WriteDlmInfoIntoReg(strBinaryFileName, DLM_MBR_FILE_REG))
		{
			rv = false;
			break;
		}
		if(DLM_ERR_SUCCESS != WriteDlmInfoIntoReg(Checksum, DLM_CHECKSUM_REG))
			rv = false;
		break;
	}while(0);

	SetDlmRegPrivileges(SE_BACKUP_NAME, false);
	SetDlmRegPrivileges(SE_RESTORE_NAME, false);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

DLM_ERROR_CODE DisksInfoCollector::OnlineOrOfflineDisk(const std::string& DiskName, const bool& bOnline)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

    std::string strDiskNumber = DiskName;
    std::string strDisk = "\\\\.\\PHYSICALDRIVE";
    size_t pos = DiskName.find(strDisk);
    if(pos != std::string::npos)
    {
        strDiskNumber = DiskName.substr(pos+strDisk.length());
    }

    DISKPART::DiskPartWrapper diskPartWrapper;
    DISKPART::diskPartDiskNumbers_t disks;
    disks.insert(strDiskNumber);
    if(bOnline)
    {
		DebugPrintf(SV_LOG_INFO,"Online Disk = %s%s\n",strDisk.c_str(), strDiskNumber.c_str());
        if(!diskPartWrapper.onlineDisks(disks))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to online disk %s: %s \n", DiskName.c_str(), diskPartWrapper.getError().c_str());
            // FIXME: for now ignore errors from DiskpartWrapper
            // dlmResult = DLM_ERR_FAILURE;
        }
    }
    else
    {
		DebugPrintf(SV_LOG_INFO,"Offline Disk = %s%s\n",strDisk.c_str(), strDiskNumber.c_str());
        if(!diskPartWrapper.offlineDisks(disks))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to offline disk %s: %s \n", DiskName.c_str(), diskPartWrapper.getError().c_str());
            // for now ignore not supported offline errors as we know xp/w2k3 can not offline. this way
            // can at least attempt the recovery
            if (!boost::algorithm::icontains(diskPartWrapper.getError(), "not supported"))
            {
                // FIXME: for now ignore errors from DiskpartWrapper
                // dlmResult = DLM_ERR_FAILURE;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}

DLM_ERROR_CODE DisksInfoCollector::ConvertEfiToNormalPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

    std::string strDiskNumber = DiskName;
    std::string strDisk = "\\\\.\\PHYSICALDRIVE";
    size_t pos = DiskName.find(strDisk);
    if(pos != std::string::npos)
    {
        strDiskNumber = DiskName.substr(pos+strDisk.length());
    }

    DebugPrintf(SV_LOG_ERROR,"Disk to Convert EFI to Normal partition = %s%s \n", strDisk.c_str(), strDiskNumber.c_str());

    DISKPART::DiskPartWrapper diskPartWrapper;
    DISKPART::DiskNumberAndPartitions diskNumberAndPartitions;

    diskNumberAndPartitions.m_diskNumber = strDiskNumber;

    DlmPartitionInfoSubIterVec_t iterVec(vecDp.begin());
    DlmPartitionInfoSubIterVec_t iterVecEnd(vecDp.end());
    for(/* empty */; iterVec != iterVecEnd; iterVec++)
    {
		if(boost::algorithm::iequals(iterVec->PartitionType, "{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}"))
        {
            diskNumberAndPartitions.m_partitionNumbers.insert(iterVec->PartitionNum);
        }
    }
    if (!diskNumberAndPartitions.m_partitionNumbers.empty()) {
        DISKPART::diskPartDiskNumberAndPartitions_t diskPartDiskNumberAndPartitions;
        diskPartDiskNumberAndPartitions.push_back(diskNumberAndPartitions);
        if (!diskPartWrapper.setPartitionId(diskPartDiskNumberAndPartitions, "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7"))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to change EFI to normal partition for the target disk = %s: %s \n", DiskName.c_str(), diskPartWrapper.getError());
            // FIXME: for now ignore errors from DiskpartWrapper
            // dlmResult = DLM_ERR_FAILURE;
        }
    }
	diskPartWrapper.rescanDisks();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}


DLM_ERROR_CODE DisksInfoCollector::ConvertNormalToEfiPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

    std::string strDiskNumber = DiskName;
    std::string strDisk = "\\\\.\\PHYSICALDRIVE";
    size_t pos = DiskName.find(strDisk);
    if(pos != std::string::npos)
    {
        strDiskNumber = DiskName.substr(pos+strDisk.length());
    }

    DebugPrintf(SV_LOG_ERROR,"Disk to Convert Normal to Efi partition = %s%s \n", strDisk.c_str(), strDiskNumber.c_str());

    DISKPART::DiskPartWrapper diskPartWrapper;
    DISKPART::DiskNumberAndPartitions diskNumberAndPartitions;

    diskNumberAndPartitions.m_diskNumber = strDiskNumber;

    DlmPartitionInfoSubIterVec_t iterVec(vecDp.begin());
    DlmPartitionInfoSubIterVec_t iterVecEnd(vecDp.end());
    for(/* empty */; iterVec != iterVecEnd; iterVec++)
    {
        if(boost::algorithm::iequals(iterVec->PartitionType, "{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}"))
        {
            diskNumberAndPartitions.m_partitionNumbers.insert(iterVec->PartitionNum);
        }
    }
    if (!diskNumberAndPartitions.m_partitionNumbers.empty()) {
        DISKPART::diskPartDiskNumberAndPartitions_t diskPartDiskNumberAndPartitions;
        diskPartDiskNumberAndPartitions.push_back(diskNumberAndPartitions);
        if (!diskPartWrapper.setPartitionId(diskPartDiskNumberAndPartitions, "c12a7328-f81f-11d2-ba4b-00a0c93ec93b"))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to change normal to EFI partition for the target disk = %s: %s \n", DiskName.c_str(), diskPartWrapper.getError());
            // FIXME: for now ignore errors from DiskpartWrapper
            // dlmResult = DLM_ERR_FAILURE;
        }
    }
	diskPartWrapper.rescanDisks() ;
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}


/*bool DisksInfoCollector::ExecuteProcess(const std::string &FullPathToExe, const std::string &Parameters)
{
	std::string strFullCommand = FullPathToExe + Parameters;
    STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	memset(&StartupInfo, 0x00, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	memset(&ProcessInformation, 0x00, sizeof(PROCESS_INFORMATION));
	if(!::CreateProcess(NULL,const_cast<char*>(strFullCommand.c_str()),NULL,NULL,FALSE,NULL,NULL, NULL, &StartupInfo, &ProcessInformation))
	{
        DebugPrintf(SV_LOG_ERROR,"CreateProcess failed with Error [%lu]\n",GetLastError());
		return false;
	}
	DWORD retValue = WaitForSingleObject( ProcessInformation.hProcess, INFINITE );
	if(retValue == WAIT_FAILED)
	{
		DebugPrintf(SV_LOG_ERROR,"WaitForSingleObject has failed.\n");
		::CloseHandle(ProcessInformation.hProcess);
		::CloseHandle(ProcessInformation.hThread);
		return false;
	}
	::CloseHandle(ProcessInformation.hProcess);
	::CloseHandle(ProcessInformation.hThread);
    return true;
}*/

bool DisksInfoCollector::ExecuteProcess(const std::string &FullPathToExe, const std::string &Parameters, std::string& OutputFile)
{

	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	std::string command = FullPathToExe + Parameters;
	//std::stringstream & results;
    FILE* pipe = _popen(command.c_str(), "r");

	std::ofstream outfile(OutputFile.c_str(), std::ios_base::out);

	if(! outfile.is_open())
	{
		DebugPrintf(SV_LOG_WARNING,"\nFailed to open %s \n",OutputFile.c_str());
	}

	do
	{
		if (0 == pipe)
		{
			outfile << "popen failed: " << errno << '\n';
			DebugPrintf(SV_LOG_ERROR,"popen Failed...For error check log file : %s\n",OutputFile.c_str());
			bRet = false;
			break;
		}

		char buffer[512];
		size_t bytesRead;
		do
		{
			bytesRead = fread(buffer, 1, sizeof(buffer), pipe);
			if (ferror(pipe))
			{
				outfile << "fread pipe failed: " << errno << '\n';
				_pclose(pipe);
				bRet = false;
				DebugPrintf(SV_LOG_ERROR,"fread pipe failed...For error check log file : %s\n",OutputFile.c_str());
				break;
			}

			outfile.write(buffer, bytesRead);

		}while(!feof(pipe));

		if(!bRet)
		{
			break;
		}
		_pclose(pipe);
	}while(0);

	outfile.close();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRet;
}


bool DisksInfoCollector::ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    DebugPrintf(SV_LOG_INFO, "Command     = %s\n", Command.c_str());
    DebugPrintf(SV_LOG_INFO, "Output File = %s\n", OutputFile.c_str());

    ACE_Process_Options options;
    options.command_line("%s",Command.c_str());

	ACE_Time_Value timeout = ACE_Time_Value::max_time;

    ACE_HANDLE handle = ACE_OS::open(OutputFile.c_str(), Modes);
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
						DebugPrintf(SV_LOG_INFO, "Command succeesfully executed having PID: %d.\n", rc);
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


// --------------------------------------------------------------------------
// Fetch the system drive (say C:\ - it will be in this format).
// --------------------------------------------------------------------------
bool DisksInfoCollector::GetSystemVolume(std::string & strSysVol)
{
    char szSysPath[MAX_PATH];
    if (0 == GetSystemDirectory(szSysPath, MAX_PATH))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch the System Directory. Error - [%lu]\n",GetLastError());
		return false;
	}

    char szSysDrive[MAX_PATH];
    if (0 == GetVolumePathName(szSysPath,szSysDrive,MAX_PATH))
    {
        DebugPrintf(SV_LOG_ERROR,"System Directory - %s\n",szSysPath);
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the System Volume from the System Directory. Error - [%lu]\n",GetLastError());
		return false;
    }
    strSysVol = std::string(szSysDrive);
    DebugPrintf(SV_LOG_INFO,"System Volume - %s\n",strSysVol.c_str());

    return true;
}

// --------------------------------------------------------------------------
// Deletes the DLM related registry entries.
// --------------------------------------------------------------------------
DLM_ERROR_CODE DisksInfoCollector::DeleteDlmRegitryEntries()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

	do
	{
		if(false == SetDlmRegPrivileges(SE_BACKUP_NAME, true))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to set SE_BACKUP_NAME privileges.\n");
			dlmResult = DLM_ERR_FAILURE;
			break;
		}
		if(false == SetDlmRegPrivileges(SE_RESTORE_NAME, true))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to set SE_RESTORE_NAME privileges.\n");
			dlmResult = DLM_ERR_FAILURE;
			break;
		}

		std::string RegPath = DLM_SOFTWARE_HIVE + std::string("\\Wow6432Node\\SV Systems");

		HKEY hKey;
		long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPath.c_str(), 0, KEY_SET_VALUE, &hKey);

		if( lRV != ERROR_SUCCESS )
		{
			DebugPrintf(SV_LOG_WARNING,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", RegPath.c_str(), lRV);

			RegPath = DLM_SOFTWARE_HIVE + std::string("\\SV Systems");
			lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPath.c_str(), 0, KEY_SET_VALUE, &hKey);
			if( lRV != ERROR_SUCCESS )
			{
				DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPath.c_str(), lRV);
				break;
			}
		}
		RegCloseKey(hKey);

		if(DLM_ERR_SUCCESS != DeleteDlmInfoFromReg(RegPath, DLM_MBR_FILE_REG))
			dlmResult = DLM_ERR_FAILURE;

		if(DLM_ERR_SUCCESS != DeleteDlmInfoFromReg(RegPath, DLM_CHECKSUM_REG))
			dlmResult = DLM_ERR_FAILURE;

		if(DLM_ERR_SUCCESS != DeleteDlmInfoFromReg(RegPath, DLM_PARTITION_FILE_REG))
			dlmResult = DLM_ERR_FAILURE;

		break;
	}while(0);

	SetDlmRegPrivileges(SE_BACKUP_NAME, false);
	SetDlmRegPrivileges(SE_RESTORE_NAME, false);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return dlmResult;
}

DLM_ERROR_CODE DisksInfoCollector::PostDynamicDiskRestoreOperns(std::vector<std::string>& RestoredDisk, bool restartVds)
{
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;
    bool bImport = false;
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DISKPART::DiskPartWrapper diskPartWrapper;
    diskPartWrapper.rescanDisks();
    if(restartVds)
    {
        if(S_FALSE == restartService("vds"))
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to re-start the \"vds\" service \n");
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"Successfully re-started the \"vds\" service \n");
        boost::this_thread::sleep(boost::posix_time::milliseconds(2000));//This is required for updating the disk managemnt
        }
    }
    DebugPrintf(SV_LOG_INFO,"Collecting offline disks information\n");
	DISKPART::diskPartDiskNumbers_t::iterator diskiter;
    DISKPART::diskPartDiskNumbers_t disks = diskPartWrapper.getDisks(DISKPART::DISK_STATUS_OFFLINE, DISKPART::DISK_TYPE_DYNAMIC);
    if(disks.empty() && !diskPartWrapper.getError().empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get offline dynamic disks: %s\n", diskPartWrapper.getError().c_str());
        // FIXME: for now ignore errors from DiskpartWrapper
        // dlmResult = DLM_ERR_FAILURE;
    }
    else
    {
		DebugPrintf(SV_LOG_INFO,"The offline disks are : \n");
		for(diskiter = disks.begin(); diskiter != disks.end(); diskiter++)
		{
			DebugPrintf(SV_LOG_INFO, "\t%s\n", diskiter->c_str());
		}
        if (!diskPartWrapper.onlineDisks(disks))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to online all dynamic disks: %s\n", diskPartWrapper.getError().c_str());
            // FIXME: for now ignore errors from DiskpartWrapper
            // dlmResult = DLM_ERR_FAILURE;
        }
    }
    DebugPrintf(SV_LOG_INFO,"Collecting Foreign disks information\n");
    disks = diskPartWrapper.getDisks(DISKPART::DISK_STATUS_FOREIGN, DISKPART::DISK_TYPE_DYNAMIC);
    if(disks.empty() && !diskPartWrapper.getError().empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get foreign  dynamic disks: %s\n", diskPartWrapper.getError().c_str());
        // FIXME: for now ignore errors from DiskpartWrapper
        // dlmResult = DLM_ERR_FAILURE;
    }
    else
    {
		DebugPrintf(SV_LOG_INFO,"The Foreign disks are : \n");
		for(diskiter = disks.begin(); diskiter != disks.end(); diskiter++)
		{
			DebugPrintf(SV_LOG_INFO, "\t%s\n", diskiter->c_str());
		}
        if(diskPartWrapper.importForeignDisks(disks))
        {
            DebugPrintf(SV_LOG_INFO,"Successfully imported the foreign disks\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to import the foreign disks: %s\n", diskPartWrapper.getError().c_str());
            // FIXME: for now ignore errors from DiskpartWrapper
            // dlmResult = DLM_ERR_FAILURE;
        }
    }
    if(!diskPartWrapper.recoverDisks())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to Recover dynamic disks: %s\n", diskPartWrapper.getError().c_str());
        DebugPrintf(SV_LOG_ERROR,"Manually recover might be required for dynamic disks after reboot\n");
        // FIXME: for now ignore errors from DiskpartWrapper
        // dlmResult = DLM_ERR_FAILURE;
    }
    // need to get disks that have status Mssing as well as check for any disks not
    // listed that are RestoreDisks in case they completely disappered.
    std::string msg("The follwoing disks are \"missing\"\n");
    std::vector<std::string> missingDisks;
    disks = diskPartWrapper.getDisks(DISKPART::DISK_STATUS_MISSING, DISKPART::DISK_TYPE_DYNAMIC);
    if (!disks.empty())
    {
		DebugPrintf(SV_LOG_INFO,"The missing disks are : \n");
		for(diskiter = disks.begin(); diskiter != disks.end(); diskiter++)
		{
			DebugPrintf(SV_LOG_INFO, "\t%s\n", diskiter->c_str());
		}

        DISKPART::diskPartDiskNumbers_t::iterator iter(disks.begin());
        DISKPART::diskPartDiskNumbers_t::iterator iterEnd(disks.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            msg += "  Disk ";
            msg += *iter;
            msg += "\n";
            missingDisks.push_back(*iter);
        }
    }
    disks = diskPartWrapper.getDisks();
    DISKPART::diskPartDiskNumbers_t::iterator disksIterEnd(disks.end());
    std::vector<std::string>::iterator restoredDiskIter(RestoredDisk.begin());
    std::vector<std::string>::iterator restoredDiskIterEnd(RestoredDisk.end());
    for(/* empty */; restoredDiskIter != restoredDiskIterEnd; ++restoredDiskIter)
    {
        DISKPART::diskPartDiskNumbers_t::iterator findIter(disks.find(*restoredDiskIter));
        if(disksIterEnd != findIter)
        {
            msg += "  Disk ";
            msg += *restoredDiskIter;
            msg += "\n";
            missingDisks.push_back(*restoredDiskIter);
        }
    }
    if(!missingDisks.empty())
    {
        msg += "reboot system in rescue cd and try again\n";
        DebugPrintf(SV_LOG_ERROR, msg.c_str());
        RestoredDisk = missingDisks;
        dlmResult = DLM_ERR_DISK_DISAPPEARED;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}

bool DisksInfoCollector::RescanDiskMgmt()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRet = true;
    DISKPART::DiskPartWrapper diskPartWrapper;
    bRet = diskPartWrapper.rescanDisks();
    if(!bRet)
    {
        DebugPrintf(SV_LOG_ERROR,"\n Failed to rescan disks: %s\n", diskPartWrapper.getError().c_str());
        // FIXME: for now ignore errors from DiskpartWrapper
        bRet = true; // FIXME: remove once honring DiskPartWrapper errors
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRet;
}


bool DisksInfoCollector::ImportDynDisks(std::list<std::string> & lstFrnDiskNums, bool & bImport)
{
    bool bRet = true;
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DISKPART::DiskPartWrapper diskPartWrapper;
    bRet = diskPartWrapper.importForeignDisks(diskPartWrapper.convertDiskNumberListToSet(lstFrnDiskNums));
    if(!bRet)
    {
        DebugPrintf(SV_LOG_ERROR,"import disks failed: %s\n", diskPartWrapper.getError());
        // FIXME: for now ignore errors from DiskpartWrapper
        bRet = true; // FIXME: remover once honoring DiskPartWrapper errors
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


bool DisksInfoCollector::RecoverDynDisks(std::list<std::string> & listDiskNum)
{
    bool bRet = true;
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DISKPART::DiskPartWrapper diskPartWrapper;
    bRet = diskPartWrapper.recoverDisks(diskPartWrapper.convertDiskNumberListToSet(listDiskNum));
    if(!bRet)
    {
        DebugPrintf(SV_LOG_ERROR,"import disks failed: %s\n", diskPartWrapper.getError());
        // FIXME: for now ignore errors from DiskpartWrapper
        bRet = true; // FIXME: remove once honring DiskPartWrapper errors
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRet;
}

HRESULT DisksInfoCollector::restartService(const std::string& serviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    SC_HANDLE schService;
    SERVICE_STATUS_PROCESS  serviceStatusProcess;
    SERVICE_STATUS serviceStatus;
    DWORD actualSize = 0;
    ULONG dwRet = 0;
    LPVOID lpMsgBuf;
    SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager.\n");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;
    }

    // stop requested service
    // PR#10815: Long Path support
    schService = OpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS);
    if(NULL != schService)
    {
        if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == serviceStatusProcess.dwCurrentState)
        {
            DebugPrintf(SV_LOG_INFO,"Stopping %s service.\n",serviceName.c_str());
            // stop service
            if (::ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus))
            {
                // wait for it to actually stop
                int retry = 1;
                do
                {
                    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
                } while (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState && retry++ <= 600/*180 */);//some times service was taking too much time to stop..So increased this value to 10 mins as suggested by Jayesh
                if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState)
                {
                    dwRet = GetLastError();
                    DebugPrintf(SV_LOG_ERROR,"Failed to stop the service: %s with Error Code : [%lu].\n",serviceName.c_str(),GetLastError());
                    ::CloseServiceHandle(schService);
                    goto Error_return_fail;
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"The service  %s is currently not running.\n",serviceName.c_str());
        }

        // start the service
        DebugPrintf(SV_LOG_INFO,"Starting %s service.\n",serviceName.c_str());
        if (::StartService(schService, 0, NULL) == 0)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to start the service: %s with Error Code : [%lu].\n",serviceName.c_str(),GetLastError());
            dwRet = GetLastError();
            ::CloseServiceHandle(schService);
            goto Error_return_fail;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"Successfully started the service  %s \n",serviceName.c_str());
        }
        ::CloseServiceHandle(schService);
    }
    else
    {
        dwRet = GetLastError();
        goto Error_return_fail;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;

  Error_return_fail:
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
        DebugPrintf(SV_LOG_ERROR,"Failed to restart %s service Error Code : [%lu].\n",serviceName.c_str(), dwRet);
        DebugPrintf(SV_LOG_ERROR,"Error %lu = %s",dwRet,(LPCTSTR)lpMsgBuf);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    }

    // Free the buffer
    LocalFree(lpMsgBuf);
    return S_FALSE;
}

bool DisksInfoCollector::GetSizeOfVolume(const std::string& strVolGuid, SV_ULONGLONG& volSize)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	HANDLE	hVolume ;

	// Remove the trailing "\" from volume GUID
    std::string strTemp = strVolGuid;
    std::string::size_type pos = strTemp.find_last_of("\\");
    if(pos != std::string::npos)
    {
        strTemp.erase(pos);
    }

    hVolume = INVALID_HANDLE_VALUE ;
	hVolume = CreateFile(strTemp.c_str(),GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT  , NULL
                         );
	if( hVolume == INVALID_HANDLE_VALUE )
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open handle for volume-%s with Error Code - %d \n", strTemp.c_str(), GetLastError());
		bResult = false;
	}
	else
	{
		ULONG	bytesWritten;
		UCHAR	DiskExtentsBuffer[0x400];
		DWORD   returnBufferSize = sizeof(DiskExtentsBuffer);
		PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
		if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
							NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer),
							&bytesWritten, NULL) )
		{
			for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ )
			{
				volSize    = (SV_ULONGLONG)DiskExtents->Extents[extent].ExtentLength.QuadPart;
				DebugPrintf(SV_LOG_INFO,"VolumeLength    = %lld\n",volSize);
			}
		}
		else
		{
			DWORD error = GetLastError();
			DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed for volume-%s with Error code - %lu.\n", strTemp.c_str(), error);
			if(error == ERROR_MORE_DATA)
			{
                DWORD nde;
                INM_SAFE_ARITHMETIC(nde = InmSafeInt<DWORD>::Type(DiskExtents->NumberOfDiskExtents) - 1, INMAGE_EX(DiskExtents->NumberOfDiskExtents))
				INM_SAFE_ARITHMETIC(returnBufferSize = sizeof(VOLUME_DISK_EXTENTS) + (nde * InmSafeInt<size_t>::Type(sizeof(DISK_EXTENT))), INMAGE_EX(sizeof(VOLUME_DISK_EXTENTS))(nde)(sizeof(DISK_EXTENT)))
				DiskExtents = (PVOLUME_DISK_EXTENTS) new UCHAR[returnBufferSize];

				if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
							NULL, 0, DiskExtents, returnBufferSize,
							&bytesWritten, NULL) )
				{

					for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ )
					{
						volSize    = (SV_ULONGLONG)DiskExtents->Extents[extent].ExtentLength.QuadPart;
						DebugPrintf(SV_LOG_INFO,"VolumeLength    = %lld\n",volSize);
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed for Volume: %s with Error code : [%lu].\n",strVolGuid.c_str(), GetLastError());
					bResult = false;

				}
				delete [] DiskExtents ;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Failed to get the volume size\n");
				bResult = false;
			}
		}
		CloseHandle(hVolume);
		hVolume = INVALID_HANDLE_VALUE ;
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool DisksInfoCollector::MountEfiVolume(const std::string& strVolGuid, std::string& strMountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	//If volume is not mounted, then create a mount directory and mount it
	if(!IsVolumeNotMounted(strVolGuid, strMountPoint))
	{
		DebugPrintf(SV_LOG_INFO,"Found the volume as unmounted : %s \n",strVolGuid.c_str());

		std::set<std::string> lstDrives;

		ListAssignedDrives(lstDrives);	  //Lists out the currently asigned drive letters

		if(false == MountW2K8EfiVolume(strMountPoint, strVolGuid, lstDrives))
		{
			strMountPoint = "z:\\";  //trying to mount with default drive letter "z"
			//mount volume
			if (false == SetVolumeMountPoint(strMountPoint.c_str(),strVolGuid.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to mount the Volume: %s Error Code: %d \n", strMountPoint.c_str(), GetLastError());
				bRet = false;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully mounted the Volume to : %s\n",strMountPoint.c_str());
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}


//Check whether the volume is mounted or not
bool DisksInfoCollector::IsVolumeNotMounted(const std::string& strVolumeGuid , std::string & strMountPoint)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;
	char szVolumePath[MAX_PATH+1] = {0};
	DWORD dwVolPathSize = 0;

#ifndef SCOUT_WIN2K_SUPPORT  //hiding the GetVolumePathNamesForVolumeName for Win2K
	if (GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
	{
		if(!std::string(szVolumePath).empty()) //if volume path name is empty, then it is not mounted.
		{
			strMountPoint = std::string(szVolumePath);
			DebugPrintf(SV_LOG_INFO,"Found mount point : %s\n",strMountPoint.c_str());
		}
		else
		{
			ret = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : %d \n",GetLastError());
		ret = false;
	}
#else
	//Need to write new code to get the volume names. Fornow skipping it.
	DebugPrintf(SV_LOG_INFO,"WiN2K macro is enabled, skipping GetVolumePathNamesForVolumeName\n");
	DebugPrintf(SV_LOG_INFO,"Volumepath not found for volume GUID : %s\n", strVolumeGuid.c_str());
	ret = false;
#endif

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


void DisksInfoCollector::RemoveAndDeleteMountPoints(const std::string & strEfiMountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	if(false == DeleteVolumeMountPoint(strEfiMountPoint.c_str()))
    {
        DebugPrintf(SV_LOG_WARNING,"Failed to unmount the volume with above mount point with Error : [%lu].\n",GetLastError());
        DebugPrintf(SV_LOG_INFO,"Please unmount the volume and delete the mount point manually.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Unmounted the volume. Deleting the mount point now.\n");
        if(false == RemoveDirectory(strEfiMountPoint.c_str()))
            DebugPrintf(SV_LOG_WARNING,"Failed to delete the mount point with Error : [%lu].\n",GetLastError());
        else
            DebugPrintf(SV_LOG_INFO,"Deleted the mount point Successfully.\n");

    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


DLM_ERROR_CODE DisksInfoCollector::CleanDisk(const std::string& DiskName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

    std::string strDiskNumber = DiskName;
    std::string strDisk = "\\\\.\\PHYSICALDRIVE";
    size_t pos = DiskName.find(strDisk);
    if(pos != std::string::npos)
    {
        strDiskNumber = DiskName.substr(pos+strDisk.length());
    }

    DebugPrintf(SV_LOG_INFO,"Clean disk = %s%s \n", strDisk.c_str(), strDiskNumber.c_str());

    DISKPART::DiskPartWrapper diskPartWrapper;
    DISKPART::diskPartDiskNumbers_t disks;
    disks.insert(strDiskNumber);
    if(!diskPartWrapper.cleanDisks(disks))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to clean disk %s: %s \n", DiskName.c_str(), diskPartWrapper.getError().c_str());
        // FIXME: for now ignore errors from DiskpartWrapper
        // dlmResult = DLM_ERR_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}


DLM_ERROR_CODE DisksInfoCollector::ImportDisks(std::set<std::string>& DiskNames)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

	DISKPART::DiskPartWrapper diskPartWrapper;
    DISKPART::diskPartDiskNumbers_t disks;    

    std::string strDiskNumber;
    std::string strDisk = "\\\\.\\PHYSICALDRIVE";

	std::set<std::string>::iterator iter = DiskNames.begin();
	for(; iter != DiskNames.end(); iter++)
	{
		size_t pos = iter->find(strDisk);
		if(pos != std::string::npos)
		{
			strDiskNumber = iter->substr(pos+strDisk.length());
			DebugPrintf(SV_LOG_INFO,"Import disk = %s%s \n", strDisk.c_str(), strDiskNumber.c_str());
			disks.insert(strDiskNumber);
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Import disk = %s%s \n", strDisk.c_str(), iter->c_str());
			disks.insert(*iter);
		}
	}

    if(!diskPartWrapper.importForeignDisks(disks))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to import the disks : %s \n", diskPartWrapper.getError().c_str());
        dlmResult = DLM_ERR_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}


DLM_ERROR_CODE DisksInfoCollector::AutoMount(bool bFlag)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

	DISKPART::DiskPartWrapper diskPartWrapper;

	if(!diskPartWrapper.automount(bFlag))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to enabole/disable the automount : %s \n", diskPartWrapper.getError().c_str());
        dlmResult = DLM_ERR_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}


DLM_ERROR_CODE DisksInfoCollector::ClearAttributeOfDisk(const std::string& DiskName, DISK_TYPE_ATTRIBUTE DiskAttribute)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

	std::string strDiskNumber = DiskName;
    std::string strDisk = "\\\\.\\PHYSICALDRIVE";
    size_t pos = DiskName.find(strDisk);
    if(pos != std::string::npos)
    {
        strDiskNumber = DiskName.substr(pos+strDisk.length());
    }

    DebugPrintf(SV_LOG_INFO,"Clear disk = %s%s \n", strDisk.c_str(), strDiskNumber.c_str());

    DISKPART::DiskPartWrapper diskPartWrapper;
    DISKPART::diskPartDiskNumbers_t disks;
    disks.insert(strDiskNumber);
	DISKPART::DISK_ATTRIBUTE diskattr;
	if(DISK_READ_ONLY == DiskAttribute)
		diskattr = DISKPART::DISK_READ_ONLY;
	if(!diskPartWrapper.clearDisksAttribute(disks, diskattr))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to clean disk %s: %s \n", DiskName.c_str(), diskPartWrapper.getError().c_str());
        dlmResult = DLM_ERR_FAILURE;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}

DLM_ERROR_CODE DisksInfoCollector::InitializeDisk(std::string DiskName, DISK_FORMAT_TYPE FormatType, DISK_TYPE DiskType)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

	do
	{
		CREATE_DISK dsk;
		DWORD nDiskSign;
		srand(time(NULL));
		dsk.PartitionStyle = PARTITION_STYLE_MBR;
		nDiskSign = rand() % 100000000 + 10000001;
		dsk.Mbr.Signature = nDiskSign;
		DebugPrintf(SV_LOG_INFO,"Generated MBR disk signature : %lu \n", dsk.Mbr.Signature);
		
		//else
		//{
		//	//Need to write code for GPT for generating the unique GUID
		//	dsk.PartitionStyle = PARTITION_STYLE_GPT;
		//}

		std::string strDiskNumber = DiskName;
		std::string strDisk = "\\\\.\\PHYSICALDRIVE";
		size_t pos = DiskName.find(strDisk);

		if(pos == std::string::npos)
			DiskName = strDisk + DiskName;
		else
			strDiskNumber = DiskName.substr(pos+strDisk.length());


		DebugPrintf(SV_LOG_INFO,"Drive to initialize is %s.\n",DiskName.c_str());

		HANDLE diskHandle = CreateFile(DiskName.c_str(),GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);	
		if(diskHandle == INVALID_HANDLE_VALUE)
		{
			DebugPrintf(SV_LOG_ERROR,"Error in opening device. Error Code : [%lu].\n",GetLastError());
			dlmResult = DLM_ERR_FAILURE; break;
		}			

		DWORD lpBytesReturned;
		bool flag = DeviceIoControl((HANDLE) diskHandle, IOCTL_DISK_CREATE_DISK, &dsk, sizeof(dsk), NULL, 0, (LPDWORD)&lpBytesReturned, NULL); 
		if (!flag)
		{ 
			DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_CREATE_DISK Failed with Error Code : [%lu].\n",GetLastError());
			CloseHandle(diskHandle);
			dlmResult = DLM_ERR_FAILURE; break;
		}
			
		flag = DeviceIoControl((HANDLE) diskHandle, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, (LPDWORD)&lpBytesReturned, (LPOVERLAPPED)NULL);
		if(!flag)
		{
			DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_UPDATE_PROPERTIES Failed with Error Code : [%lu].\n",GetLastError());
			CloseHandle(diskHandle);
			dlmResult = DLM_ERR_FAILURE; break;
		}
		CloseHandle(diskHandle);

		if(GPT == FormatType)
		{
			DISKPART::DiskPartWrapper diskPartWrapper;
			DISKPART::diskPartDiskNumbers_t disks;
			disks.insert(strDiskNumber);

			DISKPART::DISK_TYPE dt = DISKPART::DISK_TYPE_ANY;
			if(DYNAMIC == DiskType)
				dt = DISKPART::DISK_TYPE_DYNAMIC;

			if(!diskPartWrapper.convertDisks(disks, dt, DISKPART::DISK_FORMAT_TYPE_GPT))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to convert the disk %s: %s \n", DiskName.c_str(), diskPartWrapper.getError().c_str());
				dlmResult = DLM_ERR_FAILURE;
			}
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return dlmResult;
}


bool DisksInfoCollector::ListAssignedDrives(std::set<std::string>& lstDrives)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	// Buffer for drive string storage
	char szBuffer[1024];
	DWORD res = GetLogicalDriveStrings(1024, (LPTSTR)szBuffer);

	if(res != 0)

	{
		DebugPrintf(SV_LOG_DEBUG, "The logical drives of this machine are:\n");
		// Check up to 100 drives...
		char *pch = szBuffer;
		while(*pch)
		{
			std::string temp(pch);
			DebugPrintf(SV_LOG_DEBUG, "%s,\t", temp.c_str());
			lstDrives.insert(temp);
			pch = &pch[strlen(pch) + 1];
		}
		DebugPrintf(SV_LOG_DEBUG, "\n");
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "GetLogicalDriveStrings() is failed with Error code: %d\n", GetLastError());
		ret = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}

bool DisksInfoCollector::FindOutFreeDrive(std::string& strFreeDrive, std::set<std::string>& lstAssignedDrives)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	if(26 == lstAssignedDrives.size())
	{
		DebugPrintf(SV_LOG_ERROR,"There are no more free drive letters available\n",__FUNCTION__);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if(lstAssignedDrives.find("A:\\") == lstAssignedDrives.end())
		strFreeDrive = "A:\\";
	else if(lstAssignedDrives.find("B:\\") == lstAssignedDrives.end())
		strFreeDrive = "B:\\";
	else if(lstAssignedDrives.find("C:\\") == lstAssignedDrives.end())
		strFreeDrive = "C:\\";
	else if(lstAssignedDrives.find("D:\\") == lstAssignedDrives.end())
		strFreeDrive = "D:\\";
	else if(lstAssignedDrives.find("E:\\") == lstAssignedDrives.end())
		strFreeDrive = "E:\\";
	else if(lstAssignedDrives.find("F:\\") == lstAssignedDrives.end())
		strFreeDrive = "F:\\";
	else if(lstAssignedDrives.find("G:\\") == lstAssignedDrives.end())
		strFreeDrive = "G:\\";
	else if(lstAssignedDrives.find("H:\\") == lstAssignedDrives.end())
		strFreeDrive = "H:\\";
	else if(lstAssignedDrives.find("I:\\") == lstAssignedDrives.end())
		strFreeDrive = "I:\\";
	else if(lstAssignedDrives.find("J:\\") == lstAssignedDrives.end())
		strFreeDrive = "J:\\";
	else if(lstAssignedDrives.find("K:\\") == lstAssignedDrives.end())
		strFreeDrive = "K:\\";
	else if(lstAssignedDrives.find("L:\\") == lstAssignedDrives.end())
		strFreeDrive = "L:\\";
	else if(lstAssignedDrives.find("M:\\") == lstAssignedDrives.end())
		strFreeDrive = "M:\\";
	else if(lstAssignedDrives.find("N:\\") == lstAssignedDrives.end())
		strFreeDrive = "N:\\";
	else if(lstAssignedDrives.find("O:\\") == lstAssignedDrives.end())
		strFreeDrive = "O:\\";
	else if(lstAssignedDrives.find("P:\\") == lstAssignedDrives.end())
		strFreeDrive = "P:\\";
	else if(lstAssignedDrives.find("Q:\\") == lstAssignedDrives.end())
		strFreeDrive = "Q:\\";
	else if(lstAssignedDrives.find("R:\\") == lstAssignedDrives.end())
		strFreeDrive = "R:\\";
	else if(lstAssignedDrives.find("S:\\") == lstAssignedDrives.end())
		strFreeDrive = "S:\\";
	else if(lstAssignedDrives.find("T:\\") == lstAssignedDrives.end())
		strFreeDrive = "T:\\";
	else if(lstAssignedDrives.find("U:\\") == lstAssignedDrives.end())
		strFreeDrive = "U:\\";
	else if(lstAssignedDrives.find("V:\\") == lstAssignedDrives.end())
		strFreeDrive = "V:\\";
	else if(lstAssignedDrives.find("W:\\") == lstAssignedDrives.end())
		strFreeDrive = "W:\\";
	else if(lstAssignedDrives.find("X:\\") == lstAssignedDrives.end())
		strFreeDrive = "X:\\";
	else if(lstAssignedDrives.find("Y:\\") == lstAssignedDrives.end())
		strFreeDrive = "Y:\\";
	else if(lstAssignedDrives.find("Z:\\") == lstAssignedDrives.end())
		strFreeDrive = "Z:\\";
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find out the free slot drive letter %s\n",__FUNCTION__);
		ret = false;
	}

	if(ret)
		DebugPrintf(SV_LOG_INFO,"Free slot drive letter %s\n",strFreeDrive.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}

bool DisksInfoCollector::MountW2K8EfiVolume(std::string& strMountPoint, const std::string& strVolGuid, std::set<std::string>& lstAssignedDrives)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	ret = FindOutFreeDrive(strMountPoint, lstAssignedDrives);

	if(ret)
	{
		//mount volume
		if (FALSE == SetVolumeMountPoint(strMountPoint.c_str(),strVolGuid.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to mount the Volume: %s Error Code: %d \n", strMountPoint.c_str(), GetLastError());
			lstAssignedDrives.insert(strMountPoint);
			strMountPoint.clear();
			MountW2K8EfiVolume(strMountPoint, strVolGuid, lstAssignedDrives);
			ret = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully mounted the Volume: %s\n",strMountPoint.c_str());
		}
	}
	else
		DebugPrintf(SV_LOG_ERROR,"Failed to mount the Volume: %s\n", strVolGuid.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}

DLM_ERROR_CODE DisksInfoCollector::DiskCheckAndRectify()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DISKPART::DiskPartWrapper diskPartWrapper;
    diskPartWrapper.rescanDisks();

	DISKPART::diskPartDiskNumbers_t::iterator diskiter;
    DISKPART::diskPartDiskNumbers_t disks = diskPartWrapper.getDisks(DISKPART::DISK_STATUS_OFFLINE, DISKPART::DISK_TYPE_DYNAMIC);
    if(disks.empty() && !diskPartWrapper.getError().empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get offline dynamic disks: %s\n", diskPartWrapper.getError().c_str());
    }
    else
    {
		DebugPrintf(SV_LOG_INFO,"The offline disks are : \n");
		for(diskiter = disks.begin(); diskiter != disks.end(); diskiter++)
		{
			DebugPrintf(SV_LOG_INFO, "\t%s\n", diskiter->c_str());
		}
        if (!diskPartWrapper.onlineDisks(disks))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to online all dynamic disks: %s\n", diskPartWrapper.getError().c_str());
        }
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully onlined the disks\n");
			boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
		}
    }
    DebugPrintf(SV_LOG_INFO,"Collecting Foreign disks information\n");
    disks = diskPartWrapper.getDisks(DISKPART::DISK_STATUS_FOREIGN, DISKPART::DISK_TYPE_DYNAMIC);
    if(disks.empty() && !diskPartWrapper.getError().empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get foreign  dynamic disks: %s\n", diskPartWrapper.getError().c_str());
    }
    else
    {
		DebugPrintf(SV_LOG_INFO,"The Foreign disks are : \n");
		for(diskiter = disks.begin(); diskiter != disks.end(); diskiter++)
		{
			DebugPrintf(SV_LOG_INFO, "\t%s\n", diskiter->c_str());
		}
        if(diskPartWrapper.importForeignDisks(disks))
        {
            DebugPrintf(SV_LOG_INFO,"Successfully imported the foreign disks\n");
			boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to import the foreign disks: %s\n", diskPartWrapper.getError().c_str());
        }
    }
    if(!diskPartWrapper.recoverDisks())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to Recover dynamic disks: %s\n", diskPartWrapper.getError().c_str());
    }
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Successfully recovered dynamic disks: %s\n", diskPartWrapper.getError().c_str());
		boost::this_thread::sleep(boost::posix_time::milliseconds(5000));
	}

	DebugPrintf(SV_LOG_INFO,"Onlineing if any offline volumes available\n");
	if(!diskPartWrapper.onlineVolumes())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to online offline volumes: %s\n", diskPartWrapper.getError().c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Successfully onlined the volumes: %s\n", diskPartWrapper.getError().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

#endif
