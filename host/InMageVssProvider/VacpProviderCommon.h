#pragma once
#include "stdafx.h"

#define VACP_FLAGS_REGISTER_VSS_PROVIDER         0
#define VACP_FLAGS_UNREGISTER_VSS_PROVIDER       1

const GUID gVssProviderId =
{ 0x3eb85257, 0xe0b0, 0x4788, { 0xa0, 0x80, 0xd3, 0x9a, 0xa6, 0xa5, 0x15, 0xaf } };

const GUID gVssProviderVersionId =
{ 0xa4ef33d7, 0xefa0, 0x45fc, { 0x9a, 0xa, 0xa3, 0x73, 0xdd, 0xf4, 0x91, 0x6e } };

static WCHAR* gVssProviderName = L"Azure Site Recovery VSS Provider";
static WCHAR* gVssProviderVersion = L"1.0";

#define VACP_PROVIDER_LOG_ALL ( EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE)
#define VACP_PROVIDER_LOG_WARNING ( EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE)
#define VACP_PROVIDER_LOG_ERROR ( EVENTLOG_ERROR_TYPE )

HRESULT RegisterWithVss(USHORT flags)
{
    HRESULT hr = S_OK;
    CComPtr<IVssAdmin> pVssAdmin = NULL;
    hr = CoCreateInstance(CLSID_VSSCoordinator, 0, CLSCTX_LOCAL_SERVER, IID_IVssAdmin, (void**)&pVssAdmin);

    if (SUCCEEDED(hr))
    {
        if (((flags & VACP_FLAGS_UNREGISTER_VSS_PROVIDER) == VACP_FLAGS_REGISTER_VSS_PROVIDER))
        {
            hr = pVssAdmin->RegisterProvider(gVssProviderId,
                CLSID_VacpProvider,
                gVssProviderName,
                VSS_PROV_SOFTWARE,
                gVssProviderVersion,
                gVssProviderVersionId);

            if (hr == VSS_E_PROVIDER_ALREADY_REGISTERED)
                hr = S_OK;

        }
        else if (((flags & VACP_FLAGS_UNREGISTER_VSS_PROVIDER) == VACP_FLAGS_UNREGISTER_VSS_PROVIDER))
        {
            hr = pVssAdmin->UnregisterProvider(gVssProviderId);
            if (hr == VSS_E_PROVIDER_NOT_REGISTERED)
                hr = S_OK;
        }

        pVssAdmin.Release();
    }

    return hr;
}