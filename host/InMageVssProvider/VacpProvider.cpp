// VacpProvider.cpp : Implementation of CVacpProvider

#include "stdafx.h"
#include "VacpProvider.h"
#include "vacprpc.h"
#include "inmsafecapis.h"
#include "VacpProviderCommon.h"

char            CVacpProvider::m_pServerIp[MAX_PATH * 2]            = {0};
unsigned long   CVacpProvider::m_ulIoctlBufLen                      = 0;
unsigned long   CVacpProvider::m_ulSocketBufLen                     = 0;
unsigned long   CVacpProvider::m_ulIoctlOutBufLen                   = 0;
unsigned long   CVacpProvider::m_ulPortNo                           = 0;
char*           CVacpProvider::m_pSocketBuffer                      = NULL;
char*           CVacpProvider::m_pIoctlBuffer                       = NULL;
bool            CVacpProvider::m_bTagSentStatus                     = false;
bool            CVacpProvider::m_bSyncTag                           = false;
bool            CVacpProvider::m_bRemoteTagSend                     = false;
bool            CVacpProvider::m_bVerifyMode                        = false;
unsigned long   CVacpProvider::uSnapshotCount                       = 0;
PTAG_VOLUME_STATUS_OUTPUT_DATA CVacpProvider::m_pTagOutputData      = NULL;

static LONG lEventViewerLogKey                                      = 0;
static  HKEY hInMageEventViewerLogKey                               = NULL;
#if defined(_DEBUG)
DWORD g_dwEventViewerLoggingFlag                                    = 1;
#else
DWORD g_dwEventViewerLoggingFlag                                    = 0;
#endif

VSS_ID g_vacpSnapshotSetId                                          = GUID_NULL;

unsigned short CVacpProvider::m_usCount = 0;

#define INMAGE_VSS_PROVIDER_KEY                                     "SOFTWARE\\SV Systems\\InMageVssProvider\\"

#define INMAGE_VSS_PROVIDER_REMOTE_KEY                              "SOFTWARE\\SV Systems\\InMageVssProvider\\Remote\\"
#define INMAGE_VSS_PROVIDER_LOCAL_KEY                               "SOFTWARE\\SV Systems\\InMageVssProvider\\Local\\"
#define INMAGE_REMOTE_TAG_ISSUE                                     "Remote Tag Issue"
#define INMAGE_SYNC_TAG_ISSUE                                       "Sync Tag Issue"
#define INMAGE_TAG_REACHED_STATUS                                   "Tag Reached Status"
#define INMAGE_TAG_DATA_BUFFER                                      "Buffer"
#define INMAGE_TAG_DATA_BUFFER_LEN                                  "BufferLen"
#define INMAGE_OUT_BUFFER_LEN                                       "OutBufferLen"
#define INMAGE_REMOTE_TAG_PORT_NO                                   "Port No"
#define INMAGE_REMOTE_TAG_SERVER_IP                                 "Server Ip"
#define INMAGE_EVENTVIEWER_LOGGING_KEY                              "EventViewerLoggingFlag"

#define WIN2K8_SERVER_VER_MAJOR     0x06
#define WIN2K8_SERVER_VER_MINOR     0x00
#define WIN2K8_SERVER_VER_R2_MAJOR  0x06
#define WIN2K8_SERVER_VER_R2_MINOR  0x01

#define MAX_END_POINT_SIZE          50

using namespace std;

// CVacpProvider
CVacpProvider::CVacpProvider()
:m_setId(GUID_NULL)
,m_snapshotId(GUID_NULL)
,m_state(VSS_SS_UNKNOWN)
,m_lContext(0)
,m_ulQueriedLocalBufLen(0)
,m_ulQueriedSockBufLen(0)
,m_bSkipChkDriverMode(false)
,m_OverLapTagIO()
,m_wsaData()
,m_ClientSocket(INVALID_SOCKET)
,m_hControlDevice(INVALID_HANDLE_VALUE)
,hInMageRegKey(NULL)
,hInMageRemoteSubKey(NULL)
,hInMageLocalSubKey(NULL)
{
    ++m_usCount;
}

CVacpProvider::~CVacpProvider()
{
}


STDMETHODIMP CVacpProvider::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = {&IID_IVacpProvider};

    for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
        {
            return S_OK;
        }
    }
    return S_FALSE;
}

//Implementation of the IVacpProvider Interface methods
STDMETHODIMP CVacpProvider::IsItRemoteTagIssue(USHORT uRemoteSend)
{
    LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"\n uRemoteSend = %lu\n",uRemoteSend);
    if(uRemoteSend > 0)
    {
        m_bRemoteTagSend = true;
    }
    
    return S_OK;
}

STDMETHODIMP CVacpProvider::IsItSyncTagIssue(USHORT uSyncTag)
{
    LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"\n uSyncTag = %lu\n",uSyncTag);
    if(uSyncTag > 0)
    {
        m_bSyncTag = true;
    }
    return S_OK;
}

STDMETHODIMP CVacpProvider::EstablishRemoteConInfoForInMageVssProvider(USHORT uPort, unsigned char* serverIp)
{
    HRESULT hr = S_OK;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2,2), &m_wsaData);
    if (iResult != NO_ERROR)
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Error:[%d] Fail to initialize winsock.",iResult);
        hr = E_FAIL;
        return hr;
    }

    // Create a SOCKET for connecting to server
    m_ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ClientSocket == INVALID_SOCKET) 
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Error at socket(): %ld\n", WSAGetLastError());
        WSACleanup();
        return E_FAIL;
    }
    // The sockaddr_in structure specifies the address family,
    // IP address, and port of the server to be connected to.
    sockaddr_in clientService; 
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr((const char*) serverIp );
    clientService.sin_port = htons(uPort);


    LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"\n Server IP = %s\n Port No = %lu\n",serverIp,uPort);
    do 
    {
        if ( connect( m_ClientSocket, (SOCKADDR*) &clientService, sizeof(clientService) ) == SOCKET_ERROR) 
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Failed to connect: %ld\n", WSAGetLastError());
            WSACleanup();
            
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"FAILED Couldn't able to connect to remote server: %s, port: %d\n", serverIp, uPort );
            hr = E_FAIL;
            break;
        }
        else 
        {
            LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"\nConnected to %s at port %d\n",serverIp, uPort);
        }
    }while(0);
    return hr;
}

STDMETHODIMP CVacpProvider::StoreRemoteTagsBuffer(unsigned char* TagDataBuffer, ULONG ulTagBufferLen)
{
    HRESULT hr = S_OK;
    try
    {
        if(m_pSocketBuffer)
        {
            free((void*)m_pSocketBuffer);
        }
        m_pSocketBuffer = NULL;
        m_pSocketBuffer = (char*)malloc((size_t)ulTagBufferLen);
        if(m_pSocketBuffer)
        {
            memset((void*)m_pSocketBuffer,0x00,(size_t)ulTagBufferLen);
            inm_memcpy_s((void*)m_pSocketBuffer, (size_t)ulTagBufferLen, (const void*)TagDataBuffer, (size_t)ulTagBufferLen);
            m_ulSocketBufLen = ulTagBufferLen;
        }
        else
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nFailed to allocate memory for SocketBuffer. Error Code = %d",GetLastError());
            hr = E_FAIL;
        }
    }
    catch(...)
    {
        hr = E_FAIL;
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nCatch Block: Memory allocation failed.\n");
    }
    return hr;
}

