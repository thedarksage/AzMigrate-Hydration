#include "stdafx.h"
#include "Windows.h"
#include "MapDriveLetter.h"
#include "inmsafecapis.h"

#define MOUNT_UNMOUNT_DELAY 2000

/*#ifdef UNICODE	
	#define UInt8 unsigned char  
	#define UIntType unsigned int 
	#define VOLUME_GUID_SIZE_FIELD	76
	#define MNTPNTPATH_SIZE_FIELD 4
	#define MAX_POSSIBLE_PATH 65536
#else*/
	#define UInt8 unsigned char  
	#define UIntType unsigned short 
	#define VOLUME_GUID_SIZE_FIELD	48
	#define MNTPNTPATH_SIZE_FIELD 2
	#define MAX_POSSIBLE_PATH 32768
//#endif

using namespace std;

typedef map<string,string> mapStrStrMntPntPathVolGuid;
typedef map<string,VolumeInfo>mapStrVolGuidVolumeInfo;

mapStrStrMntPntPathVolGuid mapFileMPPathVolGuid;
mapStrStrMntPntPathVolGuid mapMemoryMPPathVolGuid;
mapStrVolGuidVolumeInfo	   mapStrVolInfoFromFile;
mapStrVolGuidVolumeInfo	   mapStrVolInfoFromMemory;

vector<VolumeInfo>vecDriveLetter;//A vector to store the information of Volumes
TCHAR szRootPathName[MAX_PATH] = {0};

//a File Stream Object to Write and Read the Volume Information
//basic_fstream<unsigned char,char_traits<unsigned char> >MDLInfoFile;
//fstream MDLInfoFile;
HANDLE hMDLFile;

#ifndef _CLUSUTIL_LOG
	//#define PRINTF printf
#else
	//#define printf PRINTF
	/*extern int PRINTF(char* lpszText);
	extern int PRINTF(char* lpszText,int nVal);
	extern int PRINTF(char* lpszText,char* lpszMoreText);
	extern int PRINTF(char* lpszText,int nVal,LPCTSTR lpszMoreText);
	extern int PRINTF(char* lpszText,char* lpszMoreText, char* lpszEvenMoreText,int nVal);
	extern int PRINTF(char* lpszText,char* lpszMoreText, char* lpszEvenMoreText);
	extern int PRINTF(char* lpszText,char* lpszMoreText, int nVal);*/
#endif

BOOL MapDriveLetterMain(int argc,char* argv[])
//BOOL main(int argc,char* argv[])
{
   TCHAR buf[BUFSIZE];      // buffer for unique volume identifiers
   //char buf[BUFSIZE];      // buffer for unique volume identifiers
   HANDLE volumeHandle;     // handle for the volume scan
   BOOL bFlag;              // generic results flag
   BOOL bSource = FALSE;
   BOOL bTarget = FALSE;
   int iCmdLineArg = 2;

   for(; iCmdLineArg<argc; iCmdLineArg++)
   {
		if(strcmpi(argv[iCmdLineArg],OPTION_SOURCE_HOST) == 0)
		{
			bSource = TRUE;	
			break;
	    }
		else if(strcmpi(argv[iCmdLineArg],OPTION_TARGET_HOST) == 0)
		{
			bTarget = TRUE;			
			break;
		}
		else
		{
			LogAndPrint("\nUsage:Please specify -s option if you are running at the Source Side or specify -t if you are running it on the Target Machine e.g.\n");
			LogAndPrint("\n MapDriveLetter.exe -s\n");
			LogAndPrint("\n MapDriveLetter.exe -t\n");
			return -1;
		}
   }
   if((bSource == TRUE) && (bTarget == TRUE))
   {
		LogAndPrint("\n Please specify Host.Use \"-s\" for Souce Host or \"-t\" for Target Host.Please dont use both options together\n");
		return -1;
   }
   if((bSource == FALSE) && (bTarget == FALSE))
   {
		LogAndPrint("\n Please specify Host.Use \"-s\" for Souce Host or \"-t\" for Target Host.\n");
		return -1;
   }

	//From here we'll start scanning of Volumes present in the system.
   // Open a scan for volumes.
   volumeHandle = FindFirstVolume (buf, BUFSIZE );

   if (volumeHandle == INVALID_HANDLE_VALUE)
   {
      LogAndPrint("No volumes found!\n\n");
      return (-1);
   }

   // We have a volume,now process it.
   bFlag = EnumerateVolumes (volumeHandle, buf, BUFSIZE,FALSE);

   // Do while we have volumes to process.
   while (bFlag) 
   {
      bFlag = EnumerateVolumes (volumeHandle, buf, BUFSIZE,FALSE);
   }

   // Close out the volume scan.
   bFlag = FindVolumeClose(volumeHandle);  // handle to be closed

   //Remove the System Volume from the vector
   if(!RemoveSystemVolumeFromUnMountList())
   {
	   LogAndPrint("\n Error removing the System Volume from the UnMounted Volumes List.Exiting");
	   return FALSE;
   }
    

   if(bSource)
   {
	   //Open File in Write Mode
	   hMDLFile = CreateFile(	DRIVES_LETTER_FILE_NAME,
								GENERIC_WRITE,
								0,
								NULL,
								CREATE_ALWAYS,
								NULL,
								NULL);
	if(INVALID_HANDLE_VALUE == hMDLFile)
	{
		LogAndPrint("\nError in Creating the file %s to store the Drive Letter Information.\n",DRIVES_LETTER_FILE_NAME);
		return FALSE;
	}
	if(FALSE == PersistDriveLetterInfo())
	{
		//Error Messege		   
	}
	//Close File
	CloseHandle(hMDLFile);
   }
   if(bTarget)
   {
	   //Open File in Read Mode
	   //Open File in Write Mode
	   hMDLFile = CreateFile(	DRIVES_LETTER_FILE_NAME,
								GENERIC_READ,
								0,
								NULL,
								OPEN_ALWAYS,
								NULL,
								NULL);
	if(INVALID_HANDLE_VALUE == hMDLFile)
	{
		LogAndPrint("\nError in Reading the file %s to retrieve the Drive Letter Information.\n",DRIVES_LETTER_FILE_NAME);
		return FALSE;
	}
	//Get the return Value of HandleTarget()
	   
	  SetErrorMode(SEM_FAILCRITICALERRORS);//To ignore the display of "No Disk" when information retrieved from CDROM without disk
	   
	  bFlag = HandleTarget();
	  SetErrorMode(0);//Restore the Error Mode of the process
	   
	  //Close File
	  CloseHandle(hMDLFile);
   }
  return (bFlag);
}

