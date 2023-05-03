#include <Objbase.h>
#include "cdpcli.h"


void CDPCli::InitializePlatformDeps(void)
{
    HRESULT hr;

    if(m_skip_platformdeps_init)
        return;

    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        if (hr != S_OK && hr != S_FALSE)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        hr = CoInitializeSecurity
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
        if (hr == RPC_E_TOO_LATE)
        {
            hr = S_OK;
        }

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
            CoUninitialize();
            break;
        }
    } while(false);
}


void CDPCli::DeInitializePlatformDeps(void)
{
    if(m_skip_platformdeps_init)
        return;

    CoUninitialize();
}
