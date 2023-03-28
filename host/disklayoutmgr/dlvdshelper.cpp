#include "dlvdshelper.h"

#ifdef WIN32

DLVdsHelper::DLVdsHelper(void)
:m_pIVdsService(NULL)
{
}

DLVdsHelper::~DLVdsHelper(void)
{
	INM_SAFE_RELEASE(m_pIVdsService);
}

bool DLVdsHelper::InitVDS()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bInitSuccess = false;
	HRESULT hResult;
	IVdsServiceLoader *pVdsSrvLoader = NULL;

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
			DebugPrintf(SV_LOG_INFO, "VDS Initializaton successful\n");
			bInitSuccess = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"VDS Service ready failure 0x%08x\n", hResult);
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"VDS Initialization failure 0x%08x\n", hResult);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bInitSuccess;
}

HRESULT DLVdsHelper::ProcessProviders()
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

HRESULT DLVdsHelper::ProcessPacks(IVdsSwProvider *pSwProvider)
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
				case INM_OBJ_EFI:
					FindEfiPartitionDisk(pPack);
					break;
				case INM_OBJ_PARTITION:
					GetDiskPartitionInfo(pPack);
					break;
				case INM_OBJ_FOREIGN_DISK:
					CollectForeignDisks(pPack);
					break;
				case INM_OBJ_DYNAMIC_DISK:
					CollectDynamicDisks(pPack);
					break;
				case INM_OBJ_UNKNOWN:
					CheckUnKnownDisks(pPack);
					break;
				case INM_OBJ_OFFLINE_DISK:
					CollectOfflineDisks(pPack);
					break;
				case INM_OBJ_DISK:
					CollectDisks(pPack);
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