/* Process each volume.The Boolean result indicates whether there is another
 volume to be scanned.*/
BOOL EnumerateVolumes (HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, BOOL bIncludeRemovableDrives)
//BOOL EnumerateVolumes (HANDLE volumeHandle, char *szBuffer, int iBufSize, BOOL bIncludeRemovableDrives)
{
   BOOL bFlag = TRUE;					// generic results flag for return
  
   char szVolumePath[MAX_PATH+1] = {0};
   char *pszVolumePath = NULL;//new char(MAX_PATH);

   DWORD dwVolPathSize = 0;
   string strVolumeGuid = string(szBuffer);

   //Create a VolumeGuid structure
   VolumeInfo* pVolInfo = new VolumeInfo();
   if(!pVolInfo)
   {
	   LogAndPrint("\n Error occurred in creating a Volume Guid object. Hence exiting...\n");
	   return FALSE;
   }
   pVolInfo->strVolGuid = strVolumeGuid;//assign the Volume Guid

   //Get all the list of Mount Point Paths for this Volume
#ifndef SCOUT_WIN2K_SUPPORT
	    if(GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
		{
			//parse the szVolumePath to get a list of null terminated strings as Mount Point Paths
			vector<string>vstrMountPointPaths;
			ExtractMountPointPaths(szVolumePath,vstrMountPointPaths);			
			pVolInfo->vMountPoints = vstrMountPointPaths;
			//Add this VolumeInfo object to the global MDLInfo object
			vecDriveLetter.push_back(*pVolInfo);
		}
		else
		{
			DWORD dwErr = GetLastError();
			if(ERROR_MORE_DATA == dwErr)//Failed because of insufficient buffer to store output data
			{
				pszVolumePath = new char[dwVolPathSize];
				if(!pszVolumePath)
				{
					LogAndPrint("\n Failed to allocate required memory to get the list of Mount Point Paths.");
					return FALSE;
				}
				else
				{
					//Call GetVolumePathNamesForVolumeName function again
					if(GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),
														pszVolumePath,
														dwVolPathSize,
														&dwVolPathSize))
					{
						//If this time is a success
						//then store the Mount Point Paths
						//parse the szVolumePath to get a list of null terminated strings as Mount Point Paths
						vector<string>vstrMountPointPaths;
						ExtractMountPointPaths(szVolumePath,vstrMountPointPaths);			
						pVolInfo->vMountPoints = vstrMountPointPaths;
						//Add this VolumeInfo object to the global MDLInfo object
						vecDriveLetter.push_back(*pVolInfo);
					}
				}
			}
			else
			{
				LogAndPrint("\n GetVolumePathNamesForVolumeName Failed with ErrorCode: %d",GetLastError());
				return FALSE;
			}
		}
