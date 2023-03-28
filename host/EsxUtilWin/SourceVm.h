#ifndef SOURCE_VM
#define SOURCE_VM

#include "Common.h"
#include "ace/Process.h"
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "ace/Process_Manager.h"
#include "rpcconfigurator.h"

#include <Shlobj.h>
#include <Shlguid.h>
#pragma comment(lib,"shell32.lib")

class Source:public Common
{
public:	
	Source()
    {
        m_strDiskName = "\\\\.\\PhysicalDrive";
        hPhysicalDriveIOCTL = 0;
    }
    Source(bool bP2V, bool bDeleteFiles)
	{
		 m_strDiskName = "\\\\.\\PhysicalDrive";
		 hPhysicalDriveIOCTL = 0;
		 bP2Vmachine = bP2V;
         m_bDeleteFiles = bDeleteFiles;
	}
	~Source()
	{
	}
	int initSource();
    bool GenerateTagForSrcVol(string strTarget, string strTagName, bool bCrashConsistency, Configurator* TheConfigurator);

private:
	ULONGLONG ulDiskSize;
	int fetchSourceVmInfo();
	bool persistDiskCountInConfFile(const unsigned int uDiskCount);
	BOOL EnumerateVolumes (HANDLE , TCHAR *, int );
	BOOL ListVolumesPresentInSystem();
	bool DumpVolumesOrMntPtsExtentOnDisk();
    bool PersistDiskAndScsiIDs();
	int ReadPhysicalDrives (void);
	bool getDiskSize (string & );
	HRESULT mapLogicalVolmuesToDiskOffset();
	bool IsPartitionDynamic(PARTITION_INFORMATION_EX);
	bool readDiskExtents(string &strDriveNumber,DRIVE_LAYOUT_INFORMATION_EX**);
	int findPartitionType(BYTE);
	void write_disk_info(DiskInformation* pDisk,ofstream& f);
	void write_partition_info(PartitionInfo* pPart,ofstream& f);
	void write_volume_info(volInfo* pVol,ofstream& f);
	HRESULT PersisitDiskData(PartitionInfo &objPartitionInformaton,vector<volInfo> &tempVector);
	HRESULT PersistIPAddressOfHostGuestOS();
	HRESULT PersistSystemVolInfo();
	HRESULT PersistNetworkInterfaceCard();
    bool removeDiskBinFile();
	bool persistVolumeCount();
	BOOL MountAllUnmountedVolumes();
	BOOL IsVolumeNotMounted(TCHAR *VolGUID);
    bool GetSrcReplicatedVolumes(string strTarget, string &strSrcRepVol, Configurator* TheConfigurator);
    bool ExecuteVMConsistencyScript(string strSrcRepVol, string strTagName, bool bCrashConsistency);
    void alterDiskConfig(std::map<unsigned int,unsigned int>& DiskPartMap);
    HRESULT GetFolderDescriptionId(LPCWSTR pszPath, SHDESCRIPTIONID *pdid);
    bool GetParentDirectory(std::string& ostrdst, const std::string& strsrc);
    bool ProcessAllParentPathsForRecycleBin(char InputPath[]);
    bool CheckMntPathsExistInRecycleBin();
    bool EnableFileModeForSysVol();
    bool bIsOS64Bit();
    bool GetSystemVolume(string & strSysVol);
    bool CollectEBRs(unsigned int DiskNumber, ACE_LOFF_T PartitionStartOffset);
    bool DeleteAllFilesInFolder(std::string FolderPath);


	HANDLE hPhysicalDriveIOCTL;
	//This map contains the volume GUIDS and the Corresponding the volumes's Logical names
	map<std::string,std::string> m_mapOfGuidsWithLogicalNames;
	multimap<int ,volInfo>m_mapVolInfo;
	vector<string> vecDiskSignature;
    string m_strDiskName ;
    bool m_bDeleteFiles;
    vector<DiskInformation>disk_vector;
	vector <volInfo> m_VolumeInfo_PerPartition;
	bitset<MAX_DISK_IN_SYSTEM> diskBitset;
	vector <PartitionInfo> partition_vector;
};




#endif