HRESULT DLVdsHelper::FindEfiPartitionDisk(IVdsPack *pPack)
{
USES_CONVERSION;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr = S_OK;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;
	IVdsAdvancedDisk *pAdvDisk = NULL;

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
				hr = pIUnknown->QueryInterface(IID_IVdsAdvancedDisk,
					(void**)&pAdvDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk) && (!pAdvDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					std::string strDisk;
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					DebugPrintf(SV_LOG_INFO,"Found disk : %s having partition style : %d  and GUID : %s\n", strDisk.c_str(), diskProps.PartitionStyle, GUIDString(diskProps.DiskGuid).c_str());
					if(2 == diskProps.PartitionStyle)
					{
						VDS_PARTITION_PROP *ppPartitionPropArray;
						LONG plNumberOfPartitions;
						pAdvDisk->QueryPartitions(&ppPartitionPropArray, &plNumberOfPartitions);
						DebugPrintf(SV_LOG_INFO, "No. of partitions : %ld\n", plNumberOfPartitions);
						for(LONG i =0; i < plNumberOfPartitions; i++)
						{
							DebugPrintf(SV_LOG_INFO,"\nPartitions Num: %ld, starting offset: %lld, size: %lld \n", ppPartitionPropArray[i].ulPartitionNumber, ppPartitionPropArray[i].ullOffset, ppPartitionPropArray[i].ullSize);
							DebugPrintf(SV_LOG_INFO,"\nPartition type: %s \n", GUIDString(ppPartitionPropArray[i].Gpt.partitionType).c_str());
							DebugPrintf(SV_LOG_INFO,"\nPartitions Name: %ls\n\n", ppPartitionPropArray[i].Gpt.name);

							std::string strTemp = strDisk.substr(17);
							strTemp = "\\\\.\\PHYSICALDRIVE" + strTemp;

							if(GUIDString(ppPartitionPropArray[i].Gpt.partitionType).compare("{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}") == 0)
							{
								m_mapDisks.insert(make_pair(GUIDString(diskProps.DiskGuid), strTemp));  //Fills the Efi disk info only
							}
						}
						CoTaskMemFree(ppPartitionPropArray);
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pAdvDisk);
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT DLVdsHelper::GetDiskPartitionInfo(IVdsPack *pPack)
{
	USES_CONVERSION;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr = S_OK;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;
	IVdsAdvancedDisk *pAdvDisk = NULL;
    size_t countCopied = 0;

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
				hr = pIUnknown->QueryInterface(IID_IVdsAdvancedDisk,
					(void**)&pAdvDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk) && (!pAdvDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					std::string strDisk;
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					DebugPrintf(SV_LOG_INFO,"\nFound disk : %s having partition style : %d  and GUID : %s\n", strDisk.c_str(), diskProps.PartitionStyle, GUIDString(diskProps.DiskGuid).c_str());
					if(2 == diskProps.PartitionStyle)
					{
						VDS_PARTITION_PROP *ppPartitionPropArray;
						LONG plNumberOfPartitions;
						pAdvDisk->QueryPartitions(&ppPartitionPropArray, &plNumberOfPartitions);

						DlmPartitionInfoSubVec_t vecPartitionInfo;
						std::string strTemp = strDisk.substr(17);
						strTemp = "\\\\.\\PHYSICALDRIVE" + strTemp;
						DebugPrintf(SV_LOG_INFO, "No. of partitions : %ld\n", plNumberOfPartitions);

						for(LONG i =0; i < plNumberOfPartitions; i++)
						{
							if(GUIDString(ppPartitionPropArray[i].Gpt.partitionType).compare("{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}") == 0)   //Efi Partiton tupe GUID
							{
								DebugPrintf(SV_LOG_INFO,"\nPartitions Num: %ld, starting offset: %lld, size: %lld \n", ppPartitionPropArray[i].ulPartitionNumber, ppPartitionPropArray[i].ullOffset, ppPartitionPropArray[i].ullSize);
								DebugPrintf(SV_LOG_INFO,"\nPartition type: %s \n", GUIDString(ppPartitionPropArray[i].Gpt.partitionType).c_str());
								DebugPrintf(SV_LOG_INFO,"\nPartitions Name: %ls\n\n", ppPartitionPropArray[i].Gpt.name);
								
								//Filling the Efi partition information
								DLM_PARTITION_INFO_SUB efiDiskInfo;
								efiDiskInfo.PartitionNum = SV_UINT(ppPartitionPropArray[i].ulPartitionNumber);
								efiDiskInfo.StartingOffset = ppPartitionPropArray[i].ullOffset;
								efiDiskInfo.PartitionLength = ppPartitionPropArray[i].ullSize;
								inm_strncpy_s(efiDiskInfo.DiskName, ARRAYSIZE(efiDiskInfo.DiskName), strTemp.c_str(), strTemp.length()+1);
                                inm_wcstombs_s(&countCopied, efiDiskInfo.PartitionName, ARRAYSIZE(efiDiskInfo.PartitionName), ppPartitionPropArray[i].Gpt.name, ARRAYSIZE(efiDiskInfo.PartitionName)-1);
								inm_strncpy_s(efiDiskInfo.PartitionType, ARRAYSIZE(efiDiskInfo.PartitionType), GUIDString(ppPartitionPropArray[i].Gpt.partitionType).c_str(), GUIDString(ppPartitionPropArray[i].Gpt.partitionType).length()+1);
								vecPartitionInfo.push_back(efiDiskInfo);
							}
							else if(GUIDString(ppPartitionPropArray[i].Gpt.partitionType).compare("{DE94BBA4-06D1-4D40-A16A-BFD50179D6AC}") == 0)  //Recovery partition type GUID
							{
								DebugPrintf(SV_LOG_INFO,"\nPartitions Num: %ld, starting offset: %lld, size: %lld \n", ppPartitionPropArray[i].ulPartitionNumber, ppPartitionPropArray[i].ullOffset, ppPartitionPropArray[i].ullSize);
								DebugPrintf(SV_LOG_INFO,"\nPartition type: %s \n", GUIDString(ppPartitionPropArray[i].Gpt.partitionType).c_str());
								DebugPrintf(SV_LOG_INFO,"\nPartitions Name: %ls\n\n", ppPartitionPropArray[i].Gpt.name);

								//Filling the Recovery partition information
								DLM_PARTITION_INFO_SUB recoveryPartitionInfo;
								recoveryPartitionInfo.PartitionNum = SV_UINT(ppPartitionPropArray[i].ulPartitionNumber);
								recoveryPartitionInfo.StartingOffset = ppPartitionPropArray[i].ullOffset;
								recoveryPartitionInfo.PartitionLength = ppPartitionPropArray[i].ullSize;
								inm_strncpy_s(recoveryPartitionInfo.DiskName, ARRAYSIZE(recoveryPartitionInfo.DiskName), strTemp.c_str(), strTemp.length()+1);
                                inm_wcstombs_s(&countCopied, recoveryPartitionInfo.PartitionName, ARRAYSIZE(recoveryPartitionInfo.PartitionName), ppPartitionPropArray[i].Gpt.name, ARRAYSIZE(recoveryPartitionInfo.PartitionName)-1);
								inm_strncpy_s(recoveryPartitionInfo.PartitionType, ARRAYSIZE(recoveryPartitionInfo.PartitionType), GUIDString(ppPartitionPropArray[i].Gpt.partitionType).c_str(), GUIDString(ppPartitionPropArray[i].Gpt.partitionType).length()+1);
								vecPartitionInfo.push_back(recoveryPartitionInfo);
							}
						}
						if(!vecPartitionInfo.empty())
						{
							DebugPrintf(SV_LOG_INFO,"\nSpecial partitions exist for this disk: %s\n\n", strTemp.c_str());
							m_mapPartitionDisks.insert(make_pair(strTemp, vecPartitionInfo));
						}
						else
						{
							DebugPrintf(SV_LOG_INFO,"\nNo Special partitions exist for this disk: %s\n\n", strTemp.c_str());
						}
						CoTaskMemFree(ppPartitionPropArray);
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pAdvDisk);
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT DLVdsHelper::CollectForeignDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	bool isForeignPack = false;
	//VDS Pack properties
	VDS_PACK_PROP packProps;
	hr = pPack->GetProperties(&packProps);

	if(SUCCEEDED(hr))
	{
		
		if(packProps.ulFlags & VDS_PKF_FOREIGN)
		{
			DebugPrintf(SV_LOG_INFO, "Got Foreign disk pack\n");
			isForeignPack = true;
		}
	}

	if(!isForeignPack)
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return hr;
	}

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
				std::string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
					{
						strDisk = W2A(diskProps.pwszName);
						m_listDisk.push_back(strDisk);
					}
					DebugPrintf(SV_LOG_INFO, "Foriegn Disk found, Disk Name : %s  And Disk Id : %s\n", strDisk.c_str() ,GUIDString(diskProps.id).c_str());

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


HRESULT DLVdsHelper::CollectDynamicDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	std::list<VDS_OBJECT_ID> listDynDisks;

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
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return hr;
	}

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
				std::string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{					
					if(diskProps.pwszName)
					{
						strDisk = W2A(diskProps.pwszName);
						m_listDisk.push_back(strDisk);
					}
					DebugPrintf(SV_LOG_INFO,"Dynamic Disk found, Disk Name : %s  And Disk Id : %s\n", strDisk.c_str() ,GUIDString(diskProps.id).c_str());
					
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


HRESULT DLVdsHelper::CollectDisks(IVdsPack *pPack)
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
				//Disk properties
				std::string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(0x00000007 == diskProps.dwDeviceType) //#define FILE_DEVICE_DISK 0x00000007
					{
						if(diskProps.pwszName)
						{
							strDisk = W2A(diskProps.pwszName);
							m_listDisk.push_back(strDisk);
							DebugPrintf(SV_LOG_INFO,"Got Disk, Disk Name : %s  And Disk Id : %s\n", strDisk.c_str() ,GUIDString(diskProps.id).c_str());
						}
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


HRESULT DLVdsHelper::CheckUnKnownDisks(IVdsPack *pPack)
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
						DebugPrintf(SV_LOG_INFO,"Unknown disk found : %s\n", GUIDString(diskProps.id).c_str());
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

HRESULT DLVdsHelper::CollectOfflineDisks(IVdsPack *pPack)
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
				//Disk properties
				std::string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{					
					if(VDS_DS_OFFLINE == diskProps.status)
					{
						strDisk = W2A(diskProps.pwszName);
						m_listDisk.push_back(strDisk);
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

#endif