#endif
		// Stop processing mount points on this volume. 
   bFlag = FindNextVolume(volumeHandle,// handle to scan being conducted
						  szBuffer,    // pointer to output
						  iBufSize);   // size of output buffer

   return (bFlag); 
}

	//Extract the Mount Point Paths from a list which is an array of null-terminated strings
	//terminated by an additional NULL character
void ExtractMountPointPaths(LPTSTR lpszMountPointPaths,vector<string> &strPaths)
{
	TCHAR *ptChar = lpszMountPointPaths;
	//char *ptChar = lpszMountPointPaths;
	TCHAR* pPostChar = lpszMountPointPaths;
	//char* pPostChar = lpszMountPointPaths;
	do
	{
		while((*pPostChar) != NULL)
		{	
			pPostChar++;
		}
		
		if((*ptChar) != NULL)
		{
			string strPath = ptChar;
			//cout<<endl<<"Processed "<<strPath.c_str();
			strPaths.push_back(strPath);			
		}
		pPostChar++;
		ptChar = pPostChar;
	}while((*pPostChar) != NULL);	
}
BOOL PersistDriveLetterInfo()
{
	vector<VolumeInfo>::iterator iter = vecDriveLetter.begin();
	LogAndPrint("\n Persisting Drive Letter Information to the ClusUtil.log File during -s operation:\n");
	LogAndPrint("------------------------------------------------------------------------------------\n");
	try{
	while(iter != vecDriveLetter.end())
	{
		//Repeat the following process for every MountPointPath obtained
		//Size Filed		: First Two Fields
		//MountPointPath	: The Mount Point Path equal to the size obtained in the previous two bytes
		//VolumeGUID		: The 49 byte Volume GUID of the Volume
		VolumeInfo volInfo = (*iter);				
		vector<string>::iterator iter_index = volInfo.vMountPoints.begin();
		while(iter_index != volInfo.vMountPoints.end())
		{
			//Assign the MountPoint
			string strMntPnt = (*iter_index);							
			UIntType lenOfMntPntPath = (UIntType)(strMntPnt.length());
			DWORD dwBytesWritten = 0;
			//UIntType lenOfMntPntPath = (UIntType)(strMntPnt.size())+ 1;//Include NULL character
			if(!WriteFile(hMDLFile,(LPVOID)&lenOfMntPntPath,MNTPNTPATH_SIZE_FIELD,&dwBytesWritten,0))//2 bytes for Length Field		
			{
				LogAndPrint("\n Error writing to the file...");
				return FALSE;//MCHECK
			}
			else if(dwBytesWritten != MNTPNTPATH_SIZE_FIELD)
			{
				LogAndPrint("\n Error writing data to the file...");
				return FALSE;//MCHECK
			}
			
			if(!WriteFile(hMDLFile,(LPVOID)strMntPnt.c_str(),lenOfMntPntPath,&dwBytesWritten,0))//Write the actual MntPnt Path
			{
				LogAndPrint("\n Error writing to the file...");
				return FALSE;//MCHECK
			}
			else if(lenOfMntPntPath != dwBytesWritten)
			{
				LogAndPrint("\n Error writing data to the file...");
				return FALSE;//MCHECK
			}
			
			string strTempVolGuid = volInfo.strVolGuid;
			string strVolGuid;
			ProperlyEscapeSpecialChars(strTempVolGuid,strVolGuid);
			
			//if(!WriteFile(hMDLFile,(LPVOID)strVolGuid.c_str(),(DWORD)strVolGuid.length(),&dwBytesWritten,0))//Volume Guid
			if(!WriteFile(hMDLFile,(LPVOID)strVolGuid.c_str(),(DWORD)VOLUME_GUID_SIZE_FIELD,&dwBytesWritten,0))//Volume Guid
			{
				LogAndPrint("\n Error writing to the file...");
				return FALSE;//MCHECK
			}
			//else if(strVolGuid.length() != dwBytesWritten)
			else if(VOLUME_GUID_SIZE_FIELD != dwBytesWritten)
			{
				LogAndPrint("\n Error writing data to the file...");
				return FALSE;//MCHECK
			}
			
			LogAndPrint("\n\tVolume Guid:  %s \tVolume Mount Point:  %s\n",(char*)strVolGuid.c_str(),(char*)strMntPnt.c_str());
			iter_index++;//increment the iterator to go to the next Mount Point Path
		}//end of inner while loop

		iter++;//go to the next Volume
	}
	}catch(...)
	{
		cout<<endl<<"Caught a generic exception";
	}
	LogAndPrint("\n Successfully persisted all the Drive Letters information.");
	return TRUE;
}

