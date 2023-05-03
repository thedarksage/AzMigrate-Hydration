#ifndef _INMAGE_VSS_REQUESTOR_H_
#define _INMAGE_VSS_REQUESTOR_H_

#pragma once
extern const string gstrInMageVssProviderGuid;
#if (_WIN32_WINNT > 0x500)

// _ASSERTE declaration (used by ATL) and other utility macros
#include "stdafx.h"

#include <boost/shared_ptr.hpp>

#include "util.h"
#include "common.h"

// VSS includes
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>



// VDS includes
//#include <vds.h>

//Using MS Softwar Shadow copy provider UUID for VACP
#define MS_SOFTWARE_SHADOWCOPY_PROVIDER	"b5946137-7b9f-4925-af80-51abd60b20d5"
//extern const string gstrInMageVssProviderGuid;

typedef struct _ComponentInfo
{
	_ComponentInfo()
	{
		dwDriveMask = 0;
		totalVolumes = 0;
		notifyOnBackupComplete = false;
		isSelectable = false;
		isTopLevel = false;
		isSelected = false;
		volMountPoint_info.dwCountVolMountPoint = 0;
	}

	string GetFullPath()
	{
		std::string cname = componentName;
		std::string lpath = logicalPath;
		
		//Append Backslash
		if(lpath.length() != 0)
		{
			if (lpath[lpath.length() - 1] != '\\')
				lpath = lpath.append("\\");
		}

		std::string fpath = lpath + cname;

		//Prefix with backslash
		if(fpath[0] != '\\')
			fpath = std::string("\\") + fpath;

		return fpath;
	}
	
		
	
	bool IsAncestorOf(struct _ComponentInfo &descendent)
	{
		 // The child must have a longer full path
	    if (descendent.fullPath.length() <= fullPath.length())
		    return false;

		std::string childPrefixPath = descendent.fullPath.substr(0, fullPath.length());

		// Return TRUE if the current full path is a prefix of the child full path
		if (stricmp(fullPath.c_str(), childPrefixPath.c_str()) == 0)
			return true;

		return false;
	}

	HRESULT AddVolumeMoutPointInfo(VOLMOUNTPOINT& volMP)
	{
		HRESULT hr =E_FAIL;
		if(!IsVolMountAlreadyExist(volMP))
		{
			volMountPoint_info.vVolumeMountPoints.push_back(volMP);
			volMountPoint_info.dwCountVolMountPoint++;
			hr = S_OK;
		}
		return hr;
	}

	bool IsVolMountAlreadyExist(VOLMOUNTPOINT& volMP)
	{
		bool ret = false;
		std::string strVolName;
		for(DWORD  i = 0; i < volMountPoint_info.dwCountVolMountPoint; i++)
		{
			strVolName = volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint;
			if(!strnicmp(volMP.strVolumeMountPoint.c_str(), strVolName.c_str(),strVolName.size()))
			{
				ret = true;
			}
		}
		return ret;
	}
	
    std::string componentName;
	std::string logicalPath;
	std::string captionName;
	std::string fullPath;
	bool  notifyOnBackupComplete;
	bool isSelectable;
	bool isTopLevel;
	bool isSelected;
	std::vector<std::string> FileGroupFiles;
	std::vector<std::string> DataBaseFiles;
	std::vector<std::string> DataBaseLogFiles;
	VSS_COMPONENT_TYPE componentType;
	std::vector<std::string> affectedVolumes;
	DWORD  dwDriveMask;
	unsigned int totalVolumes;
	VOLMOUNTPOINT_INFO volMountPoint_info;

}ComponentInfo_t;

typedef struct _VssAppInfo
{
	_VssAppInfo()
	{
		dwDriveMask = 0;
		totalVolumes = 0;
		isExcluded = false;
		includeSelectedComponentsOnly = false;		
		volMountPoint_info.dwCountVolMountPoint = 0;
	}

	std::string AppName;
	std::string idInstance;
	std::string idWriter;
	std::string szWriterInstanceName;

	std::vector<ComponentInfo_t> ComponentsInfo;
	std::vector<std::string> affectedVolumes; //changed from Vector<const char*> affectedVolumes; to fix compilation issue for 64 bit OS
	DWORD  dwDriveMask;
	unsigned int totalVolumes;
	bool isExcluded;
	bool includeSelectedComponentsOnly;
	VOLMOUNTPOINT_INFO volMountPoint_info;
}VssAppInfo_t;