STDMETHODIMP CVacpProvider::StoreLocalTagsBuffer(unsigned char* TagDataBuffer, ULONG ulTagBufferLen, ULONG ulOutBufLen)
{
    HRESULT hr = S_OK;
    try
    {
        if(m_pIoctlBuffer)
        {
            free(m_pIoctlBuffer);
        }
        m_pIoctlBuffer = NULL;
        m_pIoctlBuffer = (char*)malloc((size_t)ulTagBufferLen);
        if(m_pIoctlBuffer)
        {
            memset((void*)m_pIoctlBuffer,0x00,(size_t)ulTagBufferLen);
            inm_memcpy_s((void*)m_pIoctlBuffer, (size_t)ulTagBufferLen, (const void*)TagDataBuffer, (size_t)ulTagBufferLen);
            m_ulIoctlBufLen = ulTagBufferLen;
            m_ulIoctlOutBufLen = ulOutBufLen;

            //Allocate memory for out buffer
            m_pTagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(m_ulIoctlOutBufLen);
            if(!m_pTagOutputData)
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Failed to allocate memory for the Tagoutput data.Error Code = %d",GetLastError());
                hr = E_FAIL;
            }
            else
            {
                memset((void*)m_pTagOutputData,0x00,(size_t)m_ulIoctlOutBufLen);
            }
        }
        else
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Memory allocation failed. Error Code = %d",GetLastError());
            hr = E_FAIL;
        }
    }
    catch(...)
    {
        hr = E_FAIL;
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nCatch Block: Memory allocation failed for the local buffer.\n");
    }
    return hr;
}

STDMETHODIMP CVacpProvider::IsTagIssuedSuccessfullyDuringCommitTime(USHORT* ulTagSent)
{
    HRESULT hr = S_OK;
    if(true == m_bTagSentStatus)
    {
        *ulTagSent = 1;//successfully sent the tag during commit time
        LogVssEvent(EVENTLOG_SUCCESS,1,"\n InMageVssProvider successfully issued the Tag.\n");
        hr = S_OK;
    }
    else
    {
        *ulTagSent = 0;//failed to send the tag during the commit time.
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n InMageVssProvider failed to issue the Tag.\n");
        hr = E_FAIL;
    }
    return hr;
}
STDMETHODIMP CVacpProvider::SetVerifyMode()
{
    HRESULT hr = S_OK;
    m_bVerifyMode = true;
    return hr;
}


STDMETHODIMP CVacpProvider::RegisterSnapshotSetId(VSS_ID snapshotSetId)
{
    HRESULT hr = S_OK;
    g_vacpSnapshotSetId = snapshotSetId;
    return hr;
}

// IVssProviderCreateSnapshotSet methods
STDMETHODIMP CVacpProvider::EndPrepareSnapshots(IN VSS_ID SnapshotSetId)
{
    HRESULT hr = S_OK;
    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotSetId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "EndPrepareSnapshots();SnapshotSetId = %s\n Prepared for %d snapshots.", pszSnapshotId, m_vVssObjectProps.size());
        RpcStringFree(&pszSnapshotId);
    }
    m_state = VSS_SS_PREPARED;

    return hr;
}

STDMETHODIMP CVacpProvider::PreCommitSnapshots(IN VSS_ID SnapshotSetId)
{    
    HRESULT hr = S_OK;
    m_state = VSS_SS_PROCESSING_PRECOMMIT;

    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotSetId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "PreCommitSnapshots();SnapshotSetId = %s", pszSnapshotId);
        RpcStringFree(&pszSnapshotId);
    }

    if(m_bVerifyMode)//for verify mode we need not read registry data as we will not issue any tag!
    {
        m_state = VSS_SS_PRECOMMITTED;
        return hr;
    }

    return hr;
}

STDMETHODIMP CVacpProvider::CommitSnapshots(IN VSS_ID SnapshotSetId)
{
    HRESULT hr = S_OK;
    m_state = VSS_SS_PROCESSING_COMMIT;

    unsigned char *pszSnapshotSetId;
    RPC_STATUS status = UuidToString(&SnapshotSetId, &pszSnapshotSetId);
    
    if (status == RPC_S_OK)
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "CommitSnapshots();SnapshotSetId = %s", pszSnapshotSetId);
    }
    
    if(m_bVerifyMode)
    {
        m_state = VSS_SS_COMMITTED;
        m_bTagSentStatus = true;
        return hr;
    }

    if (status != RPC_S_OK)
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "CommitSnapshots(): UuidToString failed: %d\n", GetLastError());
        return S_FALSE;
    }


    HRESULT rpchr = RunVacpRpc(pszSnapshotSetId, HandleQuiescePhase);
    
    if (rpchr == S_OK)
    {

        m_state = VSS_SS_COMMITTED;
        m_bTagSentStatus = true;
        hr = S_OK;
    }
    else
    {
        m_bTagSentStatus = false;
        hr = S_FALSE;
    }

    RpcStringFree(&pszSnapshotSetId);

    LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "CommitSnapshots(); Committed %d snapshots.", m_vVssObjectProps.size());

    return hr;
}


STDMETHODIMP CVacpProvider::PostCommitSnapshots(IN VSS_ID SnapshotSetId,IN LONG lSnapshotsCount)
{
    HRESULT hr = S_OK;
    m_state = VSS_SS_PROCESSING_POSTCOMMIT;
    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotSetId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "PostCommitSnapshots();SnapshotSetId = %s\n SnapshotsCount = %d \n Snapshots with the provider %d", pszSnapshotId, lSnapshotsCount, m_vVssObjectProps.size());
        RpcStringFree(&pszSnapshotId);
    }
    
    return hr;
}

void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len)
{
    return(malloc(len));
}

void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
{
    free(ptr);
}

// The following two methods are stubs for Windows Server 2003, and they should ideally return only S_OK
STDMETHODIMP CVacpProvider::PreFinalCommitSnapshots(IN VSS_ID SnapshotSetId )
{
    HRESULT hr = S_OK;
    m_state = VSS_SS_PROCESSING_PREFINALCOMMIT;

    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotSetId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "PreFinalCommitSnapshots();SnapshotSetId = %s\n Number of snapshots in set the %d", pszSnapshotId, m_vVssObjectProps.size());
        RpcStringFree(&pszSnapshotId);
    }
    m_state = VSS_SS_PREFINALCOMMITTED;
    return hr;
}