BOOL HandleTarget()
{	
	BOOL bStatus = TRUE;
	BOOL bChanged = FALSE;
	LogAndPrint("\n Mapping Correct Drive Letters Information to the ClusUtil.log File during -t operation:\n");
	LogAndPrint("------------------------------------------------------------------------------------------\n");
	//Create a map containing MntPointPaths and VolumeGuids
	bStatus = BuildMapFromFile();
	if(!bStatus)
	{
		LogAndPrint("\n Error in creating a map by reading the file %s\n",DRIVES_LETTER_FILE_NAME);
		return bStatus;
	}

	//Create one more map containing the current MountPointPaths and Volume GUIDs
	bStatus = BuildMapFromMemory();//this contains newly added disks information also
	if(!bStatus)
	{
		LogAndPrint("\n Error in creating a map from memory.\n");
		return bStatus;
	}
	BOOL bOldVolumesRemoved = FALSE;
	BOOL bNewVolumesAdded   = FALSE;
	
	LogAndPrint("\n\n\tWARNING: If any new volumes are added and if they clash with the \n\toriginal cofiguration of drive letters,then those volumes will be Un-Mounted!\n\n");

	mapStrStrMntPntPathVolGuid::iterator posChangedMap  = mapMemoryMPPathVolGuid.begin();
	//BOOL bProcessed = FALSE;
	//while(!bProcessed)
	//{
		//bProcessed = TRUE;
		while(posChangedMap != (mapMemoryMPPathVolGuid.end()))
		{	
			string strChangedMntPath = (*posChangedMap).first;
			string strChangedVolGuid = (*posChangedMap).second;

			mapStrStrMntPntPathVolGuid::iterator posOriginalMap = mapFileMPPathVolGuid.begin();

			while(posOriginalMap != mapFileMPPathVolGuid.end())
			{	
				string strOriginalMntPath = (*posOriginalMap).first;
				string strOriginalVolGuid = (*posOriginalMap).second;
				if((strChangedVolGuid == strOriginalVolGuid) && (strChangedMntPath != strOriginalMntPath))
				//The below condition considers even the new disks that are added but clashing with the old Drive Letters
				//it will effectively unmount the new disks if their drive letters clash with the original configuration
				//if((strChangedVolGuid != strOriginalVolGuid) && (strChangedMntPath == strOriginalMntPath))
				{
					//First Un-Mount the new Volume Mount Point recursively
					if(!MountUnMountTheRootVolume((LPCTSTR)strChangedMntPath.c_str(),TRUE))
					{
						bStatus = FALSE;
						break;
					}					
					if(!MountUnMountTheRootVolume((LPCTSTR)strOriginalMntPath.c_str(),TRUE))
					{
						bStatus = FALSE;
						break;
					}
					//Mount all the relevant Changed Volume Mount Points recursively from Bottom Up					
					if(!MountUnMountTheRootVolume((LPCTSTR)strChangedMntPath.c_str(),FALSE))
					{
						bStatus = FALSE;
						break;
					}
					//Mount all the relevant Original Volume Mount Points recursively from Bottom Up					
					if(!MountUnMountTheRootVolume((LPCTSTR)strOriginalMntPath.c_str(),FALSE))
					{
						bStatus = FALSE;
						break;
					}
					bChanged = TRUE;
					
				}
				posOriginalMap++;
			}//end of inner while loop
			BuildMapFromMemory();
			posChangedMap++;
		}//end of Outer while loop
	//}//end of outermost while loop;


	//Madan
	//Compare the two maps containg Volume Guids and the Volume Mount Point Paths
	//Check if any Volume Mount Point Path for a Volume is Changed.
	//If any Volume is changed then ensure that all the parent Volume Mount Point Paths are Mounted before
	//If any new volume is added then mount that volume at the end
	UINT OldVolGuidCount = (UINT)mapStrVolInfoFromFile.size();
	UINT NewVolGuidCount = (UINT)mapStrVolInfoFromMemory.size();
	
	//The possibility is some old Volumes were remvoed and the same number of new volumes were added
	//identify the Old Volumes which were removed
	mapStrVolGuidVolumeInfo::iterator iterMapOldVolGuid = mapStrVolInfoFromFile.begin();
	while(iterMapOldVolGuid != mapStrVolInfoFromFile.end())
	{
		string strOldVolGuid = (*iterMapOldVolGuid).first;
		vector<string> vstrOldPaths = (*iterMapOldVolGuid).second.vMountPoints;		
		mapStrVolGuidVolumeInfo::iterator iterMapNewVolGuid = mapStrVolInfoFromMemory.find(strOldVolGuid);
		if(iterMapNewVolGuid == mapStrVolInfoFromMemory.end())
		{	
			LogAndPrint("\n The Volume %s is not found in the current system configuration.\n",(char*)strOldVolGuid.c_str());			
			bOldVolumesRemoved = TRUE;
			//LogAndPrint("\n Since an old volume was removed, MapDriveLetters cannot proceed.");
			LogAndPrint("\n Please add the removed volume to successfully set all replication pairs!.");
			//return FALSE;
			bOldVolumesRemoved = TRUE;
		}
		iterMapOldVolGuid++;
	}

	//Check if any new disks/Volumes are added in the system
	mapStrVolGuidVolumeInfo::iterator iterMapNewVolGuid = mapStrVolInfoFromMemory.begin();	
	while(iterMapNewVolGuid != mapStrVolInfoFromMemory.end())
	{
		string strNewVolGuid = (*iterMapNewVolGuid).first;
		vector<string>vstrNewPaths = (*iterMapNewVolGuid).second.vMountPoints;		
		mapStrVolGuidVolumeInfo::iterator iterMapOldVolGuid = mapStrVolInfoFromFile.find(strNewVolGuid);
		if(iterMapOldVolGuid == mapStrVolInfoFromFile.end())
		{
			LogAndPrint("\n The Volume Guid %s is newly added in the current system configuration.\n",(char*)strNewVolGuid.c_str());
			bNewVolumesAdded = TRUE;
			//LogAndPrint("\n Since a new volume was added, MapDriveLetters cannot proceed.");
			//LogAndPrint("\n Please remove the new volume(s) to execute the MapDriveLetters.exe.");
			LogAndPrint("\nWARNING:Please check as this volume might have been Un-Mounted if it clashes with the original volumes drive letters!");
			//return FALSE;
			bNewVolumesAdded = TRUE;
		}
		iterMapNewVolGuid++;
	}
	//Madan

	if(bChanged && (!bNewVolumesAdded) && (!bOldVolumesRemoved))
	{
		LogAndPrint("\n\tSuccessfully Mapped all the Drive Letters as the Original System Configuration.\n");
	}
	else if((!bChanged)&& (!bNewVolumesAdded) && (!bOldVolumesRemoved))
	{
		LogAndPrint("\n No Drive Letters are changed from the previous system's Drive Letter Configuration.\n");
	}
	else if(bChanged && (bOldVolumesRemoved))
	{
		LogAndPrint("\n\tAtleast one volume could not be mapped with the original drive letter configuration\n\tas atleast one original volume is remvoed from the system configuration.\n");
		LogAndPrint("\n\tRest all are successfully mapped.\n");
	}
	else if((bChanged) && (bNewVolumesAdded))
	{
		LogAndPrint("\n\tSome new volumes were added to the system and others old volumes were mapped with the original drive letters!.\n");
	}
	//Mount all the Volumes in all the Volume Mount Points One by One
	return bStatus;
}