class InMageVssRequestor
{

public:

	InMageVssRequestor()
    {
        m_dwProtectedDriveMask = 0;
		pVssBackupComponents = NULL;
		bCoInitializeSucceeded = false;
		bIsCoInitializedByCaller = false;
		SnapshotSucceeded = false;
		snapShotInProgress = false;
		pAsync = NULL;		
		pVssBackupComponentsEx = NULL;
		dwNumberofWritersTobeEnabled = 1;
		ZeroMemory(&uuidProviderID,sizeof(VSS_ID));
	}

	InMageVssRequestor(DWORD dwDriveMask, bool bCoInitializedAlready = false) 
    {
        m_dwProtectedDriveMask = dwDriveMask;
		pVssBackupComponents = NULL;
		bCoInitializeSucceeded = false;
		bIsCoInitializedByCaller = bCoInitializedAlready;
		SnapshotSucceeded = false;
		snapShotInProgress = false;
		pAsync = NULL;		
		pVssBackupComponentsEx = NULL;
		dwNumberofWritersTobeEnabled = 1;
		ZeroMemory(&uuidProviderID,sizeof(VSS_ID));
	}

	InMageVssRequestor(ACSRequest_t ACSRequest, bool bCoInitializedAlready = false) 
    {
        m_dwProtectedDriveMask = ACSRequest.volumeBitMask;
		pVssBackupComponents = NULL;
		bCoInitializeSucceeded = false;
		bIsCoInitializedByCaller = bCoInitializedAlready;
		SnapshotSucceeded = false;
		snapShotInProgress = false;
		pAsync = NULL;
		m_ACSRequest = ACSRequest;
		pVssBackupComponentsEx = NULL;
		dwNumberofWritersTobeEnabled = 1;
		ZeroMemory(&uuidProviderID,sizeof(VSS_ID));
	}
	
    /*
    ~InMageVssRequestor()
    {
        if (pVssBackupComponents)
        {
            pVssBackupComponents.Release();
            pVssBackupComponents = NULL;
        }
        if (pVssBackupComponentsEx)
        {
            pVssBackupComponentsEx.Release();
            pVssBackupComponentsEx = NULL;
        }
        if (pAsync)
        {
            pAsync.Release();
            pAsync = NULL;
        }

		UnInitialize();
        m_ACSRequest.Reset();

		// m_ACSRequest.vVolumes.clear();
		// m_ACSRequest.vApplyTagToTheseVolumes.clear();
		// m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints.clear();
		// m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint = 0;

        VssWriters.clear();
    }
    */


    void Reset()
    {
        if (pVssBackupComponents)
        {
            pVssBackupComponents.Release();
            pVssBackupComponents = NULL;
        }
        if (pVssBackupComponentsEx)
        {
            pVssBackupComponentsEx.Release();
            pVssBackupComponentsEx = NULL;
        }
        if (pAsync)
        {
            pAsync.Release();
            pAsync = NULL;
        }
    }

    void SetProtectedDriveMask(DWORD dwDriveMask)
    {
        m_dwProtectedDriveMask = dwDriveMask;
    }

    DWORD GetProtectedDriveMask()
    {
        return m_dwProtectedDriveMask;
    }
	
	ACSRequest_t GetACSRequest()
	{
		return m_ACSRequest;
	}

	VSS_ID GetCurrentSnapshotSetId()
	{
		return pSnapshotid;
	}

    std::vector<VssAppInfo_t> & GetVssAppInfo()
    {
        return VssWriters;
    }

	//HRESULT PrepareVolumesForConsistency(bool bDoFullBackup = false);
	HRESULT PrepareVolumesForConsistency(CLIRequest_t CmdRequest,bool bDoFullBackup = false);
	HRESULT FreezeVolumesForConsistency();
	HRESULT CleanupSnapShots();
	HRESULT GatherVssAppsInfo(vector<VssAppInfo_t> &AppInfo,CLIRequest_t &cliRequest,bool bAllApp = true);
	HRESULT GetVssWritersList(vector<VssAppInfo_t> &AppInfo,CLIRequest_t &cliRequest,bool bAllApp = true);
	//HRESULT MapVssWriterNameToVolumes(LWPSTR VssAppName, DWORD *pDwVolume);
	//HRESULT DiscoverWritersTobeEnabled();
	HRESULT DiscoverWritersTobeEnabled(CLIRequest_t CmdRequest);
	//HRESULT ExposeShadowCopiesLocally(VSS_PWSZ wszExpose,VSS_PWSZ* pwszExposed);
	HRESULT ExposeShadowCopiesLocally(string strShadowCopyMountVolume);
	HRESULT ProcessDeleteSnapshotSets();
	HRESULT DeleteSnapshotSets(VSS_ID snapshotSetId);
	HRESULT Initialize();

private:

