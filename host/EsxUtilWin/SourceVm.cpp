#include "SourceVm.h"
#include "Common.h"
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <algorithm>
#include <vector>
#include <utility>

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

bool Source::IsPartitionDynamic(PARTITION_INFORMATION_EX ptrPartition)
{	
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	bool ret = false;
	if (ptrPartition.PartitionStyle == PARTITION_STYLE_MBR)
	{
		ret = ptrPartition.Mbr.PartitionType == PARTITION_LDM;
	}
	else if (ptrPartition.PartitionStyle == PARTITION_STYLE_GPT)
	{
		ret = IsEqualGUID(ptrPartition.Gpt.PartitionType, PARTITION_LDM_METADATA_GUID);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}
bool Source::removeDiskBinFile()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturn = true;
	string strCurrentHost = stGetHostName();
	do
	{
		if(strCurrentHost.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Found Empty HostName.Cannot Proceed further.Exiting...\n");
			bReturn = false;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"HOST NAME = %s.\n\n",strCurrentHost.c_str());
		string strDiskInfoFile = m_strDataFolderPath + strCurrentHost + string("_")+ string("disk.bin");
        if(checkIfFileExists(strDiskInfoFile))
        {	
            DebugPrintf(SV_LOG_INFO,"Found the file %s\n",strDiskInfoFile.c_str());
            if(0 == DeleteFile(strDiskInfoFile.c_str()))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to delete file : %s with Error Code: [%lu]\n",strDiskInfoFile.c_str(),GetLastError());
                bReturn = false;
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"Deleted stale file %s successfully.\n",strDiskInfoFile.c_str());
            }
        }
	}while(0);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturn;
}
int Source::initSource()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;
	do
	{
		if(0 != getAgentInstallPath())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find the Agent Install Path\n");
			iRetValue = 1;
			break;
		}
        if(m_bDeleteFiles)
        {
            if(false == DeleteAllFilesInFolder(m_strDataFolderPath))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to delete all files and folders in %s\n", m_strDataFolderPath.c_str());
                DebugPrintf(SV_LOG_ERROR,"Please manuall delete all files and folders and rerun the job.\n");
			    iRetValue = 1;
			    break;
            }
        }
		if(false == removeDiskBinFile())
		{
			DebugPrintf(SV_LOG_ERROR,"\n Failed to delete disk.bin file.Please remove it manually and then try once again.\n");
			iRetValue = 1;
			break;
		}
		if(FALSE == MountAllUnmountedVolumes())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::MountAllUnmountedVolumes Failed\n");
			iRetValue = 1;
			break;
		}
 		if(S_FALSE == InitCOM())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::InitCom Failed\n");
			iRetValue = 1;
			break;
		}
        
        for (int i=0; i<3; i++)
        {
		    if(S_FALSE == WMIQuery())
		    {
			    DebugPrintf(SV_LOG_ERROR,"Source::WMIQuery Failed\n");
                m_mapOfDeviceIdAndControllerIds.empty();
		    }
            else
            {           
                if(! m_mapOfDeviceIdAndControllerIds.empty())
                    break;
            }
            
            if(i != 3)
            {
                DebugPrintf(SV_LOG_ERROR,"WMI query did not return disks present in the system...Trying again.\n\n");
                Sleep(10000);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"\nWMI query did not return disks present in the system in all attempts.\n");
            }
        }
        if(m_mapOfDeviceIdAndControllerIds.empty())
        {
            iRetValue = 1;
            break;
        }

        DebugPrintf(SV_LOG_INFO,"\nDiscovered all the disks present in the host successfully.\n\n");

		if(0!= fetchSourceVmInfo())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::fetchSourceVmInfo Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"\nSaved the required information of the host successfully.\n\n");

        if(false == EnableFileModeForSysVol())
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to enable the file mode for system volume.\n\n");
			break;
        }
		if(false == RegisterHostUsingCdpCli())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Register the Master Target disks using cdpcdli\n");
 			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}while(0);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}