BOOL RemoveSystemVolumeFromUnMountList()
{
	USES_CONVERSION;
	BOOL bFlag = TRUE;
	WCHAR wzSysPath[MAX_PATH];
	WCHAR wzRootVolumeName[MAX_PATH];
	WCHAR VolumeGUID[MAX_PATH];
	if(0 == GetSystemDirectoryW(wzSysPath, MAX_PATH))
	{
		LogAndPrint("\n Failed to get System Directory Path.Error :%d\n",GetLastError());
		bFlag = FALSE;
	}

	//Get the root Volume.e.g. C:\ or E:
	if( 0 == GetVolumePathNameW(wzSysPath,wzRootVolumeName,MAX_PATH))
	{
		LogAndPrint("\n Failed to get the Root volume Name of the System Directory.Error:%d\n",GetLastError());
		bFlag = FALSE;
	}

	if(FALSE == GetVolumeNameForVolumeMountPointW(
				wzRootVolumeName , // input volume mount point or directory
				VolumeGUID, // output volume name buffer
				MAX_PATH // size of volume name buffer
				))
	{
		LogAndPrint("\n Failed to get GUID for the System volume.Error:%d\n",GetLastError());
		bFlag = FALSE;
	}
	//Search the Vector for the corresponding System Volume GUID.
	vector<VolumeInfo>::iterator iter;
	iter = vecDriveLetter.begin();
	while(iter != vecDriveLetter.end())
	{	
		VolumeInfo volInfo = (*iter);
		if(volInfo.strVolGuid == W2A(VolumeGUID))
		{
			vecDriveLetter.erase(iter);
			//iter--;
			break;
		}
		iter++;
	}
	return bFlag;
}