STDMETHODIMP CVacpProvider::PostFinalCommitSnapshots(IN VSS_ID SnapshotSetId)
{
    HRESULT hr = S_OK;
    m_state = VSS_SS_PROCESSING_POSTFINALCOMMIT;

    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotSetId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "PostFinalCommitSnapshots();SnapshotSetId = %s\n Number of snapshots in the set %d", pszSnapshotId, m_vVssObjectProps.size());
        RpcStringFree(&pszSnapshotId);
    }

    if(m_bVerifyMode)
    {
        m_bVerifyMode = false;
        m_bTagSentStatus = true;
        m_state = VSS_SS_CREATED;
        return hr;
    }
    try
    {
        if(m_bRemoteTagSend)
        {
            if(m_pSocketBuffer)
            {
                free((void*)m_pSocketBuffer);
                m_pSocketBuffer = NULL;
            }
            if(hInMageRemoteSubKey)
            {
                RegCloseKey(hInMageRemoteSubKey);
                hInMageRemoteSubKey = NULL;
            }
            closesocket(m_ClientSocket);
            m_bRemoteTagSend = false;
            m_bSyncTag = false;
        }
        else
        {
            if(m_pIoctlBuffer)
            {
                free((void*)m_pIoctlBuffer);
                m_pIoctlBuffer = NULL;
            }
            if(m_pTagOutputData)
            {
                free(m_pTagOutputData);
                m_pTagOutputData = NULL;
            }
            if(hInMageLocalSubKey)
            {
                RegCloseKey(hInMageLocalSubKey);
                hInMageLocalSubKey = NULL;
            }
            m_bRemoteTagSend = false;
            m_bSyncTag = false;
            m_bVerifyMode = false;
            m_bTagSentStatus = false;
        }
    }
    catch(...)
    {
        hr = E_FAIL;
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Could not clean up the memory created on heap, returning 0x%08x\n", hr);
    }    
    if(SUCCEEDED(hr))
    {
        m_state = VSS_SS_CREATED;
    }

    auto vssObjIter = m_vVssObjectProps.begin();
    while (vssObjIter != m_vVssObjectProps.end())
    {
        vssObjIter->Obj.Snap.m_eStatus = m_state;
        vssObjIter++;
    }

    
    return hr;
}



STDMETHODIMP CVacpProvider::AbortSnapshots(IN VSS_ID SnapshotSetId)
{
    HRESULT hr = S_OK;
    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotSetId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "AbortSnapshots();SnapshotSetId = %s", pszSnapshotId);
        RpcStringFree(&pszSnapshotId);
    }
    uSnapshotCount = 0;

    m_bVerifyMode = false;
    m_state = VSS_SS_ABORTED;
    m_bTagSentStatus = false;
    
    auto vssObjIter = m_vVssObjectProps.begin();
    while (vssObjIter != m_vVssObjectProps.end())
    {
        vssObjIter->Obj.Snap.m_eStatus = m_state;
        vssObjIter++;
    }

    return hr;
}

// IVssProvider Notifications methods
STDMETHODIMP CVacpProvider::OnLoad(IN IUnknown * )
{
    //Create an entry in the registry for controlling the logging in the event viewer
    lEventViewerLogKey = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                       INMAGE_VSS_PROVIDER_KEY,
                                       0,
                                       KEY_READ|KEY_WOW64_64KEY,
                                       (PHKEY)&hInMageEventViewerLogKey);
    if(ERROR_SUCCESS != lEventViewerLogKey)
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"\nCould not Create/Open EventViewerLoggingFlag in the registry.\n");
    }
    return S_OK;
}
STDMETHODIMP CVacpProvider::OnUnload(IN BOOL )
{
    HRESULT hr = S_OK;
    uSnapshotCount = 0;
    try
    {
        if(m_pSocketBuffer)
        {
            free((void*)m_pSocketBuffer);
            m_pSocketBuffer = NULL;
        }
        if(m_pIoctlBuffer)
        {
            free((void*)m_pIoctlBuffer);
            m_pIoctlBuffer = NULL;
        }
        
        if(m_pTagOutputData)
        {
            free(m_pTagOutputData);
            m_pTagOutputData = NULL;
        }

        if(hInMageRemoteSubKey)
        {
            RegCloseKey(hInMageRemoteSubKey);
            hInMageRemoteSubKey = NULL;
        }
        if(hInMageLocalSubKey)
        {
            RegCloseKey(hInMageLocalSubKey);
            hInMageLocalSubKey = NULL;
        }
        if(hInMageRegKey)
        {
            RegCloseKey(hInMageRegKey);
            hInMageLocalSubKey = NULL;
        }
    }
    catch(...)
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Could not clean up the memory created on heap!\n");
        hr = E_FAIL;
    }    
    return hr;
}

//IVssSoftwareSnapshotProvider methods
STDMETHODIMP CVacpProvider::SetContext(LONG lContext)
{
    HRESULT hr = S_OK;
    m_lContext = lContext;
    return hr;
}

STDMETHODIMP CVacpProvider::GetSnapshotProperties(VSS_ID SnapshotId,VSS_SNAPSHOT_PROP *pProp)
{
    HRESULT hr = S_OK;
    bool bFound = false;

    unsigned char *pszSnapshotId;
    if (RPC_S_OK == UuidToString(&SnapshotId, &pszSnapshotId))
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "GetSnapshotProperties(); SnapshotId = %s\n", pszSnapshotId);
        RpcStringFree(&pszSnapshotId);
    }


    vector<VSS_OBJECT_PROP>::iterator iter = m_vVssObjectProps.begin();
    while(iter != m_vVssObjectProps.end())
    {
        if(VSS_OBJECT_SNAPSHOT == iter->Type )
        {
            if(IsEqualGUID(SnapshotId,iter->Obj.Snap.m_SnapshotId))
            {
                pProp->m_lSnapshotsCount = iter->Obj.Snap.m_lSnapshotsCount;
                pProp->m_eStatus = m_state;
                pProp->m_lSnapshotAttributes = iter->Obj.Snap.m_lSnapshotAttributes;
                pProp->m_ProviderId = iter->Obj.Snap.m_ProviderId;
                pProp->m_SnapshotId = iter->Obj.Snap.m_SnapshotId;
                pProp->m_SnapshotSetId = iter->Obj.Snap.m_SnapshotSetId;
                bFound = true;
                break;
            }
        }
        iter++;
    }

    const char * pcFound;

    if(bFound)
    {
        pcFound = "Found";
    }
    else
    {
        hr = VSS_E_OBJECT_NOT_FOUND;
        pcFound = "Not found";
    }
        
    LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "GetSnapshotProperties() Snapshot is %s. returning 0x%08x, Snapshot state is %d. Number of snapshots %d.\n", pcFound, hr, m_state, m_vVssObjectProps.size());

    return hr;
}

STDMETHODIMP CVacpProvider::Query (VSS_ID QueriedObjectId,
                      VSS_OBJECT_TYPE eQueriedObjectType,
                      VSS_OBJECT_TYPE eReturnedObjectsType,
                      IVssEnumObject **ppEnum)
{
    CComPtr<IVssEnumObject> pEnumSnapshots = NULL;

    HRESULT hr = this->QueryInterface(IID_IVssEnumObject, (LPVOID *)&pEnumSnapshots);

    if(SUCCEEDED(hr))
    {
        pEnumSnapshots->Reset();

        *ppEnum = pEnumSnapshots.Detach();
    }
    LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "Query(): count = %d, state = %d, number of snapshots = %d\n returning 0x%08x.", m_usCount, m_state, m_vVssObjectProps.size() ,hr);

    return hr;
}