int Source::fetchSourceVmInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;
	do
	{
		if(false == persistDiskCountInConfFile(m_mapOfDeviceIdAndControllerIds.size()))
		{
			DebugPrintf(SV_LOG_ERROR,"Source::persistDiskCountInConfFile Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the number of disks present in the host successfully.\n\n");

        if(false == PersistDiskAndScsiIDs())
        {
            DebugPrintf(SV_LOG_ERROR,"Source::PersistDeviceIDsAndScsiIDs Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the SCSI IDs and drive numbers of the disks in the host successfully.\n\n");

		int i = 0;
		for(; i < 3; i++)
		{
			if(FALSE == ListVolumesPresentInSystem())
			{
				DebugPrintf(SV_LOG_ERROR,"Source::ListVolumesPresentInSystem Failed...Trying again.\n");
				Sleep(10000);
				continue;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Discovered all the volumes present in the host successfully.\n\n");
			}

			if(false == CheckMntPathsExistInRecycleBin())
			{
				DebugPrintf(SV_LOG_ERROR,"Source::CheckMntPathsExistInRecycleBin Failed...Trying again.\n");
				Sleep(10000);
				continue;
			}        

			if(false == DumpVolumesOrMntPtsExtentOnDisk())
			{
				DebugPrintf(SV_LOG_ERROR,"Source::DumpVolumesOrMntPtsExtentOnDisk Failed...Trying again.\n");
				Sleep(10000);
				continue;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Collected the information of all the volumes present in the host successfully.\n\n");
			}

			if(i < 3)
            {
                DebugPrintf(SV_LOG_INFO,"Successfully retrived the required Volume information..Coming out of the loop\n\n");
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"\nFailed to retrive the Volume information even after trying 3 times.\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return 1;
            }
		}
		if(i == 3)
		{
			DebugPrintf(SV_LOG_ERROR,"\nFailed to retrive the Volume information even after trying 3 times.\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return 1;
		}
        
		if(0!= ReadPhysicalDrives())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::ReadPhysicalDrives Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Collected the information of all the disks present in the host successfully.\n\n");
		
        if(S_FALSE == mapLogicalVolmuesToDiskOffset())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::mapLogicalVolmuesToDiskOffset Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the information of all the disks and its partitions and volumes present in the host successfully.\n\n");
		
        if(S_FALSE == PersistIPAddressOfHostGuestOS())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::PersistIPAddressOfHostGuestOS() Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the IP Address of the host successfully.\n\n");

		if(S_FALSE == PersistSystemVolInfo())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::PersistSystemVolInfo() Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the System Volume Info of the host successfully.\n\n");
		
        if(S_FALSE == PersistNetworkInterfaceCard())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::PersistSystemVolInfo() Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the MAC Address of the Network Interface Card present in the host successfully.\n\n");
		
        if(false == persistVolumeCount())
		{
			DebugPrintf(SV_LOG_ERROR,"Source::persistVolumeCount() Failed\n");
			iRetValue = 1;
			break;
		}
        DebugPrintf(SV_LOG_INFO,"Saved the number of volumes present in the host successfully.\n\n");

	}while(0);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}
bool Source::persistDiskCountInConfFile(const unsigned int uDiskCount)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	string strConfFilePath = m_strInstallDirectoryPath + "\\" + CONSISTENCY_DIRECTORY + "\\" + DEPENDENT_SERVICES_CONF_FILE;
	string strDiskCount = "Disk_Count";
	string strDiskNumber =  boost::lexical_cast<string>(uDiskCount);
	if(!WritePrivateProfileString(DISK_COUNT,strDiskCount.c_str(),strDiskNumber.c_str(),strConfFilePath.c_str()) )
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to persist the Disk Count info  in the Failover service.conf file. Error Code :%d \n",GetLastError());
		bRetValue = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

// --------------------------------------------------------------------------
// This is a helper function of "ListVolumesPresentInSystem".
// Process each volume.
// Discard the fix drives.
// enumerate volumes till FindNextVolume() returns "FALSE".
// --------------------------------------------------------------------------

BOOL Source::EnumerateVolumes (HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize)
{
   DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   BOOL bFlag ;           // generic results flag for return
   DWORD dwSysFlags;     // flags that describe the file system
   TCHAR FileSysNameBuf[FILESYSNAMEBUFSIZE];

   if(DRIVE_FIXED != GetDriveType(szBuffer))
   {

		 bFlag = FindNextVolume(
			     volumeHandle,    // handle to scan being conducted
				 szBuffer,     // pointer to output
				iBufSize // size of output buffer
			);
	   DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	   return (bFlag);
   }
   char szVolumePath[MAX_PATH+1] = {0};
   DWORD dwVolPathSize = 0;
   string strVolumeGuid = string(szBuffer);

   if(GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
	{        
		m_mapOfGuidsWithLogicalNames[strVolumeGuid] = string(szVolumePath);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode: %d\n",GetLastError());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return FALSE;
	}
  
   // Is this volume an NTFS file system? 
   // PR#10815: Long Path support
   SVGetVolumeInformation( szBuffer, NULL, 0, NULL, NULL,
                         &dwSysFlags, FileSysNameBuf, 
                         ARRAYSIZE(FileSysNameBuf));

   bFlag = FindNextVolume(
             volumeHandle,    // handle to scan being conducted
             szBuffer,     // pointer to output
             iBufSize // size of output buffer
           );
	
   DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
   return (bFlag); 
}

// --------------------------------------------------------------------------
// In this module we are filling volumes guids with their correspoding logical names into "m_mapOfGuidsWithLogicalNames" by using the "EnumerateVolumes" function call.
// Remove the trailing "\" from volume GUID
// --------------------------------------------------------------------------
BOOL Source:: ListVolumesPresentInSystem()
{
   DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   TCHAR buf[BUFSIZE];      // buffer for unique volume identifiers
   HANDLE volumeHandle;     // handle for the volume scan
   BOOL bFlag;              // generic results flag
   int iCmdLineArg = 1;
   map<string,string> tempMap;

    // Open a scan for volumes.
   volumeHandle = FindFirstVolume (buf, BUFSIZE );

   if (volumeHandle == INVALID_HANDLE_VALUE)
   {
      DebugPrintf(SV_LOG_ERROR,"No volume found in entire sytem! \n");
      DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
      return FALSE;
   }

   // We have a volume,now process it.
   bFlag = EnumerateVolumes (volumeHandle, buf, BUFSIZE);

   // Do while we have volumes to process.
   while (bFlag) 
   {
      bFlag = EnumerateVolumes (volumeHandle, buf, BUFSIZE);
   }

   // Close out the volume scan.
   bFlag = FindVolumeClose(
              volumeHandle  // handle to be closed
           );
   DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
   return bFlag;
}


//----------------------------------------------------------------------
//
//*** DumpVolumesOrMntPtsExtentOnDisk

// Dumps the extents for the specified volume.
// Finding the extent of logical volume on the Disks.
// Here we're Filling the volInfo structure with the corresponding volume information and 
// Then Using the "m_mapVolInfo" to store the volInfo struct with the corresponding the Disk number.
// In m_mapVolInfo,key is Disk Number and corresponding value is VoInfo structure.
//----------------------------------------------------------------------
bool Source::DumpVolumesOrMntPtsExtentOnDisk( )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	HANDLE	hVolume;
	ULONG	extent;
	int      uDiskNumber = -1;
	ULONG	bytesWritten;
	UCHAR	DiskExtentsBuffer[0x400];
	PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
	volInfo obj;
	map<string,string> tempMap;

	map<string,string>::iterator iter_beg = m_mapOfGuidsWithLogicalNames.begin();
	map<string,string>::iterator iter_end = m_mapOfGuidsWithLogicalNames.end();

	while(iter_beg != iter_end)
	{
	   string strTemp = (iter_beg->first);
	   string::size_type pos = strTemp.find_last_of("\\");
	   if(pos !=string::npos)
	   {
		   string strSecondPart = (iter_beg)->second;
		   strTemp.erase(pos);
		   tempMap[strTemp]= strSecondPart;
		}
		else
		{
			tempMap[iter_beg->first] = iter_beg->second;
		}
	    iter_beg++;	
	}
	m_mapOfGuidsWithLogicalNames = tempMap;
	iter_beg = m_mapOfGuidsWithLogicalNames.begin();
	
	while(iter_beg != iter_end)
	{
		// Open the volume
		// PR#10815: Long Path support
		hVolume = SVCreateFile((iter_beg->first).c_str() ,
					GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
					NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |FILE_FLAG_OPEN_REPARSE_POINT  , NULL );
		if( hVolume == INVALID_HANDLE_VALUE ) 
		{
			DebugPrintf(SV_LOG_ERROR,"Error getting extents for drive.Error Code %d \n",GetLastError());
			bResult = false;
			break;
		}
		//_tprintf(_T("Extents for %s:\n"), iter_beg->second.c_str() ); 
		//
		// Get the extents
		//
		if( DeviceIoControl( hVolume,
			        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
				    NULL, 0,
					DiskExtents, sizeof(DiskExtentsBuffer),
					&bytesWritten, NULL ) ) 
		{
        	//
			// Dump the extents
			//
			LARGE_INTEGER startOffset;
			LARGE_INTEGER PartitionLength;
			LARGE_INTEGER EndingOffset;
			for( extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
				startOffset = DiskExtents->Extents[extent].StartingOffset;
				PartitionLength = DiskExtents->Extents[extent].ExtentLength;
				EndingOffset.QuadPart = startOffset.QuadPart + PartitionLength.QuadPart;
				uDiskNumber			= DiskExtents->Extents[extent].DiskNumber;
				obj.startingOffset  = startOffset;
				obj.partitionLength = PartitionLength;
				obj.endingOffset    = EndingOffset;
				//obj.strVolumeName   = iter->
				/*_tprintf(_T("   Extent [%d]:\n"), extent + 1 );
				_tprintf(_T("        Disk:   %d\n"), 
					DiskExtents->Extents[extent].DiskNumber);
				 _tprintf(_T("      Offset: %I64d\n"), 
						DiskExtents->Extents[extent].StartingOffset );
				_tprintf(_T("      Length: %I64d\n"), 
						DiskExtents->Extents[extent].ExtentLength );
		         _tprintf(_T("  End Offset: %I64d\n"), 
					    EndingOffset.QuadPart);*/
			}
			inm_strncpy_s(obj.strVolumeName, ARRAYSIZE(obj.strVolumeName), iter_beg->second.c_str(),MAX_PATH);
            m_mapVolInfo.insert(pair<int,volInfo>(uDiskNumber,obj));
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed with Error code : [%lu].\n",GetLastError());
			bResult = false;
			break;
		}
		CloseHandle( hVolume );
		iter_beg++;
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool Source::getDiskSize (string &strDriveNumber)
{
	  DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
      DebugPrintf(SV_LOG_INFO,"Disk Name = %s\n",strDriveNumber.c_str());	  
	  bool bFlag = true;
	  GET_LENGTH_INFORMATION dInfo;
      DWORD dInfoSize = sizeof(GET_LENGTH_INFORMATION);
	  DWORD dwValue = 0;
	  do
	  {
		// Try to get a handle to PhysicalDrive IOCTL, report failure and exit if can't.
		// PR#10815: Long Path support
		hPhysicalDriveIOCTL = SVCreateFile (strDriveNumber.c_str(), GENERIC_WRITE |GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
		{
			DebugPrintf(SV_LOG_ERROR,"getDiskSize::Failed to get the disk handle for disk %s with Error code: %d\n",strDriveNumber.c_str(),GetLastError());
			bFlag = false;
			break;
		}
		  
        DebugPrintf(SV_LOG_INFO,"Handle of drive %s opened successfully",strDriveNumber.c_str());
		bFlag = DeviceIoControl(hPhysicalDriveIOCTL,
	    		IOCTL_DISK_GET_LENGTH_INFO,
				NULL, 0,	
				&dInfo, dInfoSize,
				&dwValue, NULL);
		if (bFlag) 
		{
			ulDiskSize = dInfo.Length.QuadPart;
			CloseHandle(hPhysicalDriveIOCTL);
		}
		else 
		{
				DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_GET_LENGTH_INFO failed...(%lu) - (%d)\n",GetLastError(),dwValue);
				CloseHandle(hPhysicalDriveIOCTL);
				bFlag = false;
				break;
		}
	  }while(0);
	  
	  DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);  
	  return bFlag;
}

bool Source::readDiskExtents(string &strDriveNumber,DRIVE_LAYOUT_INFORMATION_EX** ptrDriveLayoutInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFlag = true;
	do
	{
			// Try to get a handle to PhysicalDrive IOCTL, report failure and exit if can't.
			// PR#10815: Long Path support
			hPhysicalDriveIOCTL = SVCreateFile (strDriveNumber.c_str(), GENERIC_WRITE |GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
			{
				DebugPrintf(SV_LOG_ERROR,"readDiskExtents::Failed to get the disk handle for disk %s with Error code: %d\n",strDriveNumber.c_str(),GetLastError());
				bFlag = false;
				break;
			}
			DWORD returned;
			int estimatedPartitionCount = 4;
			loop:
			DWORD dwSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + estimatedPartitionCount * sizeof(PARTITION_INFORMATION_EX);
			try
			{
				*ptrDriveLayoutInfo = (DRIVE_LAYOUT_INFORMATION_EX*) new BYTE[dwSize];//Just to ensure enough space
			}
			catch(std::bad_alloc)
			{
				cout<<"\n Memory allocation failed."<<endl;
				DebugPrintf(SV_LOG_ERROR,"readDiskExtents::Memory allocation through new failed.\n");
	   			CloseHandle(hPhysicalDriveIOCTL);
				bFlag = false;
				break;

			}
			BOOL b = DeviceIoControl(hPhysicalDriveIOCTL,  // handle to device
								IOCTL_DISK_GET_DRIVE_LAYOUT_EX, // dwIoControlCode
								NULL,				  // lpInBuffer
								0,					  // nInBufferSize
								(LPVOID)*ptrDriveLayoutInfo,      // output buffer
								dwSize,				 // size of output buffer
								(LPDWORD)&returned, // number of bytes returned
								NULL);				 // OVERLAPPED structure
			if (FALSE == b)
			{
				if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
				{
					estimatedPartitionCount *= 2;
					delete []*ptrDriveLayoutInfo;// without ptr'*' it was crashing...
					goto loop;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"readDiskExtents::IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed with Error code: %d\n",GetLastError());
					CloseHandle(hPhysicalDriveIOCTL);
					delete []*ptrDriveLayoutInfo;// without ptr'*' it was crashing...
     				bFlag = false;
					break;
				}
			}
	}while(0);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFlag;
}

int Source::ReadPhysicalDrives (void)
{
   DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   USES_CONVERSION;
   int iDiskSignature = 0;
   unsigned int uActualPartitionCount = 0;
   BOOL bMBR = false;
   BOOL bGPT = false;
   LARGE_INTEGER DiskSizeInfo ;
   LARGE_INTEGER lEndOffset     ;
   LARGE_INTEGER StartIngOffset ;
   LARGE_INTEGER PartitonLength ;
   string strDiveNumber;
   string strMbrFileName;
   LARGE_INTEGER iLargeInt;
 
   DRIVE_LAYOUT_INFORMATION_EX* ptrDriveLayoutInfo;
   int iBootIndicator;
   int iTemp;
	string strCurrentHost = stGetHostName();
	if(strCurrentHost.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"ReadPhysicalDrives::Found emtpy hostname\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}
   //This File will contain Disk Signatures of all disks Present in the System
   string strDiskSignatureFile = m_strDataFolderPath + string("\\") + strCurrentHost+ string("_") + DISK_SIGNATURE_FILE;
   ofstream diskSigFile(strDiskSignatureFile.c_str());
   if(diskSigFile.fail())
   {
		DebugPrintf(SV_LOG_ERROR,"ReadPhysicalDrives::Failed to open file :%s\n",strDiskSignatureFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
 		return 1;
   }
   //Object of "DiskInformation".
   DiskInformation objDiskInfomartion;

   DebugPrintf(SV_LOG_INFO,"-----------------------------------------------------\n");
   DebugPrintf(SV_LOG_INFO,"Total number of disks present in the system : %d\n",m_mapOfDeviceIdAndControllerIds.size());
   

    //enumerating through all Disks present in the system.
    map<string,string>::iterator iterDS  = m_mapOfDeviceIdAndControllerIds.begin();    
    while (iterDS != m_mapOfDeviceIdAndControllerIds.end())
    {
        //iterDS->second is drive name - ex: \\.\PHYSICALDRIVE5
        //so the disk number is at end.
        string strDiskNmbr = iterDS->second.substr(m_strDiskName.size());
        int iDiskNumber = atoi(strDiskNmbr.c_str()); //converting disknmbr to integer format
		if(strDiskNmbr.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"ReadPhysicalDrives::Found emtpy Disk Number\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return 1;
		}

        // Try to get a handle to PhysicalDrive IOCTL, report failure and exit if can't.
      	string strDriveNumber = m_strDiskName + strDiskNmbr;
		if(false == getDiskSize(strDriveNumber))
		{	
			DebugPrintf(SV_LOG_ERROR,"ReadPhysicalDrives::getDiskSize Failed\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return 1;
		}
		else
		{
			DiskSizeInfo.QuadPart = ulDiskSize;
		}
		
		if(false == readDiskExtents(strDriveNumber,&ptrDriveLayoutInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"ReadPhysicalDrives::readDiskExtents Failed\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return 1;
		}
		else
		{
			if(IsPartitionDynamic(ptrDriveLayoutInfo->PartitionEntry[0]))
			{
                // Bug 16172 - To support this, making below changes i.e consider this as RAW pst and continue.
                DebugPrintf(SV_LOG_INFO,"\nDisk = %d is dynamic. Not collecting any other info for this disk.\n",iDiskNumber);
                objDiskInfomartion.dt = DYNAMIC;                
                objDiskInfomartion.pst = RAWDISK;
                objDiskInfomartion.dwPartitionCount = 0;
			    objDiskInfomartion.ulDiskSignature  = 0;
                objDiskInfomartion.uDiskNumber      = iDiskNumber;
                objDiskInfomartion.DiskSize = DiskSizeInfo;
                objDiskInfomartion.dwActualPartitonCount = 0;
                disk_vector.push_back(objDiskInfomartion);
                iterDS++;
                continue;

				/*DebugPrintf(SV_LOG_INFO,"\nDisk = %d is dynamic\n",iDiskNumber);
				DebugPrintf(SV_LOG_INFO,"In Current solution we donot support dynamic disks. Cannot proceed furhter.Exiting..\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				exit(1);*/
			}
			else
			{
				objDiskInfomartion.dt = BASIC;
				DebugPrintf(SV_LOG_INFO,"\nDisk = %d is Basic\n",iDiskNumber);
			}
			
			//Find Partition Style Of disk
			switch(ptrDriveLayoutInfo->PartitionStyle)
			{
				case 0://MBR
					objDiskInfomartion.pst = MBR;
					iDiskSignature = ptrDriveLayoutInfo->Mbr.Signature;
					//Dump the disk Signature in the File
					bMBR = true;
					bGPT = false;
					//TO DO.Case to be handled
					//disk is uninitialized!! Then initialze it ..but in Source its something which is absurd
					if(0 == iDiskSignature)
					{
						DebugPrintf(SV_LOG_ERROR,"Unintialised disk found!! Please remove this disk from VM else if you intend to use this disk,please  initialise this disk and create Partition/Volumes on itThen try this command again.Cannot proceed further..\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						exit(-1);
					}

					strMbrFileName = m_strDataFolderPath + string(strCurrentHost)+ string("_") + string("Disk_") + strDiskNmbr + (MBR_FILE_NAME);
					DebugPrintf(SV_LOG_INFO,"Name of MBR file to be created = %s\n",strMbrFileName.c_str());
					
					iLargeInt.LowPart = 0;
					iLargeInt.HighPart = 0;

					if(-1 == DumpMbrInFile(iDiskNumber,iLargeInt,strMbrFileName,512))
					{
						DebugPrintf(SV_LOG_ERROR,"Error: Dumping of MBR failed for disk = %d",iDiskNumber);
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return 1;
					}
					break;
				case 1://GPT
					objDiskInfomartion.pst = GPT;
					DebugPrintf(SV_LOG_INFO,"Partition style is GPT\n");
					WCHAR wcGuid[256];
					if(0 == StringFromGUID2(ptrDriveLayoutInfo->Gpt.DiskId, wcGuid, 256))
					{
						WCHAR wcGuid[512];
                        if(0 == StringFromGUID2(ptrDriveLayoutInfo->Gpt.DiskId, wcGuid, 512))
						{
							DebugPrintf(SV_LOG_ERROR,"DEBUG: Unable to convert the Guid itno string = %d",GetLastError());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return 1;
						}
					}
					//Dump the Disk ID so that we can match it at the target side's disk's GUID
                   	bGPT		   = true;
					bMBR		   = false;
					iBootIndicator = -1;
					iTemp		   = 0x07;
					
					strMbrFileName = m_strDataFolderPath + strCurrentHost+ string("_") + string("Disk_") + strDiskNmbr + (GPT_FILE_NAME);
					
					DebugPrintf(SV_LOG_INFO,"GPT file name is = %s\n",strMbrFileName.c_str());
					
					iLargeInt.LowPart = 0;
					iLargeInt.HighPart = 0;
					if(-1 == DumpMbrInFile(iDiskNumber,iLargeInt,strMbrFileName,17408))
					{
						DebugPrintf(SV_LOG_ERROR,"Error: Dumping of MBR failed for disk = %d",iDiskNumber);
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return 1;
					}

					break;
				case 2://RAW
					objDiskInfomartion.pst = RAWDISK;
					DebugPrintf(SV_LOG_INFO,"INFO:Partition style is RAW \n");
					break;
			}

			if(0 == ptrDriveLayoutInfo->PartitionCount)
			{
					DebugPrintf(SV_LOG_ERROR,"Partiton Count for disk %d is 0 \n",iDiskNumber);
					DebugPrintf(SV_LOG_ERROR,"Found An unintialised Disk. \n");
					DebugPrintf(SV_LOG_ERROR,"Please remove/initialize the uninitialized disk\n");
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return 1;
			}
			DebugPrintf(SV_LOG_INFO,"Obtained Partition Count = %d\n",ptrDriveLayoutInfo->PartitionCount);
			
			//Finding the Partition information...
			DWORD i = 0;
			for (; i < ptrDriveLayoutInfo->PartitionCount; i++) 
			{
				DebugPrintf(SV_LOG_INFO,"Start of Partiton = %d\n",i+1);
				PartitionInfo objPartitionInfo;
				//TO DO
				//Use outer Variables heres
				LARGE_INTEGER lTemp1 = ptrDriveLayoutInfo->PartitionEntry[i].StartingOffset;
				LARGE_INTEGER lTemp2 = ptrDriveLayoutInfo->PartitionEntry[i].PartitionLength;

				//when to Exit from Loop.
				/*1. when total no partitons are traversed and
				2.When no more partiton is there to traverse
				*/
				if((lTemp2.QuadPart == 0))
				{
					DebugPrintf(SV_LOG_INFO," Number of partitions in the disk(ZERO CASE when ending offset is gone) = %lu\n", i);
					uActualPartitionCount = i;
					break;
				}
				////////////////////////////////////////////////////////////////////
				//	FIND PARTITON INFORMATION FOR  Volumes by Starting OFFSET
				////////////////////////////////////////////////////////////////////
				StartIngOffset = ptrDriveLayoutInfo->PartitionEntry[i].StartingOffset;
				PartitonLength = ptrDriveLayoutInfo->PartitionEntry[i].PartitionLength;
				lEndOffset.QuadPart = StartIngOffset.QuadPart + PartitonLength.QuadPart;
				
				if(bMBR)
				{
					iBootIndicator = ptrDriveLayoutInfo->PartitionEntry[i].Mbr.BootIndicator;
					//No need to check return value 
					//iTemp = findPartitionType(ptrDriveLayoutInfo->PartitionEntry[i].Mbr.PartitionType);
					iTemp = (int)ptrDriveLayoutInfo->PartitionEntry[i].Mbr.PartitionType;
					
				}
				objPartitionInfo.iBootIndicator			    = iBootIndicator;
				objPartitionInfo.uPartitionType             = iTemp;
				objPartitionInfo.uDiskNumber				= iDiskNumber;
				objPartitionInfo.startOffset.QuadPart		= StartIngOffset.QuadPart;
				objPartitionInfo.PartitionLength.QuadPart   = PartitonLength.QuadPart;
				objPartitionInfo.EndingOffset.QuadPart      = lEndOffset.QuadPart;
				
				//Pushing the Partition structure
				partition_vector.push_back(objPartitionInfo);
			}

			DebugPrintf(SV_LOG_INFO,"Value of i = %lu\n",i);
			if(i == (ptrDriveLayoutInfo->PartitionCount))
			{
				DebugPrintf(SV_LOG_INFO," Number of partitions in the disk  = %lu\n",i);
				uActualPartitionCount = i;
			}
			objDiskInfomartion.DiskSize = DiskSizeInfo;

			/*Added to avoid Problem in reading..
			Actual partition count was displaying as "6" which is absurd for BASIC disks having MBR partition...
			Need to fidn Proper soln
			*/
			DebugPrintf(SV_LOG_INFO,"Actual Partition Count = %u\n",uActualPartitionCount);
			if((uActualPartitionCount >= 4) && (MBR == objDiskInfomartion.pst ))
			{
				uActualPartitionCount = 4;
				DebugPrintf(SV_LOG_INFO,"\nAgain going to get fresh volume List from mount manager");
				DebugPrintf(SV_LOG_INFO,"\nPartition Count = %u\n",uActualPartitionCount);
			}
			if(bMBR)
			{
				objDiskInfomartion.dwActualPartitonCount = uActualPartitionCount;
				DebugPrintf(SV_LOG_INFO,"MBR - Actual Partition count assigned = %u\n",uActualPartitionCount);
			}
			else if(bGPT)
            {
				objDiskInfomartion.dwActualPartitonCount = ptrDriveLayoutInfo->PartitionCount-1;
				DebugPrintf(SV_LOG_INFO,"GPT - Actual Partition count assigned = %u\n",ptrDriveLayoutInfo->PartitionCount-1);
            }

			objDiskInfomartion.dwPartitionCount					= ptrDriveLayoutInfo->PartitionCount;
			objDiskInfomartion.ulDiskSignature					= iDiskSignature;
			objDiskInfomartion.uDiskNumber						= iDiskNumber;
			//Push disk info in the Vector
			disk_vector.push_back(objDiskInfomartion);
			//delete []ptrDriveLayoutInfo;  //Memory Leak :: Fix Me
		}
        iterDS++;
  }
  return 0;
}

int Source::findPartitionType(BYTE partitionStyle)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;
    DebugPrintf(SV_LOG_INFO,"Partition Type - %d\n",(int)partitionStyle);
	switch(partitionStyle)
	{ 
		case VALID_NTFT:
//						cout<<"\n VALID_NTFT file system";
						iRetValue = VALID_NTFT;
						break;
		case PARTITION_NTFT:
//						cout<<"\n PARTITION_NTFT system";
						iRetValue = PARTITION_NTFT;
						break;
		case PARTITION_HUGE:
//						cout<<"\n PARTITION_HUGE file system";
						iRetValue = PARTITION_HUGE;
						break;
		case PARTITION_LDM:
//						cout<<"\n PARTITION_LDM system";
						iRetValue = PARTITION_LDM;
						break;
		case PARTITION_IFS:
//						cout<<"\n Installable file system";
						iRetValue = PARTITION_IFS;
						break;
		case PARTITION_OS2BOOTMGR:
//						cout<<"\n PARTITION_OS2BOOTMGR file system";
						iRetValue = PARTITION_OS2BOOTMGR;
						break;
		case PARTITION_EXTENDED:
//						cout<<"\n An Extended Partition\n";
						iRetValue = PARTITION_EXTENDED;
						break;
        case PARTITION_FAT32:                    
//						cout<<"\n An FAT32 Partition\n";
						iRetValue = PARTITION_FAT32;
						break;
		case PARTITION_FAT32_XINT13:
//						cout<<"\n PARTITION_FAT32_XINT13 Partition\n";
						iRetValue = PARTITION_FAT32_XINT13;
						break;
		case PARTITION_ENTRY_UNUSED:
//						cout<<"\n PARTITION_ENTRY_UNUSED Partition "<<endl;
						iRetValue = PARTITION_ENTRY_UNUSED;
						break;
		default:
//						cout<<"\n Some other Partition\n";
						iRetValue = -1;
						break;
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}


/*
	
	
	
*/

//----------------------------------------------------------------------
//
//*** mapLogicalVolmuesToDiskOffset

// 1.In this module we are trying to locate the Volume Locations in parition.
// 2.In case of extended Partitions,there might be more than one volume per partion.
// 3.We are stroing the volumes corresponding to a Particular volume in the vector "m_VolumeInfo_PerPartition" .
// 4.then while calling the PersistData we are passing the Partition struct and this vector
/*
	Logic is:
		Iterate thru the Partition info vector.
		Pick Volume and then see,if volume and Partition belongs to same disk.
		If yes,See,if volume lies with in the Partition Range.
		If yes,push all volumes corresponding to this partition in a separate vectot "m_VolumeInfo_PerPartition"
		Finally call PersistDiskData to persisit the volume and Partiton corresponding information
*/
//----------------------------------------------------------------------
HRESULT Source::mapLogicalVolmuesToDiskOffset()
{	
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HRESULT h_Rest = S_OK;
	//cout<<"\nTotal Number of Partitions in All Disks. "<<partition_vector.size()<<endl;
	vector<PartitionInfo>::iterator iter_Beg = partition_vector.begin();
	vector<PartitionInfo>::iterator iter_End = partition_vector.end();
    std::map<unsigned int,unsigned int> diskPartitionCountMap;
	typedef std::pair<PartitionInfo,std::vector<volInfo> >partitionPair;
	std::vector<partitionPair> partVolVector;
	///std::vector<partVolPair>partVolVector;
	vector<volInfo> MisMateched_Volumes;
	PartitionInfo objPartitionInfo ;
	volInfo objVolInfo;
	multimap<int ,volInfo>::iterator iterMap;
	static vector<string> strVolumeNamesAlreadyProcessed;

	while(iter_Beg != iter_End)
	{
		objPartitionInfo = *iter_Beg;
		//Find out whether the disk number of the Volumes exists.
		//If yes,then Try to find out if the starting and Ending offset matches..
		for(iterMap = m_mapVolInfo.begin();iterMap!= m_mapVolInfo.end();iterMap++)
		{
			if(!(objPartitionInfo.uDiskNumber == iterMap->first))
			{
				continue;
			}
			//Disk Number Matches...Now find out if offsets matches..
			objVolInfo = iterMap->second;	
			vector<string>::iterator iter_AlreadyProcessedVolumes ;
			iter_AlreadyProcessedVolumes = find(strVolumeNamesAlreadyProcessed.begin(),strVolumeNamesAlreadyProcessed.end(),objVolInfo.strVolumeName);

			if(iter_AlreadyProcessedVolumes == strVolumeNamesAlreadyProcessed.end())
			{
				if((objPartitionInfo.startOffset.QuadPart <= objVolInfo.startingOffset.QuadPart) && (objPartitionInfo.EndingOffset.QuadPart >= objVolInfo.endingOffset.QuadPart))
				{
					DebugPrintf(SV_LOG_INFO,"Volume %s and its ",objVolInfo.strVolumeName);
					DebugPrintf(SV_LOG_INFO,"Boot indicator is %d\n",objPartitionInfo.iBootIndicator);
					m_VolumeInfo_PerPartition.push_back(objVolInfo);
					strVolumeNamesAlreadyProcessed.push_back(objVolInfo.strVolumeName);
				}
			}
		}
		
		iter_Beg++;
		if(!m_VolumeInfo_PerPartition.empty())
		{
			typedef std::map<unsigned int,unsigned int>::iterator myIter;
			myIter &mapIter = diskPartitionCountMap.find(objPartitionInfo.uDiskNumber);
			////If disk number found,just increment the Count
			if(mapIter != diskPartitionCountMap.end())
			{
				unsigned int uParititionCount = mapIter->second;
				mapIter->second = ++uParititionCount;
			}
			else
			{
				unsigned int initialCount = 1;
				diskPartitionCountMap.insert(std::make_pair(objPartitionInfo.uDiskNumber,initialCount));
			}
			partitionPair objpartitionPair;
			objpartitionPair.first = objPartitionInfo;
			objpartitionPair.second = m_VolumeInfo_PerPartition;
			partVolVector.push_back(objpartitionPair);
		}

		//	partVolPair myPair;
		//	myPair.first = objPartitionInfo;
		//	myPair.second = m_VolumeInfo_PerPartition;
			//partVolVector.push_back(myPair);
		m_VolumeInfo_PerPartition.clear();
	}
	alterDiskConfig(diskPartitionCountMap);
	std::vector<partitionPair>::iterator iterVec = partVolVector.begin();
	while(iterVec != partVolVector.end())
	{
		if(S_FALSE == PersisitDiskData((*iterVec).first,(*iterVec).second))
		{
			DebugPrintf(SV_LOG_ERROR,"\n PersisitDiskData() Failed ");
			DebugPrintf(SV_LOG_INFO,"\nExiting %s ",__FUNCTION__);
			h_Rest = S_FALSE;
			break;
		}
		iterVec++;
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return h_Rest;
}
//----------------------------------------------------------------------
//
//*** write_disk_info
// 1.Persist the Disk Structure
//----------------------------------------------------------------------
void Source::write_disk_info(DiskInformation* pDisk,ofstream& f)
{   
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	f.write(reinterpret_cast<const char*>(pDisk),sizeof(*pDisk));
    DebugPrintf(SV_LOG_INFO,"Dumped disk information for disk - %u\n\n",pDisk->uDiskNumber);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

}
//----------------------------------------------------------------------
//
//*** write_partition_info
// 1.Persist the PartitionInfo Structure
//----------------------------------------------------------------------
void Source::write_partition_info(PartitionInfo* pPart,ofstream& f)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	//printf("\n PartitionEnding Offset    = %I64d ",pPart->EndingOffset);
	//printf("\n Partition Starting Offset = %I64d ",pPart->startOffset);
	//printf("\n Partition Length          = %I64d ",pPart->PartitionLength);
	//printf("\n disk number			  	 = %d",    pPart->uDiskNumber);
	//printf("\n Boot Indicator			 = %d",     pPart->iBootIndicator);
	//printf("\n Partition type 			 = %d",     pPart->uPartitionType);
	//printf("\n Number of Volumes in Partition = %d",pPart->uNoOfVolumesInParttion);
	f.write(reinterpret_cast<const char*>(pPart),sizeof(*pPart));
	DebugPrintf(SV_LOG_INFO,"Dumped partition information for this partition\n\n");
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

}
//----------------------------------------------------------------------
//
//*** write_volume_info
// 1.Persist the volInfo Structure
//----------------------------------------------------------------------
void Source::write_volume_info(volInfo* pVol,ofstream& f)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	f.write(reinterpret_cast<const char*>(pVol),sizeof(*pVol));
	DebugPrintf(SV_LOG_INFO,"Dumped volume information for Volume - %s\n\n",pVol->strVolumeName);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//----------------------------------------------------------------------
//
//*** PersisitDiskData

// 1.In this module we are persisting the Disk,partiton and volume information .
// 2.First we are persisiting the disk header followed by Partiton header and then Volume header.
/*
 ****** LOGIC:
		Since we already getting the MBR or GPT record in readPhysicalDriveNT.with the help of these recoeds we can set the External layout of the structure.
		But in case of MBR disks ,there can be Extendede Partition.We need to create those Partition automatically also.
		To achieve this we are copying the EBR record information with the Help of executeProcess.
*/
HRESULT Source::PersisitDiskData(PartitionInfo &objPartitionInformaton,vector<volInfo> &tempVector)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	vector<DiskInformation>::iterator iter = disk_vector.begin();
	vector<volInfo>::iterator iter_volInfo;
	string strDiskInfoFile;

	//Find the current host 	
	string strCurrentHost = stGetHostName();
	if(strCurrentHost.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Found Empty HostName.Cannot Procees further.Exiting...");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
	strDiskInfoFile = m_strDataFolderPath + strCurrentHost + string("_")+ string("disk.bin");

	ofstream outfile(strDiskInfoFile.c_str(),ios_base::binary | ios_base::out | ios_base::app);

	DiskInformation  objDiskInfomartion;
	for(;iter != disk_vector.end();iter ++)
	{
		objDiskInfomartion = *iter;
		if(objPartitionInformaton.uDiskNumber == (*iter).uDiskNumber)
		{
			//Just to make sure that Disk Structure is written for Per disk exactly once...
			if(false == (diskBitset.test((*iter).uDiskNumber)))
			{
				diskBitset.set((*iter).uDiskNumber);
                DebugPrintf(SV_LOG_INFO,"========================================================\n");
                DebugPrintf(SV_LOG_INFO,"DISK INFORMATION\n");
                DebugPrintf(SV_LOG_INFO,"----------------\n");
                DebugPrintf(SV_LOG_INFO," Disk Id                      = %u\n",iter->uDiskNumber);
		        DebugPrintf(SV_LOG_INFO," Disk Type                    = %d\n",iter->dt);
		        DebugPrintf(SV_LOG_INFO," Disk No of Actual Partitions = %lu\n",iter->dwActualPartitonCount);
		        DebugPrintf(SV_LOG_INFO," Disk No of Partitions        = %lu\n",iter->dwPartitionCount);
		        DebugPrintf(SV_LOG_INFO," Disk Partition Style         = %d\n",iter->pst);
		        DebugPrintf(SV_LOG_INFO," Disk Signature               = %lu\n",iter->ulDiskSignature);
                DebugPrintf(SV_LOG_INFO," Disk Size                    = %I64d\n",iter->DiskSize.QuadPart);

				//Dumping the Disk info
				write_disk_info(&objDiskInfomartion,outfile);
			}
		
			objPartitionInformaton.uNoOfVolumesInParttion = tempVector.size();
			
            DebugPrintf(SV_LOG_INFO,"  Partition Information\n");
            DebugPrintf(SV_LOG_INFO,"  ---------------------\n");
            DebugPrintf(SV_LOG_INFO,"  Partition Disk Number       = %u\n",objPartitionInformaton.uDiskNumber);
            DebugPrintf(SV_LOG_INFO,"  Partition Partition Type    = %d\n",objPartitionInformaton.uPartitionType);
            DebugPrintf(SV_LOG_INFO,"  Partition Boot Indicator    = %d\n",objPartitionInformaton.iBootIndicator);
            DebugPrintf(SV_LOG_INFO,"  Partition No of Volumes     = %u\n",objPartitionInformaton.uNoOfVolumesInParttion);
            DebugPrintf(SV_LOG_INFO,"  Partition Partition Length  = %I64d\n",objPartitionInformaton.PartitionLength.QuadPart);
            DebugPrintf(SV_LOG_INFO,"  Partition Start Offset      = %I64d\n",objPartitionInformaton.startOffset.QuadPart);
            DebugPrintf(SV_LOG_INFO,"  Partition Ending Offset     = %I64d\n",objPartitionInformaton.EndingOffset.QuadPart);

			//Dumping the Partition info
			write_partition_info(&objPartitionInformaton,outfile);
			
			iter_volInfo = tempVector.begin();
			for(;iter_volInfo != tempVector.end();iter_volInfo++)
			{
				DebugPrintf(SV_LOG_INFO,"   Volume Information\n");
                DebugPrintf(SV_LOG_INFO,"   ------------------\n");
                DebugPrintf(SV_LOG_INFO,"   Volume Volume Name         = %s\n",(*iter_volInfo).strVolumeName);
	            DebugPrintf(SV_LOG_INFO,"   Volume Partition Length    = %I64d\n",(*iter_volInfo).partitionLength.QuadPart);
	            DebugPrintf(SV_LOG_INFO,"   Volume Starting Offset     = %I64d\n",(*iter_volInfo).startingOffset.QuadPart);
	            DebugPrintf(SV_LOG_INFO,"   Volume Ending Offset       = %I64d\n",(*iter_volInfo).endingOffset.QuadPart);			            
				
				volInfo objVolInformation;
				objVolInformation = *iter_volInfo;
				write_volume_info(&objVolInformation,outfile);
			} 
            
			if( (5 == objPartitionInformaton.uPartitionType) || 
                (15 == objPartitionInformaton.uPartitionType) )
            {
                if(false == CollectEBRs((*iter).uDiskNumber,(ACE_LOFF_T)objPartitionInformaton.startOffset.QuadPart))
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to collect EBRs for the extended partition on disk - %u\n",(*iter).uDiskNumber);
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return S_FALSE;
                }
            }
		}
	}
	return S_OK;
}

// 1. Read EBR - (First EBR resides at starting offset of extended partition)
// 2. Parse EBR and get next EBR's starting offset (Bytes 474-477 in little endian format)
// 3. if next EBR's starting offset is not zero, then collect it and repeat from step 2.
// 4. if next EBR's starting offset is zero, then return.
bool Source::CollectEBRs(unsigned int DiskNumber, ACE_LOFF_T PartitionStartOffset)
{   
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    ACE_LOFF_T bytes_to_skip = PartitionStartOffset;

    string strDriveNumber;
    stringstream out;
    out<<DiskNumber;
    strDriveNumber = out.str();
    out.str(std::string());
    char disk_name[256];
	inm_sprintf_s(disk_name, ARRAYSIZE(disk_name), "\\\\.\\PhysicalDrive%u", DiskNumber);

    string strCurrentHost = stGetHostName();
	if(strCurrentHost.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Found Empty HostName.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    int vol_count = 1;
    while(1)
    {
        string strVolCount;
        out<<vol_count;
        strVolCount = out.str();
        out.str(std::string());
        out.clear();
        
        string strEBRFileName = m_strDataFolderPath + 
                                strCurrentHost+ 
                                string("_Disk_") + 
                                strDriveNumber + 
                                string("_LogicalVolume_") + 
                                strVolCount + 
                                EBR_FILE_NAME;

        DebugPrintf(SV_LOG_INFO,"Bytes to skip = %I64d\n",bytes_to_skip);
        unsigned char ebr_sector[MBR_BOOT_SECTOR_LENGTH];
        if(1 == ReadDiskData(disk_name, bytes_to_skip, MBR_BOOT_SECTOR_LENGTH, ebr_sector))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to read the EBR data from disk - %s\n",disk_name);
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return false;
        }
        if(1 == WriteToFileInBinary(ebr_sector, MBR_BOOT_SECTOR_LENGTH, strEBRFileName.c_str()))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to write the EBR-%d for %s to file-%s\n",vol_count, disk_name,strEBRFileName.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return false;
        }
        else
            DebugPrintf(SV_LOG_INFO,"Copied the EBR-%d for %s to file-%s\n",vol_count, disk_name, strEBRFileName.c_str());

        unsigned char ebr_second_entry[EBR_SECOND_ENTRY_SIZE];
        GetBytesFromMBR(ebr_sector, EBR_SECOND_ENTRY_STARTING_INDEX, EBR_SECOND_ENTRY_SIZE, ebr_second_entry);

        ACE_LOFF_T offset = 0;
        ConvertHexArrayToDec(ebr_second_entry, EBR_SECOND_ENTRY_SIZE, offset);
        DebugPrintf(SV_LOG_INFO,"Offset = %I64d\n",offset);
        if(offset == 0)
        {
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return true;
        }

        bytes_to_skip = PartitionStartOffset + offset * 512; //assuming 512 bytes per sector.
        vol_count++;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//----------------------------------------------------------------------
// 1.To set replication Pair,We need to have Source machine IP.
// 2.We are persisting source IP in "sourceMachineName_IP.txt.
//----------------------------------------------------------------------
HRESULT Source::PersistIPAddressOfHostGuestOS()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	char currentHost[256] = {0};
	string strPersistedIPFileName ;
	string strIP_Addres_file_Path ;

	string strCurrentHost = stGetHostName();
	if(strCurrentHost.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Found Empty hostname .Cannot Procees further.Exiting...");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
	
	struct hostent *ptrHostent = NULL;
	if((ptrHostent = gethostbyname(strCurrentHost.c_str()))== NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"\n GetHostByName Failed .Error Code [%d] .Cannot Procees further.Exiting...",WSAGetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
	struct in_addr addr;
	addr.s_addr =  *(u_long *) ptrHostent->h_addr_list[0];
	char *address = inet_ntoa(addr);
	//printf("\nPinging IP Address = %s",address);

	strPersistedIPFileName = strCurrentHost + string("_IP_Address.txt");
	strIP_Addres_file_Path = m_strInstallDirectoryPath + string("\\Failover") + string("\\data\\") + strPersistedIPFileName;
	
	ofstream f(strIP_Addres_file_Path.c_str());
	if(! f.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open file [%s] Cannot Procees further.Exiting...",strIP_Addres_file_Path.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
	f<<address;
    f.close();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;			 
}
/*
* We need the source system volume info for IP chnage
*/
HRESULT Source::PersistSystemVolInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strCurrentHostName = stGetHostName();
	if(strCurrentHostName.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to find the current Host name.Cannot proceed further..");
		return S_FALSE;
	}

	string strPersistedSysVolInfoFile ;
	
	TCHAR szSysPath[MAX_PATH];
	if (0 == GetSystemDirectory(szSysPath, MAX_PATH))
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to fetch the system volume information.Error[%d]",GetLastError());
		return S_FALSE;
	}
	
	strPersistedSysVolInfoFile = m_strDataFolderPath + strCurrentHostName + string("_SysVolInfo.txt");
	ofstream outfile(strPersistedSysVolInfoFile.c_str());
	if(! outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s ",strPersistedSysVolInfoFile.c_str());
		return S_FALSE;
	}
	outfile<<szSysPath;
    outfile.close();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;
}

HRESULT Source::PersistNetworkInterfaceCard()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strCurrentHostName = stGetHostName();
	if(strCurrentHostName.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find the current Host name.Cannot proceed further..\n");
		return S_FALSE;
	}
	HRESULT hr = S_OK;
	string strParameterToDiskPartExe;
	string strExeName;
	
	string strCurrentHost = stGetHostName();
	if(strCurrentHost.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Found empty hostname.Cannot proceed further!");
		return S_FALSE;
	}
	strExeName = string("cmd /c getmac");
	strParameterToDiskPartExe = string (" > ") + string("\"") + m_strDataFolderPath +  string("Inmage_macAdd.Info")+ string("\"") ;
	 
	string strFileName = m_strDataFolderPath + string ("\\") + string("Inmage_macAdd.Info");

	 if(FALSE == ExecuteProcess(strExeName,strParameterToDiskPartExe))
	 {
		 DebugPrintf(SV_LOG_ERROR,"Failed to get MAC Address.\n");
		 DebugPrintf(SV_LOG_ERROR,"Please run rescan from diskmgmt.msc manually.\n");
		 return S_FALSE;
	 }

	ifstream inFile(strFileName.c_str());

	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open the macAdd file.Exiting...\n");
		return S_FALSE;
	}
	vector<string> strVector;
	int iCount = 0;
	string line;
	string strTemp;
	for( ; getline(inFile,line); iCount++)
	{	
		if (iCount < 3)
		{
			continue;
		}
		else
		{
			size_t index = line.find_first_of(" ");
			if(index != std::string::npos)
			{
				strTemp = line.substr(index,line.length());
				//index = string::npos;
				index = strTemp.find_first_of("{");
				if(index != std::string::npos)
				{
					string strNetWorkInterfaceCard = strTemp.substr(index,strTemp.length());
					strVector.push_back(strNetWorkInterfaceCard);
				}
			}
		}
	}
	inFile.close();
	
	if(0 == DeleteFile(strFileName.c_str()))
	{
		DebugPrintf(SV_LOG_ERROR,"Deletion of %s failed with Error [%lu]\n",strFileName.c_str(),GetLastError());
		//Not an big issue Continue...
	}
	strFileName = m_strDataFolderPath  + strCurrentHost + string("_") + ACTIVE_MAC_TRANSPORT;
	ofstream outfile(strFileName.c_str(), ios_base::out);
	if(outfile.fail())
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing.\n",strFileName.c_str());
		return S_FALSE;
	}
	vector<string>::iterator iter = strVector.begin();
	while(iter != strVector.end())
	{
		outfile<<*iter;
		outfile<<endl;
		iter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;

}
bool Source::persistVolumeCount()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strFileName = m_strDataFolderPath + stGetHostName()  + VOL_CNT_FILE;
	ofstream outFile(strFileName.c_str());
	if(outFile.fail())
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strFileName.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	outFile<<m_mapOfGuidsWithLogicalNames.size()<<endl;
	outFile.close();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

/* 	Scan through all volumes and identify the volumes which are not mounted
	Create mount directory SYSTEMDRIVE\ESXSysResPar\<COUNT>
	Mount this volume 
*/
BOOL Source::MountAllUnmountedVolumes()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	TCHAR VolGUID[BUFSIZE];     // buffer for unique volume identifiers (GUID)
	HANDLE volumeHandle;		// handle for the volume scan
	BOOL bFlag = FALSE;			// generic results flag
	int MntCount = 1;			// used for mount directories naming

	// Open a scan for volumes.
	volumeHandle = FindFirstVolume (VolGUID, BUFSIZE );
	if (volumeHandle == INVALID_HANDLE_VALUE)
	{
		DebugPrintf(SV_LOG_ERROR,"No volume is present in this host\n");
		return FALSE;
	}
	bFlag = TRUE; //Since one volume Handle is found
	
	// Scan all volumes to process.	
	while (bFlag) 
	{
		if (DRIVE_FIXED != GetDriveType(VolGUID))
		{
			bFlag = FindNextVolume(volumeHandle,VolGUID,BUFSIZE);		
			continue;
		}

		//If volume is not mounted, then create a mount directory and mount it
		if (IsVolumeNotMounted(VolGUID)) 
		{	
			DebugPrintf(SV_LOG_INFO,"Found an Unmounted Volume. Hence Mounting the volume : %s \n",VolGUID);
			CHAR szSysPath[MAX_PATH];
			CHAR szRootVolumeName[MAX_PATH];

			//Fetch the System Drive(first get sys dir and then extract drive letter from it)
			if ( 0 == GetSystemDirectory(szSysPath,MAX_PATH) )
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to fetch System Directory\n");
				return FALSE;
			}
			if ( 0 == GetVolumePathName(szSysPath,szRootVolumeName,MAX_PATH) )
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get Volume Pathname of System Directory\n");
				return FALSE;
			}

			//Create the Parent Mount directory
			string MountPath = string(szRootVolumeName) + "InMageEsx\\"; //end "\" is must
			if (FALSE == CreateDirectory(MountPath.c_str(),NULL))
			{
				if(GetLastError() != ERROR_ALREADY_EXISTS)// To Support Rerun scenario
				{
					DebugPrintf(SV_LOG_ERROR,"CreateDirectory Operation Failed with ErrorCode : %d \n",GetLastError());
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return FALSE;
				}
			}
			
			//Create the Child mount directory 
			//  Say mount1 then check if it has reparse points then create mount2
			//  check again and continue this until a directory which got no reparse points
			char cMntCount[15];
			itoa(MntCount++,cMntCount,10); //last argument 10 since MntCount is in decimal form
			string ChildMntPath = MountPath + "mount" + string(cMntCount) + string("\\");
			while(FALSE == CreateDirectory(ChildMntPath.c_str(),NULL))
			{
				if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
				{
					DebugPrintf(SV_LOG_ERROR,"Child Directory already exists. ");
					//Check whether the folder doesnot have any reparse points
					DWORD File_Attribute;
					File_Attribute = GetFileAttributes(ChildMntPath.c_str());
					if (File_Attribute & FILE_ATTRIBUTE_REPARSE_POINT)
					{
						DebugPrintf(SV_LOG_ERROR,"Its in use. So creating another directory.\n");
						itoa(MntCount++,cMntCount,10);
						ChildMntPath = MountPath + "mount" + string(cMntCount) + string("\\");
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Considering it not as Error\n");
						break;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"CreateDirectory Operation Failed - ErrorCode : %d \n",GetLastError());
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return FALSE;
				}
				
			}

			//Mount the Volume 
			if (FALSE == SetVolumeMountPoint(ChildMntPath.c_str(),string(VolGUID).c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to mount the Volume: %d \n",GetLastError());
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return FALSE;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Successfully mounted this Volume to : %s\n",ChildMntPath.c_str());
			}			
		}
		bFlag = FindNextVolume(volumeHandle,VolGUID,BUFSIZE);		
	}

	// Close out the volume scan.
	FindVolumeClose(volumeHandle);  // handle to be closed

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return TRUE;
}

//Check whether the volume is mounted or not
BOOL Source::IsVolumeNotMounted(TCHAR *VolGUID)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strVolumeGuid = string(VolGUID);
	char szVolumePath[MAX_PATH+1] = {0};
	DWORD dwVolPathSize = 0;

	if (GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
	{
		if (string(szVolumePath).empty()) //if volume path name is empty, then it is not mounted.
		{
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return TRUE;			
		}
		else
		{
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return FALSE;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : %d \n",GetLastError());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return FALSE;
	}	
}

//Write the map of Disk Number and its corresponding SCSI Port ID and Target ID to a file
//Sample info - scsi id - 1:15 , Disk 0 
//File format - 1:15!@!@!2   where !@!@! is delimiter 
bool Source::PersistDiskAndScsiIDs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    string strMapDiskAndScsiIDsFile = m_strInstallDirectoryPath +  string("\\Failover") + string("\\data\\") + stGetHostName() + string(SCSI_DISK_FILE_NAME);
    ofstream outFile(strMapDiskAndScsiIDsFile.c_str());
    if(! outFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with Error : [%lu].\n",strMapDiskAndScsiIDsFile.c_str(),GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);            
		return FALSE;
    }
    map<string,string>::iterator iterDS  = m_mapOfDeviceIdAndControllerIds.begin();    
    while (iterDS != m_mapOfDeviceIdAndControllerIds.end())
    {
        //iterDS->second is drive name - ex: \\.\PHYSICALDRIVE5
        //so the disk number is at end.
        string strDiskNmbr = iterDS->second.substr(m_strDiskName.size());
        if(strDiskNmbr.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"PersistDiskAndScsiIDs::Found emtpy Disk Number\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            outFile.close();
			return FALSE;
		}
        string strToBeWrittenOnFile = iterDS->first + string("!@!@!") + strDiskNmbr;
        outFile<<strToBeWrittenOnFile<<endl;
        iterDS++;
    }
    outFile.close();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return TRUE;
}

//Generate tags for source replicated volumes
// 1. Fetch the source replicated volumes.
// 2. Execute the consistency script to generate tags.
//Input - Target Hostname, Tag Name, bCrashConsistency
bool Source::GenerateTagForSrcVol(string strTarget, string strTagName, bool bCrashConsistency, Configurator* TheConfigurator)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
    if(0 != getAgentInstallPath())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find the Agent Install Path.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    std::string ts = GeneratreUniqueTimeStampString();
    std::string FinalTagName = strTagName + ts;
    //This will have list of replicated source volumes in the following format.
    //Ex -  "C:;E:;C:\Mount Points\MP1;"
    string strSrcRepVol = "all";
    /*if(false == GetSrcReplicatedVolumes(strTarget,strSrcRepVol,TheConfigurator))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the source replicated volumes.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }*/
    if (false == ExecuteVMConsistencyScript(strSrcRepVol,FinalTagName,bCrashConsistency))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed while executing the VM Consistency Script.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//Get Replicated Volumes on the source
//Input - strTarget (target hostname)
//Output - List of Replicated Volumes on Source with the given target in required format("C:;E:;C:\Mount Points\MP1;").
bool Source::GetSrcReplicatedVolumes(string strTarget, string &strSrcRepVol, Configurator* TheConfigurator)
{ 
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    DebugPrintf(SV_LOG_INFO,"Fetching the volumes replicated from this machine to target machine : %s.\n",strTarget.c_str());
    //Fetch the replication pairs from Configurator object
    map<string,string> mapVxPairs = TheConfigurator->getSourceVolumeDeviceNames(strTarget);    
    
    strSrcRepVol.clear();
    map<string,string>::iterator iter = mapVxPairs.begin();    
    for(;iter != mapVxPairs.end();iter++)
    {
        DebugPrintf(SV_LOG_INFO,"Source Vol : %s <==> Target Vol : %s \n",iter->second.c_str(),iter->first.c_str());
        //if the volume is drive letter add : (colon) to it.
        strSrcRepVol += string(iter->second) + (iter->second.length()==1 ? ":;" : ";");
    }

    if(strSrcRepVol.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"No Volumes are replicated on this source with the target machine : %s.\n",strTarget.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    else
        strSrcRepVol = "\"" + strSrcRepVol + "\"";

    DebugPrintf(SV_LOG_INFO,"Source volumes under replication = %s\n",strSrcRepVol.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Run the bat script for generating tags using ACE Process
//Input - Source Volumes in required format, Tag Name and is Crash Consistent or not
//Returns true if "All is Well"
bool Source::ExecuteVMConsistencyScript(string strSrcRepVol, string strTagName, bool bCrashConsistency)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
    string strFileName = m_strInstallDirectoryPath + string("\\") + CONSISTENCY_DIRECTORY + string("\\") + VM_CONSISTENCY_SCRIPT;
		
    //create an ACE process to run the generated script
	ACE_Process_Options options; 
	options.handle_inheritance(false);

    if(bCrashConsistency)
    {
        options.command_line("\"%s\" %s \"%s\" %s", strFileName.c_str(), strSrcRepVol.c_str(), strTagName.c_str(), OPTION_CRASH_CONSISTENCY);
    }
    else
    {
        options.command_line("\"%s\" %s \"%s\"", strFileName.c_str(), strSrcRepVol.c_str(), strTagName.c_str());
    }


	ACE_Process_Manager* pm = ACE_Process_Manager::instance();
	if (pm == NULL) {
		DebugPrintf(SV_LOG_ERROR,"Failed to get ACE Process Manager instance.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    pid_t pid = pm->spawn (options);
    if (pid == ACE_INVALID_PID) {
        DebugPrintf(SV_LOG_ERROR,"Failed to create ACE Process for executing  file : %s.\n",strFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

	// wait for the process to finish
	ACE_exitcode status = 0;
	pid_t rc = pm->wait(pid, &status);
	DebugPrintf(SV_LOG_INFO,"ACE Exit code status : [%d].\n",status);
	if (status == 0) {
		DebugPrintf(SV_LOG_INFO,"Successfully generated the file system consistency tag.\n");
	}
	else {
		int iError = ACE_OS::last_error();
        DebugPrintf(SV_LOG_ERROR,"Failed to generate the tag with Error : [%s].\n",ACE_OS::strerror(iError));
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

void Source::alterDiskConfig(std::map<unsigned int,unsigned int>& DiskPartMap)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    vector<DiskInformation>::iterator &diskIter = disk_vector.begin();
	while(diskIter != disk_vector.end())
	{
		std::map<unsigned int,unsigned int>::iterator mapIter;
		mapIter =  DiskPartMap.find((*diskIter).uDiskNumber);
		if(mapIter != DiskPartMap.end())
		{
			DebugPrintf(SV_LOG_INFO,"Disk number = %u\n",(*diskIter).uDiskNumber);
			DebugPrintf(SV_LOG_INFO,"Written Partition Count = %lu\n",(*diskIter).dwActualPartitonCount);
            DebugPrintf(SV_LOG_INFO,"Actual Partition Count = %u\n",mapIter->second);
			(*diskIter).dwActualPartitonCount = mapIter->second;
			DebugPrintf(SV_LOG_INFO,"After change the partition Count = %lu\n",(*diskIter).dwActualPartitonCount);
			//need break
		}
		diskIter++;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//Function to get its description ID
//Input - Path of folder
//Output - ID (pdid)
HRESULT Source::GetFolderDescriptionId(LPCWSTR pszPath, SHDESCRIPTIONID *pdid)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HRESULT hr;
    LPITEMIDLIST pidl;

    if (SUCCEEDED(hr = SHParseDisplayName(pszPath, NULL,&pidl, 0, NULL)))
    {
        IShellFolder *psf;
        LPCITEMIDLIST pidlChild;
        if (SUCCEEDED(hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&psf, &pidlChild)))
        {
            hr = SHGetDataFromIDList(psf, pidlChild,SHGDFIL_DESCRIPTIONID, pdid, sizeof(*pdid));
            psf->Release();
        }
        CoTaskMemFree(pidl);
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return hr;
}

//Function to get the parent directory of given file.
//Input - strsrc file
//Output - ostrdst (parent directory)
//returns true if it successfully fetched the parent directory.
bool Source::GetParentDirectory(std::string& ostrdst, const std::string& strsrc)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
        
    ostrdst = strsrc;
    ostrdst += "\\..";

    std::vector<char> vbuffer(MAX_PATH + 1, '\0'); 
    if (0 == GetFullPathName(ostrdst.c_str(), vbuffer.size(), &vbuffer[0], NULL))
    {   
        DWORD dwerrno = GetLastError();
        DebugPrintf(SV_LOG_ERROR, "Failed to fetch the Parent directory for %s with Error - [%lu].\n", ostrdst.c_str(), dwerrno);
        rv = false;
    }
    else
    {
        ostrdst = vbuffer.data();
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// Process all the parent dirs in given mount point
// check if anyof them is recycle bin
// if it is, then mount point is in recycle bin
// return true if the mount point is not in recycle bin
//  else  false in all other cases.
bool Source::ProcessAllParentPathsForRecycleBin(char InputPath[])
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    USES_CONVERSION;
    bool rv = true;
    std::string strCurrPath(InputPath);
    std::wstring wstrPath;
    std::string strParentPath;

    while(true)
    {
        DebugPrintf(SV_LOG_INFO, "currPath = %s\n", strCurrPath.c_str());
        wstrPath = A2W(strCurrPath.c_str());

        SHDESCRIPTIONID did;
        if (SUCCEEDED(GetFolderDescriptionId(wstrPath.c_str(), &did)) && did.clsid == CLSID_RecycleBin)
        {
            DebugPrintf(SV_LOG_ERROR, " The volume mount point[%s] is in Recycle Bin(%s).\n", InputPath, W2A(wstrPath.c_str()));
            DebugPrintf(SV_LOG_ERROR," Please clear the mount point and then rerun this job.\n\n");
            rv = false;
            break;
        }     
        if (false == GetParentDirectory(strParentPath, strCurrPath))
        {
            rv = false;
            break;
        }
        //if parent directory is same as given directory, stop because we have reached drive letter.
        // and hence no more parent directories to it.
        //otherwise we can also check for number of letters <= 3 (if we reached drive).
        if (strParentPath.length() > 3)
            strCurrPath = strParentPath;
        else
            break;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Function to check whether the mount points are not in RecycleBin
bool Source::CheckMntPathsExistInRecycleBin()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    map<std::string,std::string>::iterator iterVol = m_mapOfGuidsWithLogicalNames.begin();

    for(; iterVol!=m_mapOfGuidsWithLogicalNames.end(); iterVol++)
    {
        char VolPath[MAX_PATH + 1];
        inm_strcpy_s(VolPath, ARRAYSIZE(VolPath), iterVol->second.c_str());
        //Process the volume if it is a mount point 
        if(strlen(VolPath) > 3)
        {
            DebugPrintf(SV_LOG_INFO,"Processing the mount point - %s\n",VolPath);
            if(false == ProcessAllParentPathsForRecycleBin(VolPath))
            {
                rv = false;                
            }      
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// --------------------------------------------------------------------------
// Enable the file mode for system volume (C:) : [Bug - 13874]
// Implementing as suggested below.
//      1. Detect if a source VM is 32bit or not
//      2. Detect if source VM has equal or more than 2 volumes or not
//      3. Verify whether space in system vol is more than 512MB
//      4. If above conditions are met, enable the data file mode automatically.
// Return true if succeeds in enabling file mode when above conditions meet.
// Else return fail.
// --------------------------------------------------------------------------
bool Source::EnableFileModeForSysVol()
{  
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    bool b64Bit = bIsOS64Bit();
    int iVolCnt = m_mapOfGuidsWithLogicalNames.size();
    DebugPrintf(SV_LOG_INFO,"Total number of volumes : %d\n",iVolCnt);

    if ( (false == b64Bit) && (iVolCnt >= 2) )
    {
        DebugPrintf(SV_LOG_INFO,"Enabling the File mode for system volume.\n");
        string strSysVol;
        if( false == GetSystemVolume(strSysVol) )
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to fetch the System volume for enabling file mode.\n");
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    		return false;
        }

        //check whether free space in sys vol is greater than 512MB(512*1024*1024 bytes = 536870912 bytes)
        ULARGE_INTEGER uliQuota = {0};
        ULARGE_INTEGER uliTotalCapacity = {0};
        ULARGE_INTEGER uliFreeSpace = {0};
        if (false == GetDiskFreeSpaceEx(strSysVol.c_str(), &uliQuota, &uliTotalCapacity, &uliFreeSpace ))
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to get free space on volume %s ... Error - [%lu]\n",strSysVol.c_str(),GetLastError());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        DebugPrintf(SV_LOG_INFO,"\nFree space on system volume %s = %llu bytes\n",strSysVol.c_str(),uliFreeSpace.QuadPart);
        if (uliFreeSpace.QuadPart < 536870912) //512MB
        {
            DebugPrintf(SV_LOG_WARNING,"Minimum required free space = 512MB (or) 536870912 bytes\n");
            DebugPrintf(SV_LOG_WARNING,"Free Space on system volume %s is less than minimum required to enable the file mode.\n",strSysVol.c_str());            
            DebugPrintf(SV_LOG_WARNING,"\nTODO: Provide the sufficient free space(>512MB) in system volume and rerun the Fx job for protection.\n\n");
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        
        //formating drive letter as Cx format, but this is for drvutil command.
        FormatVolumeNameForCxReporting(strSysVol);
        DebugPrintf(SV_LOG_INFO,"System volume - %s\n",strSysVol.c_str());

        string strDrvutilExe = m_strInstallDirectoryPath + string("\\") + DRVUTIL_EXE;
        
        //First Command Options to enable file mode for the driver
        //Example - drvutil --setdriverflags -disabledatafiles -resetflags
        string strParameters = string("--setdriverflags -disabledatafiles -resetflags");
        DebugPrintf(SV_LOG_INFO, "Command : \"%s\" %s\n", strDrvutilExe.c_str(), strParameters.c_str());
        if( false == ExecuteCommandUsingAce(strDrvutilExe, strParameters) )
        {
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }

        //Second Command Options to enable for a volume(system volume here)
        //Example - drvutil --setvolumeflags C -disabledatafiles -resetflags
        strParameters = string("--setvolumeflags ") + strSysVol + string(" -disabledatafiles -resetflags");
        DebugPrintf(SV_LOG_INFO, "Command : \"%s\" %s\n", strDrvutilExe.c_str(), strParameters.c_str());
        if( false == ExecuteCommandUsingAce(strDrvutilExe, strParameters) )
        {
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }

        DebugPrintf(SV_LOG_INFO,"Enabled the File mode for system volume - %s\n\n",strSysVol.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

// --------------------------------------------------------------------------
// Check whether the OS is 64 bit
// Return true if 64 bit.
// --------------------------------------------------------------------------
bool Source::bIsOS64Bit()
{
    bool bIs64Bit = false;

    BOOL bIsWow64 = FALSE;
    LPFN_ISWOW64PROCESS fnIsWow64Process;

    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
  	if (NULL != fnIsWow64Process)
    {
        if (fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            if(!bIsWow64)
            {
                DebugPrintf(SV_LOG_INFO,"OS is 32 bit.\n");
                bIs64Bit = false;
            }       
            else
            {
                DebugPrintf(SV_LOG_INFO,"OS is 64 bit.\n");
                bIs64Bit = true;
            }
        }
    }
    return bIs64Bit;
}

// --------------------------------------------------------------------------
// Fetch the system drive (say C:\ - it will be in this format).
// --------------------------------------------------------------------------
bool Source::GetSystemVolume(string & strSysVol)
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
    strSysVol = string(szSysDrive);
    DebugPrintf(SV_LOG_INFO,"System Volume - %s\n",strSysVol.c_str());

    return true;
}

//*********************************************************************************
//  Delete all the files and folders in given folder path recursively
//*********************************************************************************
bool Source::DeleteAllFilesInFolder(std::string FolderPath)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    std::string AllFiles = FolderPath + std::string("\\*");
    hFind = FindFirstFile(AllFiles.c_str(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) 
    {
        DebugPrintf(SV_LOG_ERROR,"FindFirstFile() failed  - (%s)  [Error code - %lu]\n",AllFiles.c_str(), GetLastError());
        rv = false;
    } 
    else 
    {
        do
        {
            if(0 == strcmp(FindFileData.cFileName, "."))
            {
                continue;
            }
            else if(0 == strcmp(FindFileData.cFileName, ".."))
            {
                continue;
            }
            else if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                std::string FullPathToFile = FolderPath + std::string("\\") + FindFileData.cFileName;
                DebugPrintf(SV_LOG_INFO,"  %s        :  <DIR>\n", FullPathToFile.c_str());
                if(false == DeleteAllFilesInFolder(FullPathToFile))
                {
                    rv = false;
                }
                else
                {
                    if(0 == RemoveDirectory(FullPathToFile.c_str()))
                    {
                        DebugPrintf(SV_LOG_ERROR,"RemoveDirectory failed - (%s). Error code - %lu\n", FullPathToFile.c_str(), GetLastError());
                        rv = false;
                    }
                }
            }
            else
            {
                std::string FullPathToFile = FolderPath + std::string("\\") + FindFileData.cFileName;
                DebugPrintf(SV_LOG_INFO,"  %s\n", FullPathToFile.c_str());
                if(0 == DeleteFile(FullPathToFile.c_str()))
                {
                    DebugPrintf(SV_LOG_ERROR,"DeleteFile failed - (%s). Error code - %lu\n", FullPathToFile.c_str(), GetLastError());
                    rv = false;
                }
            }
        }while (FindNextFile(hFind, &FindFileData) != 0);
    }
    FindClose(hFind);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}