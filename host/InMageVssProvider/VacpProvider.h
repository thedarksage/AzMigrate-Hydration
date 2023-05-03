

#pragma once
#include "resource.h"       
#include "InMageVssProvider_i.h"
#include "_IVacpProviderEvents_CP.h"

#define FILE_DEVICE_INMAGE   0x00008001
#define INMAGE_FILTER_DOS_DEVICE_NAME   _T("\\\\.\\InMageFilter")

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Win CE does not support Single-threaded COM objects. "
#endif

#define IOCTL_INMAGE_TAG_VOLUME                 CTL_CODE( FILE_DEVICE_INMAGE, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS )

typedef enum _etTagStatus
{
    // etTagStatusCommited is a successful state
    ecTagStatusCommited = 0,
    // etTagStatus is initialized with ecTagStatusPending.
    ecTagStatusPending = 1,
    // etTagStatusDeleted - Tag is deleted due to StopFiltering or ClearDiffs
    ecTagStatusDeleted = 2,
    // etTagStatusDropped - Tag is dropped due to write in Bitmap file.
    ecTagStatusDropped = 3,
    // ecTagStatusInvalidGUID
    ecTagStatusInvalidGUID = 4,
    // ecTagStatusFilteringStopped is returned if volume filtering is stopped
    ecTagStatusFilteringStopped = 5,
    // ecTagStatusUnattempted - fail to generate tag
    //  for example - 
    // volume filtering is stopped for some another volume in the volume list,
    //          so no tag is added to this volume, and ecTagStatusUnattempted is returned.
    // GUID is invalid for some another volume in the volume list,
    //          so no tag is added to this volume, and ecTagStatusUnattempted is returned.
    ecTagStatusUnattempted = 6,
    // ecTagStatusFailure - Some error occurs while adding tags. Such as
    // memory allocation failure, ..
    ecTagStatusFailure = 7,
    ecTagStatusMaxEnum = 8
} etTagStatus, *petTagStatus;

typedef struct _TAG_VOLUME_STATUS_OUTPUT_DATA
{
    etTagStatus Status;
    ULONG       ulNoOfVolumes;
    etTagStatus VolumeStatus[1];
} TAG_VOLUME_STATUS_OUTPUT_DATA, *PTAG_VOLUME_STATUS_OUTPUT_DATA;

#define SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(x) ((ULONG)FIELD_OFFSET(TAG_VOLUME_STATUS_OUTPUT_DATA, VolumeStatus[x]))

typedef struct _TAG_VOLUME_STATUS_INPUT_DATA
{
    GUID        TagGUID;
} TAG_VOLUME_STATUS_INPUT_DATA, *PTAG_VOLUME_STATUS_INPUT_DATA;

typedef struct _VACP_SERVER_RESPONSE_
{
		
	int iResult; // return value of server success or failure
    int val;	// Success or Failuew code
    char str[1024];  //string related to val
		
}VACP_SERVER_RESPONSE;



// CVacpProvider