STDMETHODIMP CVacpProvider::DeleteSnapshots(VSS_ID SourceObjectId,
                                 VSS_OBJECT_TYPE eSourceObjectType,
                                 BOOL bForceDelete,
                                 LONG *plDeletedSnapshots,
                                 VSS_ID *pNondeletedSnapshotID)
{
    HRESULT hr = S_OK;
    RPC_STATUS status;
    unsigned char *pszSnapshotId;
    status = UuidToString(&SourceObjectId, &pszSnapshotId);

    if (eSourceObjectType == VSS_OBJECT_SNAPSHOT_SET)
    {
        if (status == RPC_S_OK)
            LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "DeleteSnapshots(); SnapshotSetId = %s\n", pszSnapshotId);

        unsigned long snapshotCount = 0;
        auto vssObjIter = m_vVssObjectProps.begin();
        while (vssObjIter != m_vVssObjectProps.end())
        {
            if (IsEqualGUID(vssObjIter->Obj.Snap.m_SnapshotSetId, SourceObjectId))
            {
                vssObjIter->Obj.Snap.m_eStatus = VSS_SS_DELETED;
                snapshotCount++;
            }
            vssObjIter++;
        }
        if (snapshotCount > 0)
        {
            *plDeletedSnapshots = snapshotCount;
            *pNondeletedSnapshotID = SourceObjectId;
        }
    }
    else if (eSourceObjectType == VSS_OBJECT_SNAPSHOT)
    {
        if (status == RPC_S_OK)
            LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "DeleteSnapshots(); SnapshotId = %s\n", pszSnapshotId);
        auto vssObjIter = m_vVssObjectProps.begin();
        while (vssObjIter != m_vVssObjectProps.end())
        {
            if (IsEqualGUID(vssObjIter->Obj.Snap.m_SnapshotId, SourceObjectId))
            {
                vssObjIter->Obj.Snap.m_eStatus = VSS_SS_DELETED;
                *plDeletedSnapshots = 1;
                *pNondeletedSnapshotID = SourceObjectId;
                break;
            }
            vssObjIter++;
        }
    }

    m_state = VSS_SS_DELETED;

    if (status == RPC_S_OK)
    {
        RpcStringFree(&pszSnapshotId);
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "Deleted %d snapshots.\n returning 0x%08x", *plDeletedSnapshots, hr);
    }
    return hr;
}


STDMETHODIMP CVacpProvider::BeginPrepareSnapshot(VSS_ID SnapshotSetId,
                                     VSS_ID SnapshotId,
                                     VSS_PWSZ pwszVolumeName,
                                     LONG lNewContext)
{
    HRESULT hr = S_OK;
    
    m_setId = SnapshotSetId;
    m_snapshotId = SnapshotId;
    m_state = VSS_SS_PREPARING;
    m_lContext = lNewContext;
    LSTATUS lStatus    = 0;
    DWORD   dwValue    = 0;
    DWORD   dwType     = REG_DWORD;
    DWORD   dwOutValue = sizeof(DWORD);
    lStatus = RegQueryValueEx(  hInMageEventViewerLogKey,
                                    INMAGE_EVENTVIEWER_LOGGING_KEY,
                                    0,
                                    &dwType,
                                    (LPBYTE)&dwValue,
                                    &dwOutValue);
    if(ERROR_SUCCESS != lStatus)
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"\nCould not query the EventViewerLoggingFlag's value.\n");
    }
    else
    {
        g_dwEventViewerLoggingFlag = dwValue;
    }
    
    VSS_OBJECT_PROP g_vssObject;
    g_vssObject.Type = VSS_OBJECT_SNAPSHOT;
    g_vssObject.Obj.Snap.m_SnapshotId = m_snapshotId;
    g_vssObject.Obj.Snap.m_SnapshotSetId = m_setId;
    g_vssObject.Obj.Snap.m_lSnapshotsCount = 1;
    g_vssObject.Obj.Snap.m_ProviderId = (VSS_ID)gVssProviderId;
    g_vssObject.Obj.Snap.m_pwszSnapshotDeviceObject = (VSS_PWSZ)"";
    g_vssObject.Obj.Snap.m_pwszOriginalVolumeName = (VSS_PWSZ)"";
    g_vssObject.Obj.Snap.m_pwszOriginatingMachine = (VSS_PWSZ)"";
    g_vssObject.Obj.Snap.m_pwszServiceMachine = (VSS_PWSZ)"";
    g_vssObject.Obj.Snap.m_pwszExposedName = (VSS_PWSZ)"";
    g_vssObject.Obj.Snap.m_pwszExposedPath = (VSS_PWSZ)"";
    g_vssObject.Obj.Snap.m_lSnapshotAttributes = 0; // VSS_CTX_BACKUP
    g_vssObject.Obj.Snap.m_tsCreationTimestamp = 0;
    g_vssObject.Obj.Snap.m_eStatus = m_state;

    m_vVssObjectProps.push_back(g_vssObject);

    unsigned char *pszSnapshotId,*pszSnapshotId1;
    UuidToString(&SnapshotSetId, &pszSnapshotId);
    UuidToString(&SnapshotId, &pszSnapshotId1);
    LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"BeginPrepareSnapshot();SnapshotSetId = %s\n SnapshotId = %s\n Total snapshots in this set %d",pszSnapshotId,pszSnapshotId1, m_vVssObjectProps.size());
    RpcStringFree(&pszSnapshotId);
    RpcStringFree(&pszSnapshotId1);

    return hr;
}

STDMETHODIMP CVacpProvider::IsVolumeSupported(VSS_PWSZ pwszVolumeName,
                                 BOOL *pbSupportedByThisProvider)
{
    HRESULT hr = S_OK;

    unsigned char *pszSnapshotSetId;

    *pbSupportedByThisProvider = FALSE;

    if (IsEqualGUID(g_vacpSnapshotSetId,GUID_NULL))
    {
        return hr;
    }

    RPC_STATUS status = UuidToString(&g_vacpSnapshotSetId, &pszSnapshotSetId);

    if (status == RPC_S_OK)
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "IsVolumeSupported();SnapshotSetId = %s", pszSnapshotSetId);
    }

    if (status != RPC_S_OK)
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "IsVolumeSupported(): UuidToString failed: %d\n", GetLastError());
        return hr;
    }

    HRESULT rpchr = RunVacpRpc(pszSnapshotSetId, IsSnapshotFromVacp);

    if (rpchr == S_OK)
    {
        *pbSupportedByThisProvider = TRUE;
    }

    return hr;
}

STDMETHODIMP CVacpProvider::IsVolumeSnapshotted(VSS_PWSZ pwszVolumeName,
                                    BOOL *pbSnapshotsPresent,
                                    LONG *plSnapshotCompatibility)
{
    HRESULT hr = S_OK;
    *pbSnapshotsPresent = FALSE;
    return hr;
}


STDMETHODIMP CVacpProvider::SetSnapshotProperty(VSS_ID SnapshotId,
                                   VSS_SNAPSHOT_PROPERTY_ID eSnapshotPropertyId,
                                   VARIANT vProperty)
{
    HRESULT hr = S_OK;
    unsigned char *pszSnapshotId;
    UuidToString(&SnapshotId, &pszSnapshotId);
    LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"SetSnapshotProperty();SnapshotSetId = %s",pszSnapshotId);
    RpcStringFree(&pszSnapshotId);
    return hr;
}

STDMETHODIMP CVacpProvider::RevertToSnapshot( VSS_ID SnapshotId)
{
    HRESULT hr = S_OK;
    return hr;
}


STDMETHODIMP CVacpProvider::QueryRevertStatus( VSS_PWSZ pwszVolume,
                                  IVssAsync **ppAsync)
{
    HRESULT hr = S_OK;
    return hr;
}