BOOL BuildMapFromFile()
{
	TCHAR szSize[MNTPNTPATH_SIZE_FIELD] = {0};
	//char szSize[MNTPNTPATH_SIZE_FIELD] = {0};
	TCHAR szPath[MAX_POSSIBLE_PATH] = {0};
	//char szPath[MAX_POSSIBLE_PATH] = {0};
	TCHAR szVol[VOLUME_GUID_SIZE_FIELD] ={0};
	//char szVol[VOLUME_GUID_SIZE_FIELD] ={0};
	//string strPath;
	//string strVol;
	UIntType len = 0;
	DWORD dwBytesRead = 0;
	BOOL bStatus = TRUE;
	
	string strTotalFile;
	char szTemp[0xffff] = {0};
	int i = 0;
	while(TRUE)
	{
		memset((void*)szSize,0x00,MNTPNTPATH_SIZE_FIELD);
		memset((void*)szPath,0x00,MAX_POSSIBLE_PATH);
		memset((void*)szVol,0x00,VOLUME_GUID_SIZE_FIELD);
		dwBytesRead = 0;
		
		if(!ReadFile(hMDLFile,(LPVOID)&szSize[0],MNTPNTPATH_SIZE_FIELD,&dwBytesRead,0))
		{	
			LogAndPrint("\n Error reading the file...Error = %d",GetLastError());
			return FALSE;//MCHECK
		}
		else if(MNTPNTPATH_SIZE_FIELD != dwBytesRead)
		{		
			if(0 == dwBytesRead)//Reached End Of File
			{
				break;
			}
			LogAndPrint("\n Error reading the file...Error = %d",GetLastError());
			return FALSE;//MCHECK
		}
		len = szSize[1]<<8 | szSize[0];
		if(!ReadFile(hMDLFile,(LPVOID)&szPath[0],len,&dwBytesRead,0))
		{	
			LogAndPrint("\n Error reading the file...Error = %d",GetLastError());
			return FALSE;//MCHECK
		}
		else if(len != dwBytesRead)
		{
			if(0 == dwBytesRead)//Reached the End Of File
			{
				break;
			}
			LogAndPrint("\n Error reading the file...Error = %d",GetLastError());
			return FALSE;//MCHECK
		}
		//if(!ReadFile(hMDLFile,(LPVOID)&szVol[0],(VOLUME_GUID_SIZE_FIELD),&dwBytesRead,0))
		if(!ReadFile(hMDLFile,(LPVOID)&szVol[0],(VOLUME_GUID_SIZE_FIELD -1),&dwBytesRead,0))
		{	
			LogAndPrint("\n Error reading the file...Error = %d",GetLastError());
			return FALSE;//MCHECK
		}
		//else if((VOLUME_GUID_SIZE_FIELD) != dwBytesRead)
		else if((VOLUME_GUID_SIZE_FIELD -1) != dwBytesRead)
		{
			if(0 == dwBytesRead)//Reached the End Of File
			{
				break;
			}
			LogAndPrint("\n Error reading the file...Error = %d",GetLastError());
			return FALSE;//MCHECK
		}
		//Read one problem causing character to ignore
		char szTempChar;
		if(!ReadFile(hMDLFile,(LPVOID)&szTempChar,1,&dwBytesRead,0))
		{
			cout<<endl<<"Unable to read a special character";
				return FALSE;
		}
		else if( 1 != dwBytesRead)
		{
			if(0 == dwBytesRead)
			{
				break;
			}
		}	
		string strPath = string(szPath);		
		string strVolGuid =  string(szVol);
		//Add the missing characters }\ in the strVolGuid
		string strTemp;
		strTemp = strVolGuid;
		strVolGuid = strTemp + "}\\";
		mapFileMPPathVolGuid.insert(make_pair(strPath,strVolGuid));

		//Add the Volume Guid and the Volume Mount Point Path to the Map
		VolumeInfo volInfo = mapStrVolInfoFromFile[strVolGuid];
		volInfo.vMountPoints.push_back(strPath);		
		mapStrVolInfoFromFile.insert(make_pair(strVolGuid,volInfo));
		
	}
	return TRUE;
}