class ATL_NO_VTABLE CVacpProvider :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CVacpProvider, &CLSID_VacpProvider>,
	public ISupportErrorInfo,
	public IConnectionPointContainerImpl<CVacpProvider>,
	public CProxy_IVacpProviderEvents<CVacpProvider>,
	public IDispatchImpl<IVacpProvider, &IID_IVacpProvider, &LIBID_InMageVssProviderLib, /*wMajor =*/ 1, /*wMinor =*/ 0>,
	public IVssSoftwareSnapshotProvider,
	public IVssProviderCreateSnapshotSet,
	public IVssProviderNotifications,
	public IVssEnumObject
{
public:
	CVacpProvider();
	~CVacpProvider();

DECLARE_REGISTRY_RESOURCEID(IDR_VACPPROVIDER)


BEGIN_COM_MAP(CVacpProvider)
	COM_INTERFACE_ENTRY(IVacpProvider)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
	COM_INTERFACE_ENTRY(IConnectionPointContainer)
	COM_INTERFACE_ENTRY(IVssSoftwareSnapshotProvider)
	COM_INTERFACE_ENTRY(IVssProviderCreateSnapshotSet)
	COM_INTERFACE_ENTRY(IVssProviderNotifications)
	COM_INTERFACE_ENTRY(IVssEnumObject)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(CVacpProvider)
	CONNECTION_POINT_ENTRY(__uuidof(_IVacpProviderEvents))
END_CONNECTION_POINT_MAP()
// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);


	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:

    //Interface Methods of IVacpProvider
    STDMETHOD(IsItRemoteTagIssue)(USHORT uRemoteSend);
    STDMETHOD(IsItSyncTagIssue)(USHORT uSyncTag);
    STDMETHOD(EstablishRemoteConInfoForInMageVssProvider)(USHORT uPort, unsigned char* serverIp);
    STDMETHOD(StoreRemoteTagsBuffer)(unsigned char* TagDataBuffer, ULONG ulTagBufferLen);
    STDMETHOD(StoreLocalTagsBuffer)(unsigned char* TagDataBuffer, ULONG ulTagBufferLen, ULONG ulOutBufLen);
    STDMETHOD(IsTagIssuedSuccessfullyDuringCommitTime)(USHORT* ulTagSent);
    STDMETHOD(SetVerifyMode)();
    STDMETHOD(RegisterSnapshotSetId)(VSS_ID snapshotSeId);
    STDMETHOD(RegisterWithVss)(USHORT flags);
    


	//IVssSoftwareSnapshotProvider Methods	
	STDMETHOD(SetContext)(LONG lContext);

	STDMETHOD(GetSnapshotProperties)(VSS_ID SnapshotId,
									 VSS_SNAPSHOT_PROP *pProp);

	STDMETHOD(Query) (VSS_ID QueriedObjectId,
					  VSS_OBJECT_TYPE eQueriedObjectType,
					  VSS_OBJECT_TYPE eReturnedObjectsType,
					  IVssEnumObject **ppEnum);
	
	STDMETHOD(DeleteSnapshots) ( VSS_ID SourceObjectId,
								 VSS_OBJECT_TYPE eSourceObjectType,
								 BOOL bForceDelete,
								 LONG *plDeletedSnapshots,
								 VSS_ID *pNondeletedSnapshotID);

	STDMETHOD(BeginPrepareSnapshot)( VSS_ID SnapshotSetId,
									 VSS_ID SnapshotId,
									 VSS_PWSZ pwszVolumeName,
									 LONG lNewContext);

	STDMETHOD(IsVolumeSupported)(VSS_PWSZ pwszVolumeName,
								 BOOL *pbSupportedByThisProvider);

	STDMETHOD(IsVolumeSnapshotted)( VSS_PWSZ pwszVolumeName,
									BOOL *pbSnapshotsPresent,
									LONG *plSnapshotCompatibility);

	STDMETHOD(SetSnapshotProperty)(VSS_ID SnapshotId,
								   VSS_SNAPSHOT_PROPERTY_ID eSnapshotPropertyId,
								   VARIANT vProperty);

	STDMETHOD(RevertToSnapshot)( VSS_ID SnapshotId);

	STDMETHOD(QueryRevertStatus)( VSS_PWSZ pwszVolume,IVssAsync **ppAsync);


	// IVssProviderCreateSnapshotSet Methods
	//STDMETHOD(BeginPrepareSnapshot)( VSS_ID SnapshotSetId, VSS_ID SnapshotId, VSS_PWSZ pwszVolumeName, LONG lNewContext);//CHECK
	STDMETHOD(EndPrepareSnapshots)(VSS_ID SnapshotSetId);
    STDMETHOD(PreCommitSnapshots)(VSS_ID SnapshotSetId);
    STDMETHOD(CommitSnapshots)(VSS_ID SnapshotSetId);
    STDMETHOD(PostCommitSnapshots)(VSS_ID SnapshotSetId, LONG lSnapshotsCount);
    STDMETHOD(PreFinalCommitSnapshots)(VSS_ID SnapshotSetId);
    STDMETHOD(PostFinalCommitSnapshots)(VSS_ID SnapshotSetId);
    STDMETHOD(AbortSnapshots)(VSS_ID SnapshotSetId);


	// IVssProviderNotifications Methods
	STDMETHOD(OnLoad)(IUnknown * pCallback);
	STDMETHOD(OnUnload)(BOOL bForceUnload);

	//IVssEnumObject Interface Methods
	STDMETHOD(Clone)(IVssEnumObject ** ppEnum);
	STDMETHOD(Next)(ULONG celt,VSS_OBJECT_PROP *rgelt,ULONG *pceltFetched);
	STDMETHOD(Reset)();
	STDMETHOD(Skip)(ULONG celt);
	
	
	
private:

	//private methods
	void DeleteAbortedSnapshots();	
	HRESULT SendTagsToDriver(unsigned char* TagBuffer, unsigned long ulTagBufLen,unsigned long ulOutBufLen);
	HRESULT SendTagsToRemoteServer(unsigned char* TagBuffer, unsigned long ulTagBufLen);
	HRESULT CommitSnapshotsWithRegistryProcessing();
	HRESULT ReadRegistryForTagData();
	HRESULT IsOSHigherThanW2K3();
	HRESULT AddVssObjectProp(VSS_OBJECT_PROP *);
	HRESULT RunVacpRpc(unsigned char *pszSnapshotSetId, boolean (*remoteFunction)(unsigned char * pszString));

	//private data members
	
	VSS_ID m_setId;
	VSS_ID m_snapshotId;
    VSS_SNAPSHOT_STATE m_state;
	LONG m_lContext;

	static bool m_bSyncTag;
	static bool m_bRemoteTagSend;
	static char* m_pSocketBuffer;
	static char* m_pIoctlBuffer;
	static char m_pServerIp[MAX_PATH * 2];
	static unsigned long m_ulIoctlBufLen;
	static unsigned long m_ulSocketBufLen;
	static unsigned long m_ulIoctlOutBufLen;
	static unsigned long m_ulPortNo;
	static unsigned long uSnapshotCount;
	static bool m_bVerifyMode;
	unsigned long m_ulQueriedLocalBufLen;
	unsigned long m_ulQueriedSockBufLen;
	static bool m_bTagSentStatus;
	bool m_bSkipChkDriverMode;
	static PTAG_VOLUME_STATUS_OUTPUT_DATA m_pTagOutputData;
	OVERLAPPED m_OverLapTagIO;// = { 0, 0, 0, 0, NULL /* No event. */ };
	WSADATA m_wsaData;
	SOCKET m_ClientSocket;
	HANDLE m_hControlDevice;
	HKEY hInMageRegKey;
	HKEY hInMageRemoteSubKey;
	HKEY hInMageLocalSubKey;

	std::vector<VSS_OBJECT_PROP> m_vVssObjectProps;	
	std::vector<VSS_OBJECT_PROP>::iterator m_iterVssObjProps;
	
	static unsigned short m_usCount;
};

OBJECT_ENTRY_AUTO(__uuidof(VacpProvider), CVacpProvider)
