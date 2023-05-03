#include "StdAfx.h"
#include "WMIHelper.h"
#include <comutil.h>
#include "inmsafecapis.h"

enum emZoneType
{
	PRIMARY = 1,
	SECONDARY,
	STUB

};

wchar_t* wzZoneType[] = {
	L"INVALID",
	L"Primary zone",
	L"Secondary zone",
	L"Stub zone"
};


InmWMIHelper::InmWMIHelper(void)
{
	m_pInmLocator = NULL;
	m_pInmService = NULL;
	m_pInmUserIdent = NULL;
	m_InmisConnected = FALSE;
	CoInitialize(NULL);
}

InmWMIHelper::~InmWMIHelper(void)
{
	DisconnectWMI();
	m_pInmLocator = NULL;
	m_pInmService = NULL;
	if(m_pInmUserIdent)
	{
		delete m_pInmUserIdent;
	}
}
/// Following three parameters are depend on each other. If any one of them is NULL then 
/// the remaining values will be ignored and the connection will establish with default account
//		WCHAR *szDomainName,
//		WCHAR *szUserName, 
//		WCHAR *szPwd
DWORD InmWMIHelper::ConnectWMI(WCHAR *serverName, WCHAR *szDomainName, WCHAR *szUserName, WCHAR *szPwd)
{
	HRESULT		hres;
	WCHAR		nameSpace[256] = L"\\\\";
	DWORD		ret = 0;

	//Disconnect WMI connection if it is already Connected 
	DisconnectWMI();
	
	hres = CoCreateInstance(CLSID_WbemLocator, 0,CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &m_pInmLocator);

	if (FAILED(hres))
	{
		printf("Failed to obtain the initial namespace for WMI.\n");
		LPVOID lpInmMessageBuffer;
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,hres,0,
			(LPTSTR)&lpInmMessageBuffer,0,NULL);

		printf("Error:[0x%X] %s\n",hres,(LPCTSTR)lpInmMessageBuffer);
		ret = hres;

		
		LocalFree(lpInmMessageBuffer);
		INM_DONE();
	}
	
	// if server name is not specified connect to the root\cimv2 namespace with the current user.
	if(NULL == serverName)
		inm_wcsncat_s(nameSpace,ARRAYSIZE(nameSpace), L".",256);
	else
		inm_wcsncat_s(nameSpace,ARRAYSIZE(nameSpace), (WCHAR *)serverName,256);

	inm_wcsncat_s(nameSpace,ARRAYSIZE(nameSpace), L"\\Root\\MicrosoftDNS",256);
	if( szDomainName == NULL ||	szUserName	 == NULL ||	szPwd	 == NULL)
	{
		hres = m_pInmLocator->ConnectServer(_bstr_t(nameSpace), NULL, NULL,	0,NULL,0,0, &m_pInmService);
	}
	else
	{
		hres = m_pInmLocator->ConnectServer(_bstr_t(nameSpace),szUserName, szPwd,0,NULL,0,0,&m_pInmService);
	}


	if (FAILED(hres))
	{
		printf("Failed to connect to WMI service.\n");
		LPVOID lpInmMessageBuffer;
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,hres,0,
			(LPTSTR)&lpInmMessageBuffer,0,NULL);

		printf("Error:[0x%X] %s\n",hres,(LPCTSTR)lpInmMessageBuffer);
        ret = hres;

		LocalFree(lpInmMessageBuffer);
		INM_DONE();
	}

	if(szDomainName && szUserName && szPwd)
	{
		COAUTHIDENTITY *pAuthIdentity = new COAUTHIDENTITY;
		memset(pAuthIdentity,0,sizeof(COAUTHIDENTITY));

		pAuthIdentity->Domain = (USHORT*)szDomainName;
		pAuthIdentity->DomainLength = wcslen(szDomainName);

		pAuthIdentity->Password = (USHORT*)szPwd;
		pAuthIdentity->PasswordLength = wcslen(szPwd);

		pAuthIdentity->User = (USHORT*)szUserName;
		pAuthIdentity->UserLength = wcslen(szUserName);

		pAuthIdentity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

		SetUserIdentiry(pAuthIdentity);

		hres = CoSetProxyBlanket(m_pInmService,	RPC_C_AUTHN_DEFAULT,RPC_C_AUTHZ_DEFAULT,COLE_DEFAULT_PRINCIPAL,
			RPC_C_AUTHN_LEVEL_PKT_PRIVACY,	RPC_C_IMP_LEVEL_IMPERSONATE,m_pInmUserIdent,0);
	}
	// COM security does not allow one process to access another process if you do not set the proper security properties. here set the 
	//proxy so that impersonation of the client happen.
	else
	{
		hres = CoSetProxyBlanket(m_pInmService,  RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,  NULL, RPC_C_AUTHN_LEVEL_CALL,
		   RPC_C_IMP_LEVEL_IMPERSONATE,   NULL,   0);
	}

	if (FAILED(hres))
	{
		printf("Failed to set proxy authentication information.\n");
		LPVOID lpInmMessageBuffer;
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,hres,0,
			(LPTSTR)&lpInmMessageBuffer,0,NULL);

		printf("Error:[0x%X] %s\n",hres,(LPCTSTR)lpInmMessageBuffer);
        ret = hres;

	   
		LocalFree(lpInmMessageBuffer);
		INM_DONE();
	}

	m_InmisConnected = TRUE;

	ret = ERROR_SUCCESS;
	return ret;

INM_EXITD();
	if(m_pInmLocator)
		m_pInmLocator->Release();     
	if(m_pInmService)
		m_pInmService->Release();

	return ret;
}


DWORD InmWMIHelper::DisconnectWMI()
{
	DWORD ret;

	if(m_InmisConnected)
	{
		if(m_pInmService)
			m_pInmService->Release();
		m_pInmService = NULL;
		if(m_pInmLocator)
			m_pInmLocator->Release();     
		m_pInmLocator = NULL;
	}

	m_InmisConnected = FALSE;
	ret = TRUE;

	return ret;
}


DWORD InmWMIHelper::UpdateDnsFromDS(std::vector<const char*> &vListOfZoneName)
{
	DWORD dwRet = WBEM_S_NO_ERROR;
	DWORD dwListOfZones = 0;
	if(&vListOfZoneName !=NULL)
	{
		dwListOfZones = (DWORD)vListOfZoneName.size();	
	}
	
	bool bZoneNameFound = false;
	wchar_t wzContainerName[MAX_PATH] = {0};
	wchar_t wzName[MAX_PATH] = {0};
	wchar_t wzDNSServerName[MAX_PATH] = {0};
	int iDSIntegrated = 0;
	DWORD dwZoneType = 0;
		
	HRESULT				hRes = WBEM_S_FALSE;
	IWbemClassObject	* pInmDNSServer = NULL;
	IEnumWbemClassObject* piInmWmiEnum = NULL;
	
	if(m_InmisConnected)
	{
		DWORD dwIndex = 0;
		do
		{
			hRes = m_pInmService->CreateInstanceEnum( _bstr_t(L"MicrosoftDNS_Zone"),NULL,NULL ,&piInmWmiEnum);
			if(hRes != WBEM_S_NO_ERROR)
			{
				LPVOID lpInmMessageBuffer;
				::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,hRes,0,
					(LPTSTR)&lpInmMessageBuffer,0,NULL);

				printf("Error:[0x%X] %s\n",hRes,(LPCTSTR)lpInmMessageBuffer);
				
				
				LocalFree(lpInmMessageBuffer);
				dwRet = WBEM_S_FALSE;
				INM_DONE();
			}

			if(m_pInmUserIdent)
			{
				hRes = CoSetProxyBlanket(piInmWmiEnum,	RPC_C_AUTHN_DEFAULT,RPC_C_AUTHZ_DEFAULT,	COLE_DEFAULT_PRINCIPAL,
								RPC_C_AUTHN_LEVEL_PKT_PRIVACY,	RPC_C_IMP_LEVEL_IMPERSONATE,m_pInmUserIdent,0);
						
				if (FAILED(hRes))
				{
					printf("Failed to set proxy authentication information.\n");
					LPVOID lpInmMessageBuffer;
					::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,hRes,0,
					(LPTSTR)&lpInmMessageBuffer,0,NULL);

					printf("Error:[0x%X] %s\n",hRes,(LPCTSTR)lpInmMessageBuffer);
					dwRet = hRes;
				   
					LocalFree(lpInmMessageBuffer);
					dwRet = WBEM_S_FALSE;
					INM_DONE();
				}
			}


			//  last  Next will return WBEM_S_FALSE 
			while ( (WBEM_S_NO_ERROR == hRes ) && piInmWmiEnum)
			{
				wchar_t wzZoneName[MAX_PATH] = {0};
				bZoneNameFound = false;
				ULONG uReturned; 

				hRes = piInmWmiEnum->Next( WBEM_INFINITE, 1, &pInmDNSServer, &uReturned );
				if (!uReturned) 
					continue;

				if(!pInmDNSServer)
					continue;

				_bstr_t strObjectPath  (L"MicrosoftDNS_Zone.ContainerName=");	
				if ( SUCCEEDED( hRes ) )
				{
					VARIANT		Val;
					VariantInit(&Val);
					hRes = pInmDNSServer->Get( _bstr_t(L"ContainerName"), 0L, &Val, NULL, NULL);
					if(hRes != WBEM_S_NO_ERROR)
					{
						pInmDNSServer->Release();
						pInmDNSServer = NULL;
						VariantClear(&Val);
						printf("Failed to obtain Name. Error:%d",hRes);
						dwRet = hRes;
						INM_DONE();
					}
					if(dwListOfZones)
					{
						mbstowcs(wzZoneName,vListOfZoneName[dwIndex],strlen(vListOfZoneName[dwIndex]));
						if(wcsicmp(wzZoneName,Val.bstrVal))
						{
							VariantClear(&Val);
							if(pInmDNSServer)
							{
								pInmDNSServer->Release();
								pInmDNSServer = NULL;
							}

							continue;
						}
						bZoneNameFound = true;
					}
					inm_wcscpy_s(wzContainerName,ARRAYSIZE(wzContainerName),Val.bstrVal);
					wprintf(L"\nContainerName : %s\n",Val.bstrVal);
					strObjectPath += L"\"";
					strObjectPath += Val.bstrVal;
					strObjectPath += L"\"";
					VariantClear(&Val);

					VariantInit(&Val);
					hRes = pInmDNSServer->Get( _bstr_t(L"DnsServerName"), 0L, &Val, NULL, NULL);
					if(hRes != WBEM_S_NO_ERROR)
					{
						pInmDNSServer->Release();
						pInmDNSServer = NULL;
						VariantClear(&Val);
						printf("Failed to obtain Name. Error:%d",hRes);
						dwRet = hRes;
						INM_DONE();
					}
					
					inm_wcscpy_s(wzDNSServerName,ARRAYSIZE(wzDNSServerName),Val.bstrVal);
					wprintf(L"DnsServerName : %s\n",Val.bstrVal);
					strObjectPath += L",DnsServerName=\"";
					strObjectPath += Val.bstrVal;
					strObjectPath += "\"";
					VariantClear(&Val);

					VariantInit(&Val);
					
					hRes = pInmDNSServer->Get( _bstr_t(L"Name"), 0L, &Val, NULL, NULL);
					if(hRes != WBEM_S_NO_ERROR)
					{
						pInmDNSServer->Release();
						pInmDNSServer = NULL;
						VariantClear(&Val);
						printf("Failed to obtain Name. Error:%d",hRes);
						dwRet = hRes;
						INM_DONE();
					}
					wprintf(L"Name : %s\n",Val.bstrVal);
					inm_wcscpy_s(wzName,ARRAYSIZE(wzName),Val.bstrVal);
					strObjectPath += L",Name=\"";
					strObjectPath += Val.bstrVal;
					strObjectPath += L"\"";
					VariantClear(&Val);

					VariantInit(&Val);
					hRes = pInmDNSServer->Get( _bstr_t(L"ZoneType"), 0L, &Val, NULL, NULL);
					if(hRes != WBEM_S_NO_ERROR)
					{
						pInmDNSServer->Release();
						pInmDNSServer = NULL;
						VariantClear(&Val);
						printf("Failed to obtain Name. Error:%d",hRes);
						dwRet = hRes;
						INM_DONE();
					}
					dwZoneType = Val.iVal;
					if(dwZoneType >=1 && dwZoneType <=3)
					{
						wprintf(L"ZoneType : %s\n",wzZoneType[dwZoneType]);
					}
					else
					{
						wprintf(L"ZoneType %d\n", dwZoneType);
					}
					VariantClear(&Val);

					VariantInit(&Val);
					hRes = pInmDNSServer->Get( _bstr_t(L"DsIntegrated"), 0L, &Val, NULL, NULL);
					if(hRes != WBEM_S_NO_ERROR)
					{
						pInmDNSServer->Release();
						pInmDNSServer = NULL;
						VariantClear(&Val);
						printf("Failed to obtain Name. Error:%d",hRes);
						dwRet = hRes;
						INM_DONE();
					}
					iDSIntegrated = Val.iVal;
					wprintf(L"DS Integrated : %s\n",iDSIntegrated?L"Yes":L"No");
					if(iDSIntegrated == 0)
					{
						wprintf(L"Zone %s is not DS Integrated. Can not update the Zone from DS.\n",wzContainerName);
						VariantClear(&Val);
						if(dwListOfZones)
						{
							pInmDNSServer->Release();
							pInmDNSServer = NULL;
							VariantClear(&Val);
							break;
						}
						else
						{
							pInmDNSServer->Release();
							pInmDNSServer = NULL;
							VariantClear(&Val);
							continue;
						}
					}

					/*if(dwZoneType != 1)
					{
						wprintf(L"Zone type is not primary type. Can not update zone from DS\n",wzContainerName);
						VariantClear(&Val);
						continue;
					}*/
					VariantClear(&Val);
				}

				_bstr_t  MethodName = (L"UpdateFromDS");
				_bstr_t  ZoneClassName = (L"MicrosoftDNS_Zone");
				
				IWbemClassObject* pClass = NULL;
				hRes = m_pInmService->GetObject(ZoneClassName , 0, NULL, &pClass, NULL);

				hRes = pClass->GetMethod(MethodName, 0, 0, 0);

				// Execute Method
				hRes = m_pInmService->ExecMethod(strObjectPath,MethodName, 0,NULL, NULL, NULL,NULL);
				if(WBEM_S_NO_ERROR != hRes)
				{
					printf("Failed to excute WMI method.\n");
					LPVOID lpInmMessageBuffer;
					::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,hRes,0,
					(LPTSTR)&lpInmMessageBuffer,0,NULL);

					printf("Error:[0x%X] %s\n",hRes,(LPCTSTR)lpInmMessageBuffer);
					
					
					LocalFree(lpInmMessageBuffer);

				}
				else
				{
					printf("ZoneUpdateFromDs Succeeded.\n");
				}
				if(pInmDNSServer)
				{
					pInmDNSServer->Release();
					pInmDNSServer = NULL;
				}

				hRes = WBEM_S_NO_ERROR;
				if(bZoneNameFound)
				{
					break;
				}
			} 

			if((!bZoneNameFound) && (dwIndex < dwListOfZones))
			{
				printf("\n\nSpecified zone name \"%s\" is not found.\n",vListOfZoneName[dwIndex]);
				dwRet = WBEM_S_FALSE;
			}
	
			dwIndex++;
			if(pInmDNSServer)
			{
				pInmDNSServer->Release();
				pInmDNSServer = NULL;
			}
			if(piInmWmiEnum)
			{
				piInmWmiEnum->Release();
				piInmWmiEnum = NULL;
			}
		}
		while(dwIndex < dwListOfZones);
	}
	else
	{
		printf("Operation failed. NOT connected to WMI server.\n");
		dwRet = WBEM_S_FALSE;
	}

INM_EXITD();
	if(piInmWmiEnum)
	{
		piInmWmiEnum->Release();
		piInmWmiEnum = NULL;
	}
	if(pInmDNSServer)
	{
		pInmDNSServer->Release();
		pInmDNSServer = NULL;
	}

	return dwRet;
}