//Private Methods
void CVacpProvider::DeleteAbortedSnapshots()
{
    return;
}

HRESULT CVacpProvider::SendTagsToDriver(unsigned char* TagBuffer, unsigned long ulTagBufLen,unsigned long ulOutBufLen)
{
    HRESULT hr = S_OK;
    using std::string;
    
    // Open the filter driver
    m_hControlDevice = CreateFile(INMAGE_FILTER_DOS_DEVICE_NAME,
                                    GENERIC_READ |
                                    GENERIC_WRITE, FILE_SHARE_READ | 
                                    FILE_SHARE_WRITE,
                                    NULL, 
                                    OPEN_EXISTING, 
                                    FILE_FLAG_OVERLAPPED, 
                                    NULL ); 

    // Check if the handle is valid or not
    if( m_hControlDevice == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"FAILED Couldn't get handle to filter device, err %d\n", GetLastError() );
        return hr;
    }
    //Issue IOCTL to driver
    BOOL    bResult = false;
    DWORD    dwReturn = 0;
    DWORD   IoctlCmd = IOCTL_INMAGE_TAG_VOLUME;
    ULONG   ulOutputLength = ulOutBufLen;
    
    if(!m_pTagOutputData)
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n The out tag buffer data is not avaialable.\n");
        hr = E_FAIL;
        return hr;
    }
    bResult = DeviceIoControl(
                               m_hControlDevice,
                               (DWORD)IoctlCmd,
                               (LPVOID)TagBuffer,
                               ulTagBufLen,
                               (PVOID)m_pTagOutputData,
                                ulOutputLength,
                               &dwReturn,
                               &m_OverLapTagIO //a valid OVERLAPPED structure for getting event completion notification
                               );
   if(FALSE == bResult)//when isuing of IOCTL failed
   {
        m_bTagSentStatus  = false;
        if (ERROR_IO_PENDING != GetLastError())
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "FAILED to issue IOCTL_INMAGE_TAG_VOLUME ioctl. Error code:%d\n", GetLastError());
            hr = E_FAIL;
        }
        else//Pending IO
        {
            m_bTagSentStatus = true;
            hr = S_OK;
        }
   }
   else//when IOCTL is issued successfully
   {   
        m_bTagSentStatus = true;
        if (GetOverlappedResult(m_hControlDevice, &m_OverLapTagIO, &dwReturn, FALSE))
        {
            hr = S_OK;
        }
        else
        {
            m_bTagSentStatus = false;
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Failed to commit tags to one or all volumes\n");
            hr = E_FAIL;
        }
   }
    if( !(m_hControlDevice == INVALID_HANDLE_VALUE) )
    {
        CloseHandle(m_hControlDevice);
        m_hControlDevice = NULL;
    }
    return hr;
}

