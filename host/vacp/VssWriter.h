#ifndef _INMAGE_VSS_WRITER_H_
#define _INMAGE_VSS_WRITER_H_

#pragma once

#if (_WIN32_WINNT > 0x500)

// VSS includes
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>

// VDS includes
#include <vds.h>
#include "common.h"


class InMageVssWriter : public CVssWriter
{

public:

    InMageVssWriter() 
    {
        m_bInitialized = false;
        m_dwProtectedDriveMask = 0;
		CallBackHandler = (void (*)(void *))NULL;
		CallBackContext = NULL;
		bCoInitializeSucceeded = false;
		bIsCoInitializedByCaller = false;
		volMountPoint_info.dwCountVolMountPoint = 0;
		bSentTagsSuccessfully = false;
    }

	InMageVssWriter(DWORD dwDriveMask, bool bCoInitializedAlready = false)
	{
		m_bInitialized = false;
        m_dwProtectedDriveMask = dwDriveMask;
		CallBackHandler = (void (*)(void *))NULL;
		CallBackContext = NULL;
		bCoInitializeSucceeded = false;
		bIsCoInitializedByCaller = bCoInitializedAlready;
		volMountPoint_info.dwCountVolMountPoint = 0;
		bSentTagsSuccessfully = false;
	}

	InMageVssWriter(VOLMOUNTPOINT_INFO& pVolMountPoint_info,
					DWORD dwDriveMask, 
					std::vector<AppSummary_t> &pApplications,
					bool bCoInitializedAlready = false)
	{
		m_bInitialized = false;
        m_dwProtectedDriveMask = dwDriveMask;
		CallBackHandler = (void (*)(void *))NULL;
		CallBackContext = NULL;
		bCoInitializeSucceeded = false;
		bIsCoInitializedByCaller = bCoInitializedAlready;
		bSentTagsSuccessfully = false;

		volMountPoint_info.dwCountVolMountPoint = pVolMountPoint_info.dwCountVolMountPoint;
		for(unsigned int i = 0; i < pVolMountPoint_info.dwCountVolMountPoint; i++)
		{
			volMountPoint_info.vVolumeMountPoints.push_back(pVolMountPoint_info.vVolumeMountPoints[i]);
		}
		std::vector<AppSummary_t>::iterator iter = pApplications.begin();
		while(iter != pApplications.end())
		{
		  AppSummary_t objAppSummary;
		  objAppSummary.totalVolumes = iter->totalVolumes;
		  objAppSummary.m_appName = iter->m_appName;
          vector<ComponentSummary_t>::iterator iterCmp = iter->m_vComponents.begin();
		  while(iterCmp != iter->m_vComponents.end())
		  {
			ComponentSummary_t component;
			component.bIsSelectable = (*iterCmp).bIsSelectable;
			component.ComponentName = ((*iterCmp).ComponentName);
			component.ComponentCaption = ((*iterCmp).ComponentCaption);
			component.ComponentLogicalPath = ((*iterCmp).ComponentLogicalPath);
			objAppSummary.m_vComponents.push_back(component);
			iterCmp++;
		  }
		  m_AppsSummary.push_back(objAppSummary);
		  iter++;
		}
	}

    ~InMageVssWriter()
    {
        UnInitialize();
    }

    bool Initialize();

    void UnInitialize()
    {
        if (IsInitialized())
	    {
		  Unsubscribe();
		  m_bInitialized = false;
	    }
		if (bCoInitializeSucceeded)
		{
			CoUninitialize();
			bCoInitializeSucceeded = false;
		}
    }

    bool IsInitialized()
    {
        return m_bInitialized;
    }

	void RegisterCallBackHandler(void (*fn)(void *), void *ctx)
	{
		if (fn != NULL)
		{
			CallBackHandler = fn;
			if (ctx != NULL)
			{
				CallBackContext = ctx;
			}
		}
	}

	void SetCallBackContext(void *ctx)
	{
		if (ctx != NULL)
		{
			CallBackContext = ctx;
		}
	}

	void *GetCallBackContext()
	{
		return CallBackContext;
	}

	void SetProtectedDriveMask(DWORD dwDriveMask)
	{
		m_dwProtectedDriveMask |= dwDriveMask;
	}
	void ClearProtectedDriveMask(DWORD dwDriveMask)
	{
		m_dwProtectedDriveMask &= ~dwDriveMask;
	}

	void ResetProtectedDriveMask(DWORD dwDriveMask)
	{
		m_dwProtectedDriveMask = 0;
	}

	DWORD GetProtectedDriveMask()
	{
		return m_dwProtectedDriveMask;
	}

	bool GetApplicationConsistencyState()
	{
		return bSentTagsSuccessfully;
	}

	void SetApplicationConsistencyState(bool bTagsGeneratedStatus)
	{
		bSentTagsSuccessfully = bTagsGeneratedStatus;
	}

    virtual bool STDMETHODCALLTYPE OnIdentify(IN IVssCreateWriterMetadata *pMetadata);

    virtual bool STDMETHODCALLTYPE OnPrepareBackup(IN IVssWriterComponents *pComponent);

    virtual bool STDMETHODCALLTYPE OnPrepareSnapshot();

    virtual bool STDMETHODCALLTYPE OnFreeze();

    virtual bool STDMETHODCALLTYPE OnThaw();

    virtual bool STDMETHODCALLTYPE OnAbort();

    virtual bool STDMETHODCALLTYPE OnPostSnapshot(IN IVssWriterComponents *pComponent);

    virtual bool STDMETHODCALLTYPE OnBackupComplete(IN IVssWriterComponents *pComponent);

    virtual bool STDMETHODCALLTYPE OnBackupShutdown(IN VSS_ID SnapshotSetId);

    virtual bool STDMETHODCALLTYPE OnPreRestore(IN IVssWriterComponents *pComponent);

    virtual bool STDMETHODCALLTYPE OnPostRestore(IN IVssWriterComponents *pComponent);

private:

    bool m_bInitialized;
	bool bCoInitializeSucceeded;
	bool bIsCoInitializedByCaller;
    DWORD m_dwProtectedDriveMask;
    unsigned int m_uTotalComponentsAdded;
	VSS_ID m_WriterGUID;
    char m_szWriterName[255];
	void (*CallBackHandler)(void *);
	void *CallBackContext;
	VOLMOUNTPOINT_INFO volMountPoint_info;
	bool bSentTagsSuccessfully;
	void STDMETHODCALLTYPE PrintLocalTime();
	vector<AppSummary_t> m_AppsSummary;
};

#endif
#endif /* _INMAGE_VSS_WRITER_H_ */