BOOL BuildMapFromMemory()
{
	BOOL bStatus = FALSE;
	string strVolGuid;

	//Remove System Volume from the list
	if(!RemoveSystemVolumeFromUnMountList())
	{
		LogAndPrint("\n Error in Building Map from Memory excluding the System Volume.");
		return FALSE;
	}
	vector<VolumeInfo>::iterator iterVolumeInfo;
	iterVolumeInfo = vecDriveLetter.begin();
	while(iterVolumeInfo != vecDriveLetter.end())
	{
		strVolGuid.clear();
		VolumeInfo volInfo;
		volInfo = (*iterVolumeInfo);
		strVolGuid = volInfo.strVolGuid;
		vector<string>::iterator iterMntPath;
		iterMntPath = volInfo.vMountPoints.begin();
		while(iterMntPath != volInfo.vMountPoints.end())
		{			
			string strMntPntPath = (*iterMntPath);
			mapMemoryMPPathVolGuid.insert(make_pair(strMntPntPath,strVolGuid));

			//Add the Volume Guid and the Volume Mount Point Path to the Map
			VolumeInfo volInfo = mapStrVolInfoFromMemory[strVolGuid];
			volInfo.vMountPoints.push_back(strMntPntPath);			
			mapStrVolInfoFromMemory.insert(make_pair(strVolGuid,volInfo));

			iterMntPath++;
			bStatus = TRUE;
		}		
		iterVolumeInfo++;
	}	
	return bStatus;
}
void StripVolumeSpecialChars(const string strOld,string & strNew)
{
	strNew = strOld.substr(10,38);
	strNew = strNew + "\0";
}

void AddVolumeSpecialChars(const string strOld,string & strNew)
{
	string strVolGuidPrefix = "\\\\?\\Volume";
	string strVolGuidSuffix = "\\";
	strNew = strVolGuidPrefix + strOld + strVolGuidSuffix;
}
void ProperlyEscapeSpecialChars(const string strOld,string &strNew)
{
	string strTemp;
	StripVolumeSpecialChars(strOld,strTemp);
	AddVolumeSpecialChars(strTemp,strNew);
}

//A Recursive function to Un-Mount the Volume Mount Points from the Top Down
//(i.e Un-Mount the inner most child Volume Mount Point first and then the upper one to the top.
BOOL MountUnMountTheRootVolume(LPCTSTR lpszPath,BOOL bUnMount)
{	
	BOOL bStatus = TRUE;
	string strVolGuid;
	string strOldRootPathName		= "Old";
	string strCurrentRootPathName	= lpszPath;
	memset((void*)szRootPathName,0x00,sizeof(szRootPathName));
	if(strCurrentRootPathName.length() != 3)//Note this check is done to solve issues faced exclusively in W2K8
	{
		while(strOldRootPathName != strCurrentRootPathName)
		{			
			//bStatus = GetVolumePathName(strCurrentRootPathName.c_str(),szRootPathName,MAX_PATH);
			if(!GetVolumePathName(strCurrentRootPathName.c_str(),szRootPathName,MAX_PATH))
			//if(!bStatus)
			{
				//LogAndPrint("\n Error getting the Root Path Name for %s while Un-Mounting.\tError = %d",(char*)strCurrentRootPathName.c_str(),GetLastError());
				//return FALSE;
				break;
			}
			else
			{
				strOldRootPathName = strCurrentRootPathName;
				strCurrentRootPathName = szRootPathName;			
			}
		}//end of while loop
	}
	else if(strCurrentRootPathName.length() == 3)
	{
		inm_memcpy_s((void*)szRootPathName, sizeof(szRootPathName), (const void*)lpszPath, sizeof(lpszPath));
	}
	else
	{
		LogAndPrint("\nErro in getting the root path name of %s.",(char*)lpszPath);
		return FALSE;
	}

	//After getting the Root Path Name, Un-Mount it
	//Note: If the Base Root Path is Un-Mounted, then all the nested Volume Mount Points will remain
	//intact and next time if we change the drive letter by Mounting a Volume Mount Point, then all the
	//nested Volume Mount Points will be obtained back. This is a technique to which captilizes the
	//Windows feature of storing the Volume Mount Points and the Volume GUIDs information in the 
	//Windows registry. Windows will automatically map all the Volume Mount Points to the new/old drive letter.
	if(bUnMount)//Un-Mounting the Base Root Path
	{
		string strUnMountPath = string(szRootPathName);
		if((UINT)strUnMountPath.length() == 3)//to ensure that it is a utmost base volume like G:\//MCHECK
		{
			if(!DeleteVolumeMountPoint(szRootPathName))//Un-Mount the Root Base Volume (= remove the drive letter)
			{
				//LogAndPrint("\n\t Unable to Delete the Volume Mount Point %s\tError = %d",szRootPathName,(int)GetLastError());
			}
			else
			{
				LogAndPrint("\n\t Un-Mounted\t %s\n",szRootPathName);
				Sleep(MOUNT_UNMOUNT_DELAY);
			}
		}
	}
	else//Mounting the Base Root Path
	{	
		//Get the Volume Guid of the Root Base Path
		mapStrStrMntPntPathVolGuid::iterator iter = mapFileMPPathVolGuid.find((string)szRootPathName);
		if(iter != mapFileMPPathVolGuid.end())
		{
			strVolGuid = (*iter).second;//get the Volume Guid.
			if(!SetVolumeMountPoint((LPCTSTR)szRootPathName,strVolGuid.c_str()))
			{
				//LogAndPrint("\n\tError in setting the Volume Mount Point %s for Volume %s,\tError = %d",(char*)szRootPathName,(char*)strVolGuid.c_str(),(int)GetLastError());
			}
			else
			{
				LogAndPrint("\n\tMounted Volume %s at\t%s\n",(char*)strVolGuid.c_str(),(char*)szRootPathName);
				Sleep(MOUNT_UNMOUNT_DELAY);
			}
		}
		else
		{
			//LogAndPrint("\nMCHECK:Could not find %s in binary file",szRootPathName);
		}
	}//end of Mounting the Base Root Path
	return bStatus;
}

