#include "wmi.h"
#include <sstream>
ACE_Recursive_Thread_Mutex WMIConnector::m_lock ;
#pragma comment(lib, "wbemuuid.lib")


WMIConnector::WMIConnector()
{
    m_WbemServices = NULL ;
    m_WbemLocator = NULL ;
}

SVERROR WMIConnector::ConnectToServer(const std::string& hostName, const std::string& objPath)
{
    AutoGuard guard(m_lock) ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    HRESULT hres ;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &m_WbemLocator);
    if( FAILED(hres) )
    {
        DebugPrintf(SV_LOG_ERROR, "CoCreateInstance failed. Error %08X\n", hres) ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "CoCreateInstance is successful\n") ;
        m_objectNameSpace = "" ;
        m_objectNameSpace += "\\\\" ;
        m_objectNameSpace += hostName ;
        m_objectNameSpace += "\\" ;
        m_objectNameSpace += objPath ;
        
        hres = m_WbemLocator->ConnectServer(_bstr_t(m_objectNameSpace.c_str()),
                                            NULL, NULL, 0, NULL, 0, 0,
                                            &m_WbemServices) ;
        if( FAILED(hres) )
        {
            DebugPrintf(SV_LOG_WARNING, "ConnectServer failed %08X\n", hres) ;
            m_WbemLocator->Release(); 
            m_WbemLocator = NULL ;
            if( WBEM_E_INVALID_NAMESPACE != hres )
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to connect to the WMI Provider %s\n", m_objectNameSpace.c_str()) ;
            }
            DebugPrintf(SV_LOG_WARNING, "IWbemLocator::ConnectServer returned error %08X\n", hres) ;
        }
        else
        {
            hres = CoSetProxyBlanket(m_WbemServices,
                                    RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                    NULL, RPC_C_AUTHN_LEVEL_CALL,
                                    RPC_C_IMP_LEVEL_IMPERSONATE, NULL,
                                    EOAC_NONE);
            if( FAILED(hres) )
            {
                m_WbemServices->Release();     
                m_WbemLocator->Release();
                m_WbemServices = NULL ;
                m_WbemLocator  = NULL ;
                DebugPrintf(SV_LOG_ERROR, "CoSetProxyBlanket failed %08X\n", hres) ;
            }
            else
            {
                bRet = SVS_OK ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR WMIConnector::execQuery(const std::string& query, IEnumWbemClassObject** pEnumerator)
{
    AutoGuard guard(m_lock) ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    HRESULT hres ;
    hres = m_WbemServices->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
                            NULL, pEnumerator) ;
    if( FAILED(hres) )
    {
        m_WbemServices->Release();     
        m_WbemLocator->Release();
        m_WbemLocator = NULL ;
        m_WbemServices = NULL ;
        DebugPrintf(SV_LOG_ERROR, "Query %s is failed. Error %08X\n", query.c_str(), hres) ;
        DebugPrintf(SV_LOG_DEBUG, "Namespace : %s\n", m_objectNameSpace.c_str()) ;
    }
    else
    {
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

WMIConnector::~WMIConnector()
{
    if( m_WbemServices )
    {
        m_WbemServices->Release();     
    }
    if( m_WbemLocator )
    {
        m_WbemLocator->Release();
    }
}