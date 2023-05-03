// DIUtilMajor.cpp : windows functions
//

#include <map>

#include <Windows.h>
#include <winioctl.h>
#include <atlbase.h>
#include <string>
#include <sstream>

#include "CLocalMachineWithDisks.h"
#include "errorexception.h"
#include "ILogger.h"
#include "PlatformAPIs.h"

#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

#define GUID_LEN    36
#define MAX_GUID_LEN 40

HRESULT ComInitialize(ILoggerPtr pILogger)
{
	HRESULT hrInm = S_OK;

	pILogger->LogInfo("ENTERED: ComInitialize\n");

	do
	{
		hrInm = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (hrInm == RPC_E_CHANGED_MODE)
		{
			pILogger->LogInfo("FILE: %s, LINE %d\n", __FILE__, __LINE__);
			pILogger->LogInfo("FAILED: CoInitializeEx() failed. hrInm = 0x%08x \n.", hrInm);
			hrInm = S_FALSE;
			break;
		}
		if ((hrInm == S_OK) || (hrInm == S_FALSE)/*Already Com is Initialized*/)
		{
			hrInm = S_OK;
		}
		hrInm = CoInitializeSecurity
			(
			NULL,                                //  IN PSECURITY_DESCRIPTOR         pSecDesc,
			-1,                                  //  IN LONG                         cAuthSvc,
			NULL,                                //  IN SOLE_AUTHENTICATION_SERVICE *asAuthSvc,
			NULL,                                //  IN void                        *pReserved1,
			RPC_C_AUTHN_LEVEL_CONNECT,           //  IN DWORD                        dwAuthnLevel,
			RPC_C_IMP_LEVEL_IMPERSONATE,         //  IN DWORD                        dwImpLevel,
			NULL,                                //  IN void                        *pAuthList,
			EOAC_NONE,                           //  IN DWORD                        dwCapabilities,
			NULL                                 //  IN void                        *pReserved3
			);
		// Check if the securiy has already been initialized by someone in the same process.
		if (hrInm == RPC_E_TOO_LATE)
		{
			hrInm = S_OK;
		}

		if (hrInm != S_OK)
		{
			pILogger->LogInfo("FILE: %s, LINE %d\n", __FILE__, __LINE__);
			pILogger->LogInfo("FAILED: CoInitializeSecurity() failed. hrInm = 0x%08x \n.", hrInm);
			CoUninitialize();
			break;
		}
	} while (FALSE);

	pILogger->LogInfo("EXITED: ComInitialize\n");
	return hrInm;
}