HRESULT CVacpProvider::ReadRegistryForTagData()
{
    HRESULT hr = S_OK;
    do
    {
        LSTATUS lStatus = 0;
        DWORD dwValue = 0;
        DWORD dwType = REG_DWORD;
        DWORD dwOutValue= sizeof(DWORD);
        DWORD dwOutDataLen = sizeof(DWORD);

        if(m_bRemoteTagSend)
        {
            if(!m_bSyncTag)
            {
                //Query the registry for REMOTE and read the buffer and buffer length to be sent over socket to the VACP server
                LONG lResCreateInMageRemoteSubKey = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                                                INMAGE_VSS_PROVIDER_REMOTE_KEY,
                                                                0,
                                                                KEY_READ|KEY_WOW64_64KEY,
                                                                (PHKEY)&hInMageRemoteSubKey);
                if(ERROR_SUCCESS != lResCreateInMageRemoteSubKey)
                {
                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nCould not access create/set \"Remote \" sub key in the registry.Error Code = %lu",lResCreateInMageRemoteSubKey);
                    hr = E_FAIL;
                    break;
                }
                else//if the Remote key is opened successfully
                {
                    //first query the length of the remote buffer
                    dwType = REG_BINARY;
                    dwOutDataLen = 0;
                    lStatus = RegQueryValueEx(hInMageRemoteSubKey,
                                                INMAGE_TAG_DATA_BUFFER,
                                                0,
                                                &dwType,
                                                NULL,
                                                (LPDWORD)&dwOutDataLen);
                    if(ERROR_SUCCESS == lStatus)
                    {
                        try
                        {
                            m_pSocketBuffer = (char*)malloc((size_t)dwOutDataLen);
                            if(m_pSocketBuffer)
                            {
                                memset((void*)m_pSocketBuffer,0x00,(size_t)dwOutDataLen);
                                lStatus = RegQueryValueEx(hInMageRemoteSubKey,
                                                            INMAGE_TAG_DATA_BUFFER,
                                                            0,
                                                            &dwType,
                                                            (LPBYTE)m_pSocketBuffer,
                                                            (LPDWORD)&dwOutDataLen);
                                if(ERROR_SUCCESS != lStatus)
                                {
                                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Error in reading the buffer from registry for sending tags to remote server.\n");
                                    hr = E_FAIL;
                                    break;
                                }
                                else
                                {
                                    //Store the earlier queried buffer length
                                    m_ulQueriedSockBufLen = dwOutDataLen;
                                    //Read the buffer length also from the registry
                                    dwType = REG_DWORD;
                                    dwValue = 0;
                                    dwOutDataLen = sizeof(DWORD);
                                    lStatus = RegQueryValueEx (hInMageRemoteSubKey,
                                                                INMAGE_TAG_DATA_BUFFER_LEN,
                                                                0,
                                                                &dwType,
                                                                (LPBYTE)&dwValue,
                                                                &dwOutDataLen);
                                    if(ERROR_SUCCESS != lStatus)
                                    {
                                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError in querying the length of remote tags buffer from regisry.\n");
                                        hr = E_FAIL;
                                        break;
                                    }
                                    else
                                    {
                                        //compare the queried length of the buffer and that of the actual length
                                        if(m_ulQueriedSockBufLen != dwValue)
                                        {
                                            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError: Discrepancy: Actual remote buffer length = %lu; Queried length = %lu\n",dwValue,m_ulQueriedSockBufLen);
                                            hr = E_FAIL;
                                            break;
                                        }
                                        else
                                        {
                                            m_ulSocketBufLen = dwValue;
                                        }
                                    }
                                }
                                
                                //Query the Port No
                                dwType = REG_DWORD;
                                dwValue = 0;
                                dwOutDataLen = sizeof(DWORD);
                                lStatus = RegQueryValueEx( hInMageRemoteSubKey,
                                                            INMAGE_REMOTE_TAG_PORT_NO,
                                                            0,
                                                            &dwType,
                                                            (LPBYTE)&dwValue,
                                                            &dwOutDataLen);
                                if(ERROR_SUCCESS != lStatus)
                                {
                                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError in querying the Server Port No from regisry.\n");
                                    hr = E_FAIL;
                                    break;
                                }
                                else
                                {
                                    m_ulPortNo = dwValue;
                                }

                                //Query the Server Ip
                                dwType = REG_SZ;
                                dwOutDataLen = 0;
                                //First get the required size
                                lStatus = RegQueryValueEx ( hInMageRemoteSubKey,
                                                            INMAGE_REMOTE_TAG_SERVER_IP,
                                                            0,
                                                            &dwType,
                                                            NULL,
                                                            (LPDWORD)&dwOutDataLen);
                                if(ERROR_SUCCESS == lStatus)
                                {
                                    //The required length os Server Ip is now obtained in dwOutDataLen
                                    try
                                    {
                                        if(m_pServerIp)
                                        {
                                            memset((void*)m_pServerIp,0x00,(size_t)(dwOutDataLen + 1));
                                            lStatus = RegQueryValueEx( hInMageRemoteSubKey,
                                                                        INMAGE_REMOTE_TAG_SERVER_IP,
                                                                        0,
                                                                        &dwType,
                                                                        (LPBYTE)m_pServerIp,
                                                                        (LPDWORD)&dwOutDataLen);
                                            if(ERROR_SUCCESS != lStatus)
                                            {
                                                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Error in reading the ServerIp from the registry.Error = %08x\n",lStatus);
                                                hr = E_FAIL;
                                                break;
                                            }
                                            else
                                            {
                                                //m_pServerIp contains the IP address or host name of the Vacp Server
                                                //Now establish a Socket Connection with the Vacp Server.
                                                hr = EstablishRemoteConInfoForInMageVssProvider((USHORT)m_ulPortNo,(unsigned char*)m_pServerIp);
                                                if(hr != S_OK)
                                                {
                                                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nFailed to establish socket connection with the Vacp Server. Error = %08x\n",hr);
                                                    break;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Memory could not be allocated for %d bytes to store Vacp Server Ip/Host.\n Error = %lu\n",dwOutDataLen,GetLastError());
                                            hr = E_FAIL;
                                            break;
                                        }
                                    }
                                    catch(...)
                                    {
                                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Memory allocation failure.\n");
                                        hr = E_FAIL;
                                        break;
                                    }
                                }
                            }
                        }
                        catch(std::bad_alloc)
                        {
                            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nBad memory allocated.\n");
                            hr = E_FAIL;
                            break;
                        }
                        catch(HRESULT hre)
                        {
                            hr = hre;
                            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nFailed.Invalid handle. Error Code = %08x\n",hr);
                            break;
                        }
                        catch(...)
                        {
                            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Could not allocate memory to read the Remote Tags data buffer.\n");
                            hr = E_FAIL;
                            break;
                        }
                    }
                }
            }
        }
        
        else //if we have to send tag locally using ioctl
        {
            //Query the registry for LOCAL and read the buffer,buffer length and the out buffer length  to be sent to the locally available InVolFlt Driver
            LONG lResCreateInMageLocalSubKey = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                                            INMAGE_VSS_PROVIDER_LOCAL_KEY,
                                                            0,
                                                            KEY_READ|KEY_WOW64_64KEY,
                                                            (PHKEY)&hInMageLocalSubKey);
            if(ERROR_SUCCESS != lResCreateInMageLocalSubKey)
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nCould not accesscreate/set \"Local \" sub key in the registry.Error Code = %lu",lResCreateInMageLocalSubKey);
                hr = E_FAIL;
                break;
            }
            else//on successfully opening the LOCAL subkey
            {
                //Read the buffer, buffer length and the outbuffer length
                dwType = REG_BINARY;
                dwOutDataLen = 0;
                lStatus = RegQueryValueEx(hInMageLocalSubKey,
                                            INMAGE_TAG_DATA_BUFFER,
                                            0,
                                            &dwType,
                                            NULL,
                                            (LPDWORD)&dwOutDataLen);
                if(ERROR_SUCCESS == lStatus)//on successfully getting the length in dwOutDataLen
                {
                    try
                    {
                        m_pIoctlBuffer = (char*)malloc((size_t)dwOutDataLen);
                        if(m_pIoctlBuffer)
                        {
                            memset((void*)m_pIoctlBuffer,0x00,(size_t)dwOutDataLen);
                            lStatus = RegQueryValueEx( hInMageLocalSubKey,
                                                        INMAGE_TAG_DATA_BUFFER,
                                                        0,
                                                        &dwType,
                                                        (LPBYTE)m_pIoctlBuffer,
                                                        (LPDWORD)&dwOutDataLen);
                            if(ERROR_SUCCESS != lStatus)
                            {
                                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Error in reading the buffer from registry for sending tags to remote server.\n");
                                hr = E_FAIL;
                                break;
                            }
                            else
                            {

                                //Store the earlier queried buffer length
                                m_ulQueriedLocalBufLen = dwOutDataLen;
                                //Read the buffer length also from the registry
                                dwType = REG_DWORD;
                                dwValue = 0;
                                dwOutDataLen = sizeof(DWORD);
                                lStatus = RegQueryValueEx (hInMageLocalSubKey,
                                                            INMAGE_TAG_DATA_BUFFER_LEN,
                                                            0,
                                                            &dwType,
                                                            (LPBYTE)&dwValue,
                                                            &dwOutDataLen);
                                if(ERROR_SUCCESS != lStatus)
                                {
                                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError in querying the length of local tags buffer from regisry.\n");
                                    hr = E_FAIL;
                                    break;
                                }
                                else
                                {
                                    //compare the queried length of the buffer and that of the actual length
                                    if(m_ulQueriedLocalBufLen != dwValue)
                                    {
                                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError:There is discrepancy in the actual local buffer length and the queried length.\n");
                                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError:Actual local buffer length = %lu \n Queried length = %lu\n",dwValue,m_ulQueriedLocalBufLen);
                                        hr = E_FAIL;
                                        break;
                                    }
                                    else
                                    {
                                        //store the local buffer length for issuing tags locally using Ioctl
                                        m_ulIoctlBufLen = dwValue;
                                        //query the OutBufferLength also from the registry
                                        dwType = REG_DWORD;
                                        dwValue = 0;
                                        dwOutDataLen = sizeof(DWORD);
                                        lStatus = RegQueryValueEx( hInMageLocalSubKey,
                                                                    INMAGE_OUT_BUFFER_LEN,
                                                                    0,
                                                                    &dwType,
                                                                    (LPBYTE)&dwValue,
                                                                    &dwOutDataLen);
                                        if(ERROR_SUCCESS != lStatus)
                                        {
                                            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError in querying the OutBufferLength from the registry.\n");
                                            hr = E_FAIL;
                                            break;
                                        }
                                        else
                                        {
                                            m_ulIoctlOutBufLen = dwValue;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    catch(std::bad_alloc)
                    {
                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nBad memory allocated during registry access for Local Tag Information.\n");
                        hr = E_FAIL;
                        break;
                    }
                    catch(HRESULT hre)
                    {
                        hr = hre;
                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nFailed.Invalid handle while reading the registry for Local Tag information. Error Code = %08x\n",hr);
                        break;
                    }
                    catch(...)
                    {
                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Could not allocate memory to read the Local Tags data buffer.\n");
                        hr = E_FAIL;
                        break;
                    }
                }
            }
        }
    }
    while(FALSE);
    return hr;
}
HRESULT CVacpProvider::CommitSnapshotsWithRegistryProcessing()
{
    HRESULT hr = S_OK;
    do
    {
        if(m_bRemoteTagSend)
        {
            if(!m_bSyncTag)
            {
                if((m_pSocketBuffer != NULL) && (m_ulSocketBufLen > 0))
                {
                    //Send the tags to the remote server
                    hr = SendTagsToRemoteServer((unsigned char*)m_pSocketBuffer,m_ulSocketBufLen);
                    if(hr != S_OK)
                    {
                        LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError:Failed to send the tags to the remote server.\n");
                        m_bTagSentStatus = false;
                        hr = E_FAIL;
                        break;
                    }
                    else
                    {
                        LogVssEvent(EVENTLOG_SUCCESS,1,"\nSuccessfully sent the tags to the remote server.\n");
                        m_bTagSentStatus = true;
                    }
                }
                else
                {
                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Error:Either buffer is invalid or the buffer length is invalid.\n");
                    hr = E_FAIL;
                    break;
                }
            }
        }
        else //if we have to send tag locally using ioctl
        {
            if((m_pIoctlBuffer != NULL) && (m_ulIoctlBufLen > 0) && (m_ulIoctlOutBufLen > 0))
            {
                m_pTagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(m_ulIoctlOutBufLen);  
                if(!m_pTagOutputData)
                {
                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Failed to allocate memory for the Tagoutput data.Error Code = %d",GetLastError());
                    hr = E_FAIL;
                    break;
                }
                else
                {
                    memset((void*)m_pTagOutputData,0x00,(size_t)m_ulIoctlOutBufLen);
                }
                //Send tags to the local InVolFlt Driver using ioctl
                hr = SendTagsToDriver((unsigned char*)m_pIoctlBuffer,m_ulIoctlBufLen,m_ulIoctlOutBufLen);
                if(hr != S_OK)
                {
                    LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\nError:Failed to send the tags to the remote server.\n");
                    m_bTagSentStatus = false;
                    hr = E_FAIL;
                    break;
                }
                else
                {
                    LogVssEvent(EVENTLOG_SUCCESS,1,"\nSuccessfully sent the tags to the local driver.\n");
                    m_bTagSentStatus = true;
                }
            }
            else
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"\n Error:Either buffer is invalid or the buffer length is invalid or its outbuffer length is invalid.\n");
                hr = E_FAIL;
                break;
            }
        }
    }
    while(FALSE);
    return hr;
}

HRESULT CVacpProvider::SendTagsToRemoteServer(unsigned char* TagBuffer, unsigned long ulTagBufLen)
{
    HRESULT hr = S_OK;
    if(m_bRemoteTagSend)
    {
        using std::string;
        USES_CONVERSION;    // declare locals used by the ATL macros
        
        //Issue packets to remote server
        BOOL    bResult = false;
        DWORD    dwReturn = 0;
        
        VACP_SERVER_RESPONSE tVacpServerResponse;

        unsigned long tmplongdata = htonl(ulTagBufLen);
        do 
        {
            if (::send(m_ClientSocket,(char *)&tmplongdata, sizeof tmplongdata,0) == SOCKET_ERROR)
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"FAILED Couldn't able to send total Tag data length to remote server");
                hr = E_FAIL;
                break;
            }
        
            else if (::send(m_ClientSocket,(char*)TagBuffer, ulTagBufLen,0) == SOCKET_ERROR)
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"FAILED Couldn't able to send Tag data to remote server");
                hr = E_FAIL;
                break;
            }
            else if (::recv(m_ClientSocket,(char*)&tVacpServerResponse, sizeof tVacpServerResponse,0) <= 0)
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE,1,"FAILED Couldn't able to get acknowledgement from remote server");
                hr = E_FAIL;
                break;
            }

            tVacpServerResponse.iResult  = ntohl(tVacpServerResponse.iResult);
            if(tVacpServerResponse.iResult > 0)
            {
                bResult = true;
            }

            tVacpServerResponse.val = ntohl(tVacpServerResponse.val);
            tVacpServerResponse.str[tVacpServerResponse.val] = '\0';
            
            LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"INFO: VacpServerResponse:[%d] %s\n", tVacpServerResponse.iResult, tVacpServerResponse.str);
        
        } while(0);

        if(bResult)
        {
            m_bTagSentStatus = true;
            hr = S_OK;
        }
        else
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"FAILED to Send Tag to the Remote Server for following Server devices\n");
            LogVssEvent(EVENTLOG_ERROR_TYPE,1,"Error code:%d",GetLastError());
            ShowMsg("\n");
            hr = E_FAIL;
        }
    }
    return hr;
}

HRESULT CVacpProvider::AddVssObjectProp(VSS_OBJECT_PROP *srcProp)
{
    HRESULT hr = S_OK;

    VSS_OBJECT_PROP vssObject;
    
    vssObject.Type = VSS_OBJECT_SNAPSHOT;
    vssObject.Obj.Snap.m_SnapshotId = srcProp->Obj.Snap.m_SnapshotId;
    vssObject.Obj.Snap.m_SnapshotSetId = srcProp->Obj.Snap.m_SnapshotSetId;
    vssObject.Obj.Snap.m_lSnapshotsCount = srcProp->Obj.Snap.m_lSnapshotsCount;
    
    vssObject.Obj.Snap.m_ProviderId = srcProp->Obj.Snap.m_ProviderId;
    vssObject.Obj.Snap.m_eStatus = srcProp->Obj.Snap.m_eStatus;
    
    vssObject.Obj.Snap.m_pwszSnapshotDeviceObject = (VSS_PWSZ)"";
    vssObject.Obj.Snap.m_pwszOriginalVolumeName = (VSS_PWSZ)"";
    vssObject.Obj.Snap.m_pwszOriginatingMachine = (VSS_PWSZ)"";
    vssObject.Obj.Snap.m_pwszServiceMachine = (VSS_PWSZ)"";
    
    vssObject.Obj.Snap.m_pwszExposedName = (VSS_PWSZ)"";
    vssObject.Obj.Snap.m_pwszExposedPath = (VSS_PWSZ)"";
    vssObject.Obj.Snap.m_lSnapshotAttributes = 0;
    vssObject.Obj.Snap.m_tsCreationTimestamp = 0;
    
    m_vVssObjectProps.push_back(vssObject);

    return hr;
}


HRESULT CVacpProvider::RunVacpRpc(unsigned char *pszSnapshotSetId, boolean(*remoteFunction)(unsigned char * pszString))
{
    HRESULT hr = S_FALSE;

    do {

        unsigned char * pszUuid = NULL;
        unsigned char * pszProtocolSequence = (unsigned char *)"ncacn_np";
        unsigned char * pszNetworkAddress = NULL;
        unsigned char * pszOptions = NULL;
        unsigned char * pszStringBinding = NULL;
        unsigned long ulCode;
        RPC_STATUS status;

        char szEndPoint[MAX_END_POINT_SIZE];
        inm_strcpy_s(szEndPoint, ARRAYSIZE(szEndPoint), "\\pipe\\");
        inm_strcat_s(szEndPoint, ARRAYSIZE(szEndPoint), (char *)pszSnapshotSetId);

        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "RunVacpRpc();pipe endpoint = %s", szEndPoint);

        status = RpcStringBindingCompose(pszUuid,
            pszProtocolSequence,
            pszNetworkAddress,
            (unsigned char *)szEndPoint,
            pszOptions,
            &pszStringBinding);

        if (status != RPC_S_OK)
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "RunVacpRpc(): SnapshotSetId %s RpcStringBindingCompose failed: %d\n", pszSnapshotSetId, GetLastError());
            break;
        }

        status = RpcBindingFromStringBinding(pszStringBinding, &vacp_IfHandle);

        if (status != RPC_S_OK)
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "RunVacpRpc(): SnapshotSetId %s RpcBindingFromStringBinding failed: %d\n", pszSnapshotSetId, GetLastError());
        }
        else
        {

            DWORD startTime, endTime, diffTime;
            startTime = GetTickCount();

            RpcTryExcept
            {
                boolean ret = remoteFunction((unsigned char *)szEndPoint);
                if (ret)
                    hr = S_OK;

            }
            RpcExcept(1)
            {
                ulCode = RpcExceptionCode();
                LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "RunVacpRpc(): SnapshotSetId %s RPC call to VACP failed: %d %d\n", pszSnapshotSetId, ulCode, hr);
            }
            RpcEndExcept

            endTime = GetTickCount();
            diffTime = endTime - startTime;

            LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "Completed RPC call in %d ms.\n", diffTime);


            status = RpcBindingFree(&vacp_IfHandle);
            if (status != RPC_S_OK)
            {
                LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "RunVacpRpc(): SnapshotSetId %s RpcBindingFree failed: %d\n", pszSnapshotSetId, GetLastError());
            }
        }

        status = RpcStringFree(&pszStringBinding);
        if (status != RPC_S_OK)
        {
            LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "RunVacpRpc(): SnapshotSetId %s RpcStringFree failed: %d\n", pszSnapshotSetId, GetLastError());
        }

    } while (false);

    return hr;
}


STDMETHODIMP CVacpProvider::Clone( IVssEnumObject **ppEnum)
{
    HRESULT hr = S_OK;
    
    CComObject<CVacpProvider> *pVacpProvider = NULL;
    
    hr = CComObject<CVacpProvider>::CreateInstance(&pVacpProvider);

    if (FAILED(hr))
    {
        LogVssEvent(EVENTLOG_ERROR_TYPE, 1, "Failed to clone snapshot properties. CoCreateInstance failed with 0x%08x", hr);
        return hr;
    }
    
    CComPtr<IVssEnumObject> ref(pVacpProvider);

    LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "Clone(): cloning %d snapshot objects", m_vVssObjectProps.size());
    
    auto vssObjIter = m_vVssObjectProps.begin();
    while (vssObjIter != m_vVssObjectProps.end())
    {
        
        pVacpProvider->AddVssObjectProp(&(*vssObjIter));
        vssObjIter++;
    }
    
    pVacpProvider->Reset();

    *ppEnum = ref.Detach();
    
    return hr;
}

STDMETHODIMP CVacpProvider::Next( ULONG celt,VSS_OBJECT_PROP* rgelt,ULONG *pceltFetched)
{
    HRESULT hr = S_OK;
    ULONG ulCount = 0;
    try
    {
        if(!(rgelt))
        {
            hr = E_POINTER;
            LogVssEvent(EVENTLOG_ERROR_TYPE,1," There is no valid pointer to store the returned list of Snapshot properties.");
        }
        else
        {
            while(m_iterVssObjProps != m_vVssObjectProps.end())
            {
                if((ulCount < celt))
                {
                    rgelt[ulCount].Type = VSS_OBJECT_SNAPSHOT;
                    rgelt[ulCount].Obj.Snap.m_SnapshotId = m_iterVssObjProps->Obj.Snap.m_SnapshotId;
                    rgelt[ulCount].Obj.Snap.m_SnapshotSetId = m_iterVssObjProps->Obj.Snap.m_SnapshotSetId;
                    rgelt[ulCount].Obj.Snap.m_lSnapshotsCount = m_iterVssObjProps->Obj.Snap.m_lSnapshotsCount;
                    rgelt[ulCount].Obj.Snap.m_ProviderId = m_iterVssObjProps->Obj.Snap.m_ProviderId;
                    rgelt[ulCount].Obj.Snap.m_pwszSnapshotDeviceObject = (VSS_PWSZ)"";
                    rgelt[ulCount].Obj.Snap.m_pwszOriginalVolumeName = (VSS_PWSZ)"";
                    rgelt[ulCount].Obj.Snap.m_pwszOriginatingMachine = (VSS_PWSZ)"";
                    rgelt[ulCount].Obj.Snap.m_pwszServiceMachine = (VSS_PWSZ)"";
                    rgelt[ulCount].Obj.Snap.m_pwszExposedName = (VSS_PWSZ)"";
                    rgelt[ulCount].Obj.Snap.m_pwszExposedPath = (VSS_PWSZ)"";
                    rgelt[ulCount].Obj.Snap.m_lSnapshotAttributes = 0;
                    rgelt[ulCount].Obj.Snap.m_tsCreationTimestamp = 0;
                    rgelt[ulCount].Obj.Snap.m_eStatus = m_iterVssObjProps->Obj.Snap.m_eStatus;
                    ulCount++;
                    m_iterVssObjProps++;
                }
                else
                {
                    break;
                }
            }
            
            *pceltFetched = ulCount;
            if(ulCount != celt)
            {
                hr = S_FALSE;
                LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"Could return only %d number of snapshots out of the requested %d snapshots.",ulCount,celt);
            }
        }
        if(S_OK == hr )
        {
            LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"Successfully returned %d snapshot properties.",celt);
        }
    }
    catch(...)
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "Caught Exception in InMageVssProvider::Next().");
    }
    
    return hr;
}

STDMETHODIMP CVacpProvider::Reset()
{
    HRESULT hr = S_OK;
    m_iterVssObjProps = m_vVssObjectProps.begin();//CHECK should we send E_FAIL when there are no snapshots?
    if( 0 == m_vVssObjectProps.size())
    {
        hr = E_FAIL;
        LogVssEvent(EVENTLOG_INFORMATION_TYPE, g_dwEventViewerLoggingFlag, "Could not reset the IVssEnumObject.");
    }
    else
    {
        LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"Successfully reset the IVssEnumObject to point to the first snapshot item in the iterators list.");
    }
    return hr;
}

STDMETHODIMP CVacpProvider::Skip( ULONG celt)
{
    HRESULT hr = S_OK;
    ULONG ulCount = 0;
    
    while ((ulCount < celt) && (m_iterVssObjProps != m_vVssObjectProps.end()))
    {
        m_iterVssObjProps++;
        ulCount++;
    }

    if(ulCount != celt)//an attempt is made to read beyond the available list of snapshots
    {
        hr = S_FALSE;
        LogVssEvent(EVENTLOG_INFORMATION_TYPE,g_dwEventViewerLoggingFlag,"Number of available snapshots %d are less than requested to skip %d.",ulCount, celt);
    }
    return hr;
}

HRESULT CVacpProvider::IsOSHigherThanW2K3()
{
  HRESULT hFlag = S_FALSE;
  OSVERSIONINFO osVerInfo;
  ZeroMemory(&osVerInfo,sizeof(osVerInfo));
  osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
  BOOL bVer = ::GetVersionEx(&osVerInfo);
  if((osVerInfo.dwMajorVersion >= WIN2K8_SERVER_VER_MAJOR)&&
      (osVerInfo.dwMinorVersion >= WIN2K8_SERVER_VER_MINOR))
  {
      hFlag = S_OK;
  }
  return hFlag;
}

STDMETHODIMP CVacpProvider::RegisterWithVss(USHORT flags)
{
    return ::RegisterWithVss(flags);
}