#ifdef _CLUSUTIL_LOG
	#define printf LogAndPrint
#endif
char chPrintBuffer[1024] = {0};
#define CLUSUTIL_LOGFILE	"ClusUtilLogFile.log"
ofstream ClusUtilLogFile(CLUSUTIL_LOGFILE,ios::out | ios::app);
void CloseLogFile()
{
	//LogAndPrint("\n\t\t End of logging in ClusUtil.exe.\n");
	//LogAndPrint("\n******************************************************************************************\n");
	ClusUtilLogFile.close();
}
int LogAndPrint(std::string strDesc)
{
	cout<<strDesc;
	ClusUtilLogFile<<strDesc;
	ClusUtilLogFile.flush();
	return (int)strDesc.size();
}
int LogAndPrint(char* lpszText)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText);
	//printf(lpszText);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}
int LogAndPrint(const char* lpszText)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText);
	//printf(lpszText);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);

}
int LogAndPrint(char* lpszText,int nVal)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, nVal);
	//printf(lpszText,nVal);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}
int LogAndPrint(char* lpszText,long nVal)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, nVal);
	//printf(lpszText,nVal);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (long)strlen(chPrintBuffer);
}

int LogAndPrint(char* lpszText,char* lpszMoreText)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, lpszMoreText);
	//printf(lpszText,lpszMoreText);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}
int LogAndPrint(char* lpszText,const char* lpszMoreText)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, lpszMoreText);
	//printf(lpszText,lpszMoreText);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}

int LogAndPrint(char* lpszText,int nVal,LPCTSTR lpszMoreText)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, nVal, lpszMoreText);
	//printf(lpszText,nVal,lpszMoreText);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}
int LogAndPrint(char* lpszText,char* lpszMoreText, char* lpszEvenMoreText,int nVal)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, lpszMoreText, lpszEvenMoreText, nVal);
	//printf(lpszText,lpszMoreText,lpszEvenMoreText,nVal);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}
int LogAndPrint(char* lpszText,char* lpszMoreText, char* lpszEvenMoreText)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, lpszMoreText, lpszEvenMoreText);
	//printf(lpszText,lpszMoreText,lpszEvenMoreText);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}
int LogAndPrint(char* lpszText,char* lpszMoreText, int nVal)
{
	memset((void*)chPrintBuffer,0x00,1024);
	inm_sprintf_s(chPrintBuffer, ARRAYSIZE(chPrintBuffer), lpszText, lpszMoreText, nVal);
	//printf(lpszText,lpszMoreText,nVal);
	cout<<chPrintBuffer;
	ClusUtilLogFile<<chPrintBuffer;
	ClusUtilLogFile.flush();
	return (int)strlen(chPrintBuffer);
}

//#endif