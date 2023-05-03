#pragma once
#ifndef DLVDSHELPER__H
#define DLVDSHELPER__H

#include "common.h"
#include "dlmapi.h"

#ifdef WIN32

#include <windows.h>
#include <atlconv.h>
#include <tchar.h>
#include <initguid.h>
#include <vdslun.h>
#include <vds.h>
#include <vdserr.h>

#define INM_SAFE_RELEASE(x) {if(NULL != x) { x->Release(); x=NULL; } }
#define GUID_LEN	50

inline std::string GUIDString(const GUID & guid){
	USES_CONVERSION;
	WCHAR szGUID[GUID_LEN] = {0};
	if(!StringFromGUID2(guid,szGUID,GUID_LEN))
		szGUID[0] = L'\0'; //failure
	return W2A(szGUID);
}

class DLVdsHelper
{
	IVdsService* m_pIVdsService;

	enum {
		INM_OBJ_EFI,
		INM_OBJ_PARTITION,
		INM_OBJ_FOREIGN_DISK,
		INM_OBJ_DYNAMIC_DISK,
		INM_OBJ_UNKNOWN,
		INM_OBJ_OFFLINE_DISK,
		INM_OBJ_DISK,
	} m_objType;

	std::map<std::string, std::string> m_mapDisks; //conatains map of disks(depends on disocvery reason e.g. efi case only efi info )
	DlmPartitionInfoSubMap_t m_mapPartitionDisks;  //contains map of disk to disk partition information
	std::list<std::string> m_listDisk;
	bool m_bAvailable;
	
	HRESULT ProcessProviders();
	HRESULT ProcessPacks(IVdsSwProvider *pSwProvider);
	HRESULT FindEfiPartitionDisk(IVdsPack *pPack);
	HRESULT GetDiskPartitionInfo(IVdsPack *pPack);
	HRESULT CollectForeignDisks(IVdsPack *pPack);
	HRESULT CollectDynamicDisks(IVdsPack *pPack);
	HRESULT CheckUnKnownDisks(IVdsPack *pPack);
	HRESULT CollectOfflineDisks(IVdsPack *pPack);
	HRESULT CollectDisks(IVdsPack *pPack);

public:
	DLVdsHelper(void);
	~DLVdsHelper(void);

	bool InitilizeVDS()
	{
		bool bInit = false;
		for(int i=0; i<3; i++)
		{
			if(!InitVDS())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS\n");
				UnInitialize();
				Sleep(2000);
				continue;
			}
			else
			{
				bInit = true;
				break;
			}
		}
		if(bInit)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully initialized the VDS\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS in all three tries.\n");
		}
		return bInit;
	}

	void UnInitialize()
	{
		DebugPrintf(SV_LOG_INFO,"Uninitializing VDS instance\n");
		INM_SAFE_RELEASE(m_pIVdsService);
		//CoUninitialize();
	}

	bool InitVDS();

	void GetDiskPartitionInfo(DlmPartitionInfoSubMap_t& mapDisksToPartitions)
	{
		m_objType = INM_OBJ_PARTITION;
		ProcessProviders();
		DlmPartitionInfoSubIterMap_t iter = m_mapPartitionDisks.begin();
		for(; iter != m_mapPartitionDisks.end(); iter++)
		{
			mapDisksToPartitions.insert(make_pair(iter->first, iter->second));
		}
		m_mapPartitionDisks.clear();	
	}

	void FindEfiPartitionDisk(std::map<std::string, std::string>& mapEfiDisks)
	{
		m_objType = INM_OBJ_EFI;
		ProcessProviders();
		std::map<std::string,std::string>::iterator iter = m_mapDisks.begin();
		for(; iter != m_mapDisks.end(); iter++)
		{
			mapEfiDisks.insert(make_pair(iter->first, iter->second));
		}
		m_mapDisks.clear();
	}

	void CollectForeignDisks(std::list<std::string>& listDiskNumb)
	{
		m_listDisk.clear();
		DebugPrintf(SV_LOG_INFO,"Collecting Foreign disks information using VDS\n");
		m_objType = INM_OBJ_FOREIGN_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			std::list<std::string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				std::string phyDrive = "\\\\?\\PhysicalDrive";
				std::string strDiskNum = iter->substr(phyDrive.size());
				listDiskNumb.push_back(strDiskNum);
			}
		}
	}

	void CollectDynamicDisks(std::list<std::string>& listDiskNumb)
	{
		m_listDisk.clear();
		DebugPrintf(SV_LOG_INFO,"Collecting Dynamic disks information using VDS\n");
		m_objType = INM_OBJ_DYNAMIC_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			std::list<std::string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				std::string phyDrive = "\\\\?\\PhysicalDrive";
				std::string strDiskNum = iter->substr(phyDrive.size());
				listDiskNumb.push_back(strDiskNum);
			}
		}
	}

	bool CheckUnKnowndisks()
	{
		m_bAvailable = false;
		DebugPrintf(SV_LOG_INFO,"Collecting Foreign disks information using VDS\n");
		m_objType = INM_OBJ_UNKNOWN;
		ProcessProviders();
		return m_bAvailable;
	}

	void CollectOfflineDisks(std::list<std::string>& listDiskNumb)
	{
		m_listDisk.clear();
		m_objType = INM_OBJ_OFFLINE_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			std::list<std::string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				std::string phyDrive = "\\\\?\\PhysicalDrive";
				std::string strDiskNum = iter->substr(phyDrive.size());
				listDiskNumb.push_back(strDiskNum);
			}
		}
	}

	void CollectDisks(std::set<std::string>& lstDiskNums)
	{
		m_listDisk.clear();
		m_objType = INM_OBJ_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			std::list<std::string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				std::string phyDrive = "\\\\?\\PhysicalDrive";
				std::string strDiskNum = std::string("\\\\.\\PHYSICALDRIVE") + iter->substr(phyDrive.size());
				DebugPrintf(SV_LOG_INFO,"Disk Found : %s \n", strDiskNum.c_str());
				lstDiskNums.insert(strDiskNum);
			}
		}
		DebugPrintf(SV_LOG_INFO,"Collected all the disks information using VDS\n");
	}
};

#else

class DLVdsHelper
{
	public:
	DLVdsHelper(void)
	{
	}
	~DLVdsHelper(void)
	{
	}
	bool InitilizeVDS()
	{
		//need to write code for Linux
		return true;
	}
	void UnInitialize()
	{
		//need to write code for Linux
	}
	void FindEfiPartitionDisk(std::map<std::string, std::string>& mapEfiDisks)
	{
		//need to write code for Linux
	}
	void GetDiskPartitionInfo(DlmPartitionInfoSubMap_t& mapDisksToPartitions)
	{
		//Need to write code for Linux
	}
	void CollectForeignDisks(std::list<std::string>& listDiskNumb)
	{
	}
	void CollectDynamicDisks(std::list<std::string>& listDiskNumb)
	{
	}
	void CollectDisks(std::set<std::string>& lstDiskNums)
	{
	}
	bool CheckUnKnowndisks()
	{
	    return true;
	}
};

#endif
#endif