HRESULT GetDiskInfo(ILoggerPtr pILogger, std::map<std::string, std::string> &signatureToDiskNamesMap)
{
	HRESULT hr = S_OK;

	hr = ComInitialize(pILogger);

	if (FAILED(hr))
	{
		pILogger->LogInfo("Com Initialization failed. Error Code %0x\n", hr);
		return hr;
	}

	IWbemLocator *pLoc = NULL;
	IWbemServices *pWbemService = NULL;
	IEnumWbemClassObject *pEnum = NULL;

	// get the disks from WMI class Win32_DiskDrive

	do {

		hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc);

		if (FAILED(hr))
		{
			pILogger->LogInfo("Failed to create IWbemLocator object. Error Code %0x\n", hr);
			break;
		}

		hr = pLoc->ConnectServer(BSTR(L"ROOT\\cimv2"), NULL, NULL, 0, NULL, 0, 0, &pWbemService);

		if (FAILED(hr))
		{
			pILogger->LogInfo("Could not connect to WMI service. Error Code %0x\n", hr);
			break;
		}

		hr = pWbemService->ExecQuery(
			BSTR(L"WQL"),
			BSTR(L"Select * FROM Win32_DiskDrive"),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

		if (FAILED(hr))
		{
			pILogger->LogInfo("Query for Disk's Device ID failed. Error Code %0x\n", hr);
			break;
		}

		USES_CONVERSION;

		while (pEnum)
		{
			ULONG uReturned = 0;
			IWbemClassObject *pObj = NULL;
			hr = pEnum->Next
				(
				WBEM_INFINITE,
				1,
				&pObj,
				&uReturned
				);

			if (FAILED(hr))
			{
				pILogger->LogInfo("pEnum->Next failed. Error Code %0x\n", hr);
				break;
			}

			if (uReturned == 0)
				break;

			VARIANT val;
			hr = pObj->Get(L"DeviceID", 0, &val, 0, 0);

			if (FAILED(hr))
			{
				pILogger->LogInfo("Get(DeviceID) failed. Error Code %0x\n", hr);
				break;
			}
			std::string strPhysicalDiskName = std::string(W2A(val.bstrVal));
			VariantClear(&val);
			pObj->Release();

			pILogger->LogInfo("Found Disk Drive : %s\n", strPhysicalDiskName.c_str());

			// get the disk signature for each disk drive using IOCTL_DISK_GET_DRIVE_LAYOUT_EX
			HANDLE hDisk = CreateFile(strPhysicalDiskName.c_str(), GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, NULL);
			if (hDisk == INVALID_HANDLE_VALUE)
			{
				continue;
			}

			int EXPECTEDPARTITIONS = 2;
			DRIVE_LAYOUT_INFORMATION_EX *dli = NULL;
			unsigned int drivelayoutsize;
			DWORD bytesreturned;
			DWORD err;
			std::wstring signature;

			do
			{
				if (dli)
				{
					free(dli);
					dli = NULL;
				}
				drivelayoutsize = sizeof (DRIVE_LAYOUT_INFORMATION_EX)+((EXPECTEDPARTITIONS - 1) * sizeof (PARTITION_INFORMATION_EX));
				dli = (DRIVE_LAYOUT_INFORMATION_EX *)calloc(1, drivelayoutsize);

				if (NULL == dli)
				{
					pILogger->LogInfo("Failed to allocate %u bytes for %d expected partitions, "
						"for IOCTL_DISK_GET_DRIVE_LAYOUT_EX\n", drivelayoutsize,
						EXPECTEDPARTITIONS);
					break;
				}

				bytesreturned = 0;
				err = 0;
				if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, dli, drivelayoutsize, &bytesreturned, NULL))
				{
					if (PARTITION_STYLE_MBR == dli->PartitionStyle)
					{
						wostringstream ss;
						ss << dli->Mbr.Signature;
						signature = ss.str();
					}
					else if (PARTITION_STYLE_GPT == dli->PartitionStyle)
					{
						USES_CONVERSION;
						WCHAR szGUID[MAX_GUID_LEN] = { 0 };

						if (!StringFromGUID2(dli->Gpt.DiskId, szGUID, MAX_GUID_LEN))
							continue;
						// szGUID[0] = L'\0';
						std::wstring strGuid(szGUID);
						signature = strGuid.substr(1, GUID_LEN);
					}

					std::string strSignature(signature.begin(), signature.end());
					signatureToDiskNamesMap.insert(std::make_pair(strSignature, strPhysicalDiskName));
					break;
				}
				else if ((err = GetLastError()) == ERROR_INSUFFICIENT_BUFFER)
				{
					pILogger->LogInfo("with EXPECTEDPARTITIONS = %d, IOCTL_DISK_GET_DRIVE_LAYOUT_EX says insufficient buffer\n", EXPECTEDPARTITIONS);
					EXPECTEDPARTITIONS *= 2;
				}
				else
				{
					std::stringstream ss;
					ss << "IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed on disk " << strPhysicalDiskName << " with error " << err;
					pILogger->LogInfo("%s\n", ss.str().c_str());
					break;
				}
			} while (true);

			if (dli)
			{
				free(dli);
				dli = NULL;
			}

			CloseHandle(hDisk);

		}

	} while (false);


	if (pEnum != NULL)
		pEnum->Release();

	if (pWbemService != NULL)
		pWbemService->Release();

	if (pLoc != NULL)
		pLoc->Release();

	if (!FAILED(hr))
		hr = S_OK;

	return hr;
}

void GetDiskNames(const CLocalMachineWithDisks::Disks_t &diskIds, ILoggerPtr pILogger, CLocalMachineWithDisks::Disks_t &diskNames)
{
	// Get signature to disks mapping
	std::map<std::string, std::string> signatureToDiskNamesMap;
	GetDiskInfo(pILogger, signatureToDiskNamesMap);

	// Find the disk names
	for (CLocalMachineWithDisks::ConstDisksIter_t idit = diskIds.begin(); idit != diskIds.end(); idit++) {
		std::map<std::string, std::string>::const_iterator mit = signatureToDiskNamesMap.find(*idit);
		if (signatureToDiskNamesMap.end() != mit) {
			pILogger->LogInfo("Disk %s has signature/GUID %s.\n", mit->second.c_str(), idit->c_str());
			diskNames.push_back(mit->second);
		}
		else
			throw ERROR_EXCEPTION << "Disk Id " << (*idit) << " does not exist on system";
	}
}

boost::shared_ptr<IPlatformAPIs> GetPlatformAPIs(void)
{
	boost::shared_ptr<IPlatformAPIs> p;
	p.reset(new CWin32APIs());

	return p;
}