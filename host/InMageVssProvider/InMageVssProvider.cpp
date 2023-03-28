


#include "stdafx.h"
#include "resource.h"
#include "InMageVssProvider_i.h"
#include "dllmain.h"

//GUID of InMage VSS Provider

const GUID g_gProviderId =
  {0x3eb85257, 0xe0b0, 0x4788, {0xa0, 0x80, 0xd3, 0x9a, 0xa6, 0xa5, 0x15, 0xaf}};


STDAPI DllCanUnloadNow(void)
{
  return _AtlModule.DllCanUnloadNow();
}



STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}



STDAPI DllRegisterServer(void)
{
	ShowMsg("\n Inside InMage VSS Provider DLL DllRegisterServer() function.\n");

    HRESULT hr = _AtlModule.DllRegisterServer();

	return hr;
}



STDAPI DllUnregisterServer(void)
{
	ShowMsg("\n Inside InMage VSS Provider DLLUnRegisterServer() function.\n");
	
	CComPtr<IVssAdmin> pVssAdmin;
	HRESULT hr = CoCreateInstance( CLSID_VSSCoordinator,
                           NULL,
                           CLSCTX_ALL,
                           IID_IVssAdmin,
                           (void **) &pVssAdmin);

    if (SUCCEEDED( hr ))
	{
        hr = pVssAdmin->UnregisterProvider( g_gProviderId );
		if(SUCCEEDED(hr))
		{
			ShowMsg("\n InMage VSS Provider is un-registered.\n");
		}
    }

	hr = _AtlModule.DllUnregisterServer();
	pVssAdmin.Release();
	return hr;
}


STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine)
{
	USES_CONVERSION;
	ShowMsg("\n InMage VSS Provider's DllInstall() function.\n");
    HRESULT hr = E_FAIL;
    static const char szUserSwitch[] = _T("user");

    if (pszCmdLine != NULL)
    {
    	if (_tcsnicmp(W2A(pszCmdLine), szUserSwitch, _countof(szUserSwitch)) == 0)
    	{
    		AtlSetPerUserRegistration(true);
    	}
    }

    if (bInstall)
    {	
    	hr = DllRegisterServer();
    	if (FAILED(hr))
    	{	
    		DllUnregisterServer();
    	}
    }
    else
    {
    	hr = DllUnregisterServer();
    }
    return hr;
}