	inline bool IsInitialized()
	{
		return (bCoInitializeSucceeded || bIsCoInitializedByCaller) ;
	}

	void UnInitialize()
	{
		if (bCoInitializeSucceeded)
		{
			//CoUninitialize();
			bCoInitializeSucceeded = false;
		}
		m_ACSRequest.vVolumes.clear();
		m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints.clear();
		m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint = 0;
		m_ACSRequest.vApplyTagToTheseVolumes.clear();
	}

	bool GetProviderUUID();
	//HRESULT Initialize();
	HRESULT PrintWritersInfo(unsigned int);
	HRESULT CheckWriterStatus(LPCWSTR wszOperation);
	HRESULT WaitAndCheckForAsyncOperation(IVssAsync *pva);
	HRESULT AddVolumesToSnapshotSet();
	HRESULT SelectComponentsForBackup();
	HRESULT MarkComponentsForBackup();
	HRESULT NotifyWritersOnBackupComplete(bool succeeded);
	void DiscoverTopLevelComponents(vector<ComponentInfo_t> &ComponentsInfo);
    void GetExcludedVolumeList();

	HRESULT FillInVssAppInfo(IVssExamineWriterMetadata * pInmMetadata, VssAppInfo_t &App,bool bSkipEnumeration = false);	
	HRESULT FillInVssAppInfoEx(CComPtr<IVssExamineWriterMetadataEx> pInmMetadataEx, VssAppInfo_t &App,DWORD &dwAppTobeAdded,bool bSkipEnumeration = false);
	HRESULT PrintWritersInfoEx(unsigned int);
	HRESULT	ProcessWMFiledesc(IVssWMFiledesc *pFileDesc, ComponentInfo_t &Component, vector<const char *> &FilePathVector);

	void DiscoverExcludedWriters(VssAppInfo_t &VssAppInfo);
	bool IsWriterExcluded(const char *WriterName, VSS_ID idWriter, VSS_ID idInstance);

	HRESULT	ProcessWMFiledescEx(IVssWMFiledesc *pFileDesc, ComponentInfo_t &Component, std::vector<std::string> &FilePathVector);
	HRESULT AddVolumeMoutPointInfo(VOLMOUNTPOINT & volMP);
	HRESULT AddVolumeMoutPointInfo(VOLMOUNTPOINT_INFO& volumeMountPoint_info,VOLMOUNTPOINT& volMP);
	bool IsVolMountAlreadyExist(VOLMOUNTPOINT & volMP);
	bool IsVolMountAlreadyExist(VOLMOUNTPOINT_INFO& volMountPoint_info, VOLMOUNTPOINT& volMP );
	bool IsVolMountAlreadyExistInApp(VOLMOUNTPOINT& volMP,VssAppInfo_t &App);
	HRESULT AddVolumesToSnapshotSetEx();

private:
   	
	bool bCoInitializeSucceeded;
	bool SnapshotSucceeded;
	bool bIsCoInitializedByCaller;	
	bool snapShotInProgress;
    DWORD m_dwProtectedDriveMask;
	CComPtr<IVssBackupComponents> pVssBackupComponents;
    unsigned cWriters;
	CComPtr<IVssAsync> pAsync;
	VSS_ID pSnapshotid;
	std::vector<VssAppInfo_t> VssWriters;
	ACSRequest_t m_ACSRequest;
	CComPtr<IVssBackupComponentsEx> pVssBackupComponentsEx;
	DWORD dwNumberofWritersTobeEnabled;

	VSS_ID uuidProviderID;
	std::vector<VSS_ID>m_vSnapshotIds;
	
};

typedef boost::shared_ptr<InMageVssRequestor> InMageVssRequestorPtr;

typedef std::map<const HRESULT, const std::string> VSS_Error_Code_String_t;

typedef std::map< const VSS_WRITER_STATE, const std::string> VSS_Writer_State_String_t;

#endif


#endif /* _INMAGE_VSS_REQUESTOR_H_ */