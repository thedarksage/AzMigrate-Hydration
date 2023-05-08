#include "stdafx.h"
using namespace std;
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include "VssRequestor.h"
#include "..\\common\\svstatus.h"
#include "VssWriter.h"
#include "vacp.h"
#include "VacpConf.h"
#include "devicefilter.h"
#include "inmsafecapis.h"
#include <map>
#include "VacpErrorCodes.h"
#include "VacpErrors.h"
#include "TagTelemetryKeys.h"
#include <boost/algorithm/string/replace.hpp>
#include <atlconv.h> // Use string conversion macros used in ATL

#ifndef XDebugPrintf
#define XDebugPrintf
#endif


extern bool        gbTagSend;
extern bool        gbRetryable;
extern DWORD    gdwRetryOperation;
extern DWORD    gdwMaxRetryOperation;
extern bool        gbIsOSW2K3SP1andAbove;
extern DWORD    gdwMaxSnapShotInProgressRetryCount;
extern bool     gbPersist;
extern bool    gbUseInMageVssProvider;
extern bool gbIncludeAllApplications;
extern bool gbSkipUnProtected;
extern bool g_bDistributedVacpFailed;
extern bool g_bDistributedTagRevokeRequest;

#define STR_INMAGE_VSS_WRITER  "InMageVSSWriter"
extern VSS_ID InMageVssWriterGUID;
extern unsigned char *gpInMageVssWriterGUID;
class VssAppConsistencyProvider;
extern vector<VSS_ID> gvSnapshotSets;
extern VssAppConsistencyProvider *gptrVssAppConsistencyProvider;

extern bool gbDeleteSnapshotSets;
extern bool gbEnumSW;
extern HANDLE        ghControlDevice;
extern VSS_ID g_ProviderGuid;

extern FailMsgList g_VacpFailMsgsList;
extern bool gbUseDiskFilter;
extern boost::shared_ptr<Event> g_Quit;

extern void inm_printf(const char * format, ...);


// update these error strings from following command
//  grep VSS_WS_ vss.h | awk '{print "{" $1 ",\"" $1 "\"},"}'
//
VSS_Writer_State_String_t gszVSSWriterState =
{
    { VSS_WS_UNKNOWN, "VSS_WS_UNKNOWN" },
    { VSS_WS_STABLE, "VSS_WS_STABLE" },
    { VSS_WS_WAITING_FOR_FREEZE, "VSS_WS_WAITING_FOR_FREEZE" },
    { VSS_WS_WAITING_FOR_THAW, "VSS_WS_WAITING_FOR_THAW" },
    { VSS_WS_WAITING_FOR_POST_SNAPSHOT, "VSS_WS_WAITING_FOR_POST_SNAPSHOT" },
    { VSS_WS_WAITING_FOR_BACKUP_COMPLETE, "VSS_WS_WAITING_FOR_BACKUP_COMPLETE" },
    { VSS_WS_FAILED_AT_IDENTIFY, "VSS_WS_FAILED_AT_IDENTIFY" },
    { VSS_WS_FAILED_AT_PREPARE_BACKUP, "VSS_WS_FAILED_AT_PREPARE_BACKUP" },
    { VSS_WS_FAILED_AT_PREPARE_SNAPSHOT, "VSS_WS_FAILED_AT_PREPARE_SNAPSHOT" },
    { VSS_WS_FAILED_AT_FREEZE, "VSS_WS_FAILED_AT_FREEZE" },
    { VSS_WS_FAILED_AT_THAW, "VSS_WS_FAILED_AT_THAW" },
    { VSS_WS_FAILED_AT_POST_SNAPSHOT, "VSS_WS_FAILED_AT_POST_SNAPSHOT" },
    { VSS_WS_FAILED_AT_BACKUP_COMPLETE, "VSS_WS_FAILED_AT_BACKUP_COMPLETE" },
    { VSS_WS_FAILED_AT_PRE_RESTORE, "VSS_WS_FAILED_AT_PRE_RESTORE" },
    { VSS_WS_FAILED_AT_POST_RESTORE, "VSS_WS_FAILED_AT_POST_RESTORE" },
    { VSS_WS_FAILED_AT_BACKUPSHUTDOWN, "VSS_WS_FAILED_AT_BACKUPSHUTDOWN" },
    { VSS_WS_COUNT, "VSS_WS_COUNT" }

};

// update these error strings from following command
// grep VSS_E_ vsserror.h | grep define | awk '{print "{" $2 ",\"" $2 "\"},"}'
//
VSS_Error_Code_String_t  gVssErrorString =
{
    { S_OK, "NO ERROR" },
    { E_ACCESSDENIED, "ACCESS DENIED" },
    { VSS_E_BAD_STATE, "VSS_E_BAD_STATE" },
    { VSS_E_UNEXPECTED, "VSS_E_UNEXPECTED" },
    { VSS_E_PROVIDER_ALREADY_REGISTERED, "VSS_E_PROVIDER_ALREADY_REGISTERED" },
    { VSS_E_PROVIDER_NOT_REGISTERED, "VSS_E_PROVIDER_NOT_REGISTERED" },
    { VSS_E_PROVIDER_VETO, "VSS_E_PROVIDER_VETO" },
    { VSS_E_PROVIDER_IN_USE, "VSS_E_PROVIDER_IN_USE" },
    { VSS_E_OBJECT_NOT_FOUND, "VSS_E_OBJECT_NOT_FOUND" },
    { VSS_E_VOLUME_NOT_SUPPORTED, "VSS_E_VOLUME_NOT_SUPPORTED" },
    { VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER, "VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER" },
    { VSS_E_OBJECT_ALREADY_EXISTS, "VSS_E_OBJECT_ALREADY_EXISTS" },
    { VSS_E_UNEXPECTED_PROVIDER_ERROR, "VSS_E_UNEXPECTED_PROVIDER_ERROR" },
    { VSS_E_CORRUPT_XML_DOCUMENT, "VSS_E_CORRUPT_XML_DOCUMENT" },
    { VSS_E_INVALID_XML_DOCUMENT, "VSS_E_INVALID_XML_DOCUMENT" },
    { VSS_E_MAXIMUM_NUMBER_OF_VOLUMES_REACHED, "VSS_E_MAXIMUM_NUMBER_OF_VOLUMES_REACHED" },
    { VSS_E_FLUSH_WRITES_TIMEOUT, "VSS_E_FLUSH_WRITES_TIMEOUT" },
    { VSS_E_HOLD_WRITES_TIMEOUT, "VSS_E_HOLD_WRITES_TIMEOUT" },
    { VSS_E_UNEXPECTED_WRITER_ERROR, "VSS_E_UNEXPECTED_WRITER_ERROR" },
    { VSS_E_SNAPSHOT_SET_IN_PROGRESS, "VSS_E_SNAPSHOT_SET_IN_PROGRESS" },
    { VSS_E_MAXIMUM_NUMBER_OF_SNAPSHOTS_REACHED, "VSS_E_MAXIMUM_NUMBER_OF_SNAPSHOTS_REACHED" },
    { VSS_E_WRITER_INFRASTRUCTURE, "VSS_E_WRITER_INFRASTRUCTURE" },
    { VSS_E_WRITER_NOT_RESPONDING, "VSS_E_WRITER_NOT_RESPONDING" },
    { VSS_E_WRITER_ALREADY_SUBSCRIBED, "VSS_E_WRITER_ALREADY_SUBSCRIBED" },
    { VSS_E_UNSUPPORTED_CONTEXT, "VSS_E_UNSUPPORTED_CONTEXT" },
    { VSS_E_VOLUME_IN_USE, "VSS_E_VOLUME_IN_USE" },
    { VSS_E_MAXIMUM_DIFFAREA_ASSOCIATIONS_REACHED, "VSS_E_MAXIMUM_DIFFAREA_ASSOCIATIONS_REACHED" },
    { VSS_E_INSUFFICIENT_STORAGE, "VSS_E_INSUFFICIENT_STORAGE" },
    { VSS_E_NO_SNAPSHOTS_IMPORTED, "VSS_E_NO_SNAPSHOTS_IMPORTED" },
    { VSS_E_SOME_SNAPSHOTS_NOT_IMPORTED, "VSS_E_SOME_SNAPSHOTS_NOT_IMPORTED" },
    { VSS_E_MAXIMUM_NUMBER_OF_REMOTE_MACHINES_REACHED, "VSS_E_MAXIMUM_NUMBER_OF_REMOTE_MACHINES_REACHED" },
    { VSS_E_REMOTE_SERVER_UNAVAILABLE, "VSS_E_REMOTE_SERVER_UNAVAILABLE" },
    { VSS_E_REMOTE_SERVER_UNSUPPORTED, "VSS_E_REMOTE_SERVER_UNSUPPORTED" },
    { VSS_E_REVERT_IN_PROGRESS, "VSS_E_REVERT_IN_PROGRESS" },
    { VSS_E_REVERT_VOLUME_LOST, "VSS_E_REVERT_VOLUME_LOST" },
    { VSS_E_REBOOT_REQUIRED, "VSS_E_REBOOT_REQUIRED" },
    { VSS_E_TRANSACTION_FREEZE_TIMEOUT, "VSS_E_TRANSACTION_FREEZE_TIMEOUT" },
    { VSS_E_TRANSACTION_THAW_TIMEOUT, "VSS_E_TRANSACTION_THAW_TIMEOUT" },
    { VSS_E_VOLUME_NOT_LOCAL, "VSS_E_VOLUME_NOT_LOCAL" },
    { VSS_E_CLUSTER_TIMEOUT, "VSS_E_CLUSTER_TIMEOUT" },
    { VSS_E_WRITERERROR_INCONSISTENTSNAPSHOT, "VSS_E_WRITERERROR_INCONSISTENTSNAPSHOT" },
    { VSS_E_WRITERERROR_OUTOFRESOURCES, "VSS_E_WRITERERROR_OUTOFRESOURCES" },
    { VSS_E_WRITERERROR_TIMEOUT, "VSS_E_WRITERERROR_TIMEOUT" },
    { VSS_E_WRITERERROR_RETRYABLE, "VSS_E_WRITERERROR_RETRYABLE" },
    { VSS_E_WRITERERROR_NONRETRYABLE, "VSS_E_WRITERERROR_NONRETRYABLE" },
    { VSS_E_WRITERERROR_RECOVERY_FAILED, "VSS_E_WRITERERROR_RECOVERY_FAILED" },
    { VSS_E_BREAK_REVERT_ID_FAILED, "VSS_E_BREAK_REVERT_ID_FAILED" },
    { VSS_E_LEGACY_PROVIDER, "VSS_E_LEGACY_PROVIDER" },
    { VSS_E_MISSING_DISK, "VSS_E_MISSING_DISK" },
    { VSS_E_MISSING_HIDDEN_VOLUME, "VSS_E_MISSING_HIDDEN_VOLUME" },
    { VSS_E_MISSING_VOLUME, "VSS_E_MISSING_VOLUME" },
    { VSS_E_AUTORECOVERY_FAILED, "VSS_E_AUTORECOVERY_FAILED" },
    { VSS_E_DYNAMIC_DISK_ERROR, "VSS_E_DYNAMIC_DISK_ERROR" },
    { VSS_E_NONTRANSPORTABLE_BCD, "VSS_E_NONTRANSPORTABLE_BCD" },
    { VSS_E_CANNOT_REVERT_DISKID, "VSS_E_CANNOT_REVERT_DISKID" },
    { VSS_E_RESYNC_IN_PROGRESS, "VSS_E_RESYNC_IN_PROGRESS" },
    { VSS_E_CLUSTER_ERROR, "VSS_E_CLUSTER_ERROR" },
    { VSS_E_UNSELECTED_VOLUME, "VSS_E_UNSELECTED_VOLUME" },
    { VSS_E_SNAPSHOT_NOT_IN_SET, "VSS_E_SNAPSHOT_NOT_IN_SET" },
    { VSS_E_NESTED_VOLUME_LIMIT, "VSS_E_NESTED_VOLUME_LIMIT" },
    { VSS_E_NOT_SUPPORTED, "VSS_E_NOT_SUPPORTED" },
    { VSS_E_WRITERERROR_PARTIAL_FAILURE, "VSS_E_WRITERERROR_PARTIAL_FAILURE" },
    { VSS_E_ASRERROR_DISK_ASSIGNMENT_FAILED, "VSS_E_ASRERROR_DISK_ASSIGNMENT_FAILED" },
    { VSS_E_ASRERROR_DISK_RECREATION_FAILED, "VSS_E_ASRERROR_DISK_RECREATION_FAILED" },
    { VSS_E_ASRERROR_NO_ARCPATH, "VSS_E_ASRERROR_NO_ARCPATH" },
    { VSS_E_ASRERROR_MISSING_DYNDISK, "VSS_E_ASRERROR_MISSING_DYNDISK" },
    { VSS_E_ASRERROR_SHARED_CRIDISK, "VSS_E_ASRERROR_SHARED_CRIDISK" },
    { VSS_E_ASRERROR_DATADISK_RDISK0, "VSS_E_ASRERROR_DATADISK_RDISK0" },
    { VSS_E_ASRERROR_RDISK0_TOOSMALL, "VSS_E_ASRERROR_RDISK0_TOOSMALL" },
    { VSS_E_ASRERROR_CRITICAL_DISKS_TOO_SMALL, "VSS_E_ASRERROR_CRITICAL_DISKS_TOO_SMALL" },
    { VSS_E_WRITER_STATUS_NOT_AVAILABLE, "VSS_E_WRITER_STATUS_NOT_AVAILABLE" },
    { VSS_E_ASRERROR_DYNAMIC_VHD_NOT_SUPPORTED, "VSS_E_ASRERROR_DYNAMIC_VHD_NOT_SUPPORTED" },
    { VSS_E_CRITICAL_VOLUME_ON_INVALID_DISK, "VSS_E_CRITICAL_VOLUME_ON_INVALID_DISK" },
    { VSS_E_ASRERROR_RDISK_FOR_SYSTEM_DISK_NOT_FOUND, "VSS_E_ASRERROR_RDISK_FOR_SYSTEM_DISK_NOT_FOUND" },
    { VSS_E_ASRERROR_NO_PHYSICAL_DISK_AVAILABLE, "VSS_E_ASRERROR_NO_PHYSICAL_DISK_AVAILABLE" },
    { VSS_E_ASRERROR_FIXED_PHYSICAL_DISK_AVAILABLE_AFTER_DISK_EXCLUSION, "VSS_E_ASRERROR_FIXED_PHYSICAL_DISK_AVAILABLE_AFTER_DISK_EXCLUSION" },
    { VSS_E_ASRERROR_CRITICAL_DISK_CANNOT_BE_EXCLUDED, "VSS_E_ASRERROR_CRITICAL_DISK_CANNOT_BE_EXCLUDED" },
    { VSS_E_ASRERROR_SYSTEM_PARTITION_HIDDEN, "VSS_E_ASRERROR_SYSTEM_PARTITION_HIDDEN" },
    { VSS_E_FSS_TIMEOUT, "VSS_E_FSS_TIMEOUT" }
};

std::set<HRESULT> gVssVolumeSkippedReason = 
{
    VSS_E_NESTED_VOLUME_LIMIT,
    VSS_E_OBJECT_ALREADY_EXISTS,
    VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER
};

// Writer failed states ref: https://docs.microsoft.com/en-us/windows/desktop/api/vss/ne-vss-_vss_writer_state
static bool WriterStateFailed(VSS_WRITER_STATE state)
{
    switch (state)
    {
    case VSS_WS_FAILED_AT_IDENTIFY:
    case VSS_WS_FAILED_AT_PREPARE_BACKUP:
    case VSS_WS_FAILED_AT_PREPARE_SNAPSHOT:
    case VSS_WS_FAILED_AT_FREEZE:
    case VSS_WS_FAILED_AT_THAW:
    case VSS_WS_FAILED_AT_POST_SNAPSHOT:
    case VSS_WS_FAILED_AT_BACKUP_COMPLETE:
    case VSS_WS_FAILED_AT_PRE_RESTORE:
    case VSS_WS_FAILED_AT_POST_RESTORE:
    case VSS_WS_FAILED_AT_BACKUPSHUTDOWN:
        return true;
    default:
        return false;
    }
}

HRESULT InMageVssRequestor::GatherVssAppsInfo(vector<VssAppInfo_t> &AppInfo,CLIRequest_t &cliRequest,bool bAllApp)
{
    using namespace std;
    USES_CONVERSION;
    HRESULT hr = S_OK;
    
    XDebugPrintf("ENTERED: InMageVssRequestor::GatherVssAppsInfo\n");

    do
    {
        if (!IsInitialized())
        {
            CHECK_SUCCESS_RETURN_VAL(Initialize(), hr);
        }

        CHECK_SUCCESS_RETURN_VAL(CreateVssBackupComponents(&pVssBackupComponents), hr);
            
        pVssBackupComponents->QueryInterface(IID_IVssBackupComponentsEx, (void**)&pVssBackupComponentsEx);

        CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->InitializeForBackup(), hr);
            
        CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->SetBackupState(false, true, VSS_BT_COPY, false), hr);

        if (!gbPersist)
        {
            CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->SetContext(VSS_CTX_BACKUP | VSS_VOLSNAP_ATTR_NO_AUTORECOVERY), hr);
        }
        else
        {
            CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->SetContext(VSS_CTX_APP_ROLLBACK), hr);
        }

        CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->GatherWriterMetadata(&pAsync), hr);

        CHECK_FAILURE_RETURN_VAL(WaitAndCheckForAsyncOperation(pAsync), hr);

        pAsync.Release();
        pAsync = NULL;

        CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->GetWriterMetadataCount(&cWriters), hr);
        DWORD dwAppTobeAdded = true;
        CComPtr<IVssExamineWriterMetadataEx> pInmMetadata = NULL;

        for (unsigned int iWriter = 0; iWriter < cWriters; iWriter++)
        {
            VSS_ID idInstance = GUID_NULL;
            VssAppInfo_t VssAppInfo;

            CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->GetWriterMetadataEx(iWriter, &idInstance, &pInmMetadata), hr);

            do
            {
                 CHECK_SUCCESS_RETURN_VAL(FillInVssAppInfoEx(pInmMetadata, VssAppInfo,dwAppTobeAdded), hr);
                if(dwAppTobeAdded)
                {
                    DiscoverExcludedWriters(VssAppInfo);
                    AppInfo.push_back(VssAppInfo);
                }

            }while(false);//this do while loop is to print all the writers info even if one of the wirters is exiting because of errors
            pInmMetadata.Release();
            pInmMetadata = NULL;

        }//end of for loop for writers
        
        if (pInmMetadata)
        {
            pInmMetadata.Release();
            pInmMetadata = NULL;
        }
    } while(false);

    
    XDebugPrintf("EXITED: InMageVssRequestor::GatherVssAppsInfo\n");
    return hr;
}

HRESULT InMageVssRequestor::FillInVssAppInfo(IVssExamineWriterMetadata * pInmMetadata, 
                                             VssAppInfo_t &App,
                                             bool bSkipEnumeration)
{
    XDebugPrintf("ENTERED: InMageVssRequestor::FillInVssAppInfo\n");
    using namespace std;
    USES_CONVERSION;
    HRESULT hr = S_OK;
    
    do
    {
        VSS_ID idInstance = GUID_NULL;
        VSS_ID idWriter = GUID_NULL;
        CComBSTR bstrWriterName;
        VSS_USAGE_TYPE usage = VSS_UT_UNDEFINED;
        VSS_SOURCE_TYPE source= VSS_ST_UNDEFINED;
        
        // Get writer identity information
        CHECK_SUCCESS_RETURN_VAL(pInmMetadata->GetIdentity(
            &idInstance,
            &idWriter,
            &bstrWriterName,
            &usage,
            &source),
            hr);
    
        unsigned char *pszInstanceId;
        unsigned char *pszWriterId;

        //Convert GUIDs to rpc string
        UuidToString(&idInstance, &pszInstanceId);
        UuidToString(&idWriter, &pszWriterId);

        //Initialize the VsswriterName
        App.AppName =  W2CA(bstrWriterName);
        App.idInstance = (const char *)pszInstanceId;
        App.idWriter = (const char *)pszWriterId;
        
        //Free rpc strings
        RpcStringFree(&pszInstanceId);
        RpcStringFree(&pszWriterId);

        // Get file counts      
        unsigned cIncludeFiles = 0;
        unsigned cExcludeFiles = 0;
        unsigned cComponents = 0;

        CHECK_SUCCESS_RETURN_VAL(pInmMetadata->GetFileCounts(
            &cIncludeFiles,
            &cExcludeFiles, 
            &cComponents),
            hr);

        char *affectedVolume = new (nothrow) char[MAX_PATH + 1];
        if (affectedVolume == NULL)
        {
            DebugPrintf("%s: failed to allocate memory for affected volume.\n", __FUNCTION__);
            hr = VACP_MEMORY_ALLOC_FAILURE;
            CHECK_SUCCESS_RETURN_VAL(hr, hr);
        }

        // Enumerate components
        for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
        {
            // Get component
            CComPtr<IVssWMComponent> pComponent = NULL;
            CHECK_SUCCESS_RETURN_VAL(pInmMetadata->GetComponent(
                iComponent, 
                &pComponent),
                hr);

            // Get the component info
            PVSSCOMPONENTINFO pInfo = NULL;
            CHECK_SUCCESS_RETURN_VAL(pComponent->GetComponentInfo(&pInfo), hr);

            ComponentInfo_t Component;

            Component.componentName = W2CA(pInfo->bstrComponentName);
            Component.logicalPath = W2CA(pInfo->bstrLogicalPath);
            Component.captionName = W2CA(pInfo->bstrCaption);
            Component.componentType = pInfo->type;
            Component.notifyOnBackupComplete = pInfo->bNotifyOnBackupComplete;
            Component.isSelectable = pInfo->bSelectable;
            Component.fullPath = Component.GetFullPath();
            Component.dwDriveMask = 0;
            Component.totalVolumes = 0;

            bool validateMountPoint = false;
            

            memset((void*)affectedVolume,0x00,MAX_PATH + 1);

            if(bSkipEnumeration)
            {
                char bufSysVol[4096] = {0};
                DWORD nRet = GetWindowsDirectory(bufSysVol,4096);
                if(!nRet)
                {
                    inm_printf("\nError in getting the system volume for the system writer.");
                    hr = VACP_E_ERROR_GETTING_SYSVOL_FOR_SYSTEMWRITER;
                    return hr;
                }
                else
                {
                    CHECK_SUCCESS_RETURN_VAL(GetVolumeRootPathEx(bufSysVol, affectedVolume, MAX_PATH, validateMountPoint), hr);

                    DWORD volIndex = 0;
                    DWORD volBitMask = 0;
                    VOLMOUNTPOINT volMP;
                    char baseVolPath[MAX_PATH+1] = {0};
                    GetVolumePathName(bufSysVol,baseVolPath,MAX_PATH);
                    if(strstr(baseVolPath,"\\\\?\\Volume"))
                    {
                        DWORD dwVolPathSize = 0;
                        char szVolumePath[MAX_PATH+1] = {0};

                        GetVolumePathNamesForVolumeName(baseVolPath,szVolumePath,MAX_PATH,&dwVolPathSize);
                        memset(baseVolPath,0,MAX_PATH+1);
                        inm_strcpy_s(baseVolPath,ARRAYSIZE(baseVolPath),szVolumePath);                  
                    }
                
                    if(validateMountPoint)
                    {
                        volMP.strVolumeName = baseVolPath;
                        volMP.strVolumeMountPoint = affectedVolume;
                        Component.AddVolumeMoutPointInfo(volMP);
                    }            
                    else
                    { 
                        CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeNameEx(affectedVolume, &volIndex,validateMountPoint), hr);                  
                        if( 0 != volIndex)
                        {
                            volBitMask = (1<<volIndex);
                            if ((Component.dwDriveMask & volBitMask) == 0)
                            {
                                Component.dwDriveMask |= volBitMask;
                                Component.affectedVolumes.push_back(affectedVolume);
                                Component.totalVolumes++;
                            }
                         }
                    }//not a mount point
                }//end of got the System Volume form Windows API instead of VSS's System Writer
            }//end of bSkipEnumeration
            else
            {
                for(unsigned i = 0; i < pInfo->cFileCount; i++)
                {
                    IVssWMFiledesc *pFileDesc = NULL;
                    CHECK_SUCCESS_RETURN_VAL(pComponent->GetFile (i, &pFileDesc), hr);
                    CHECK_SUCCESS_RETURN_VAL(ProcessWMFiledescEx(pFileDesc, Component, Component.FileGroupFiles), hr);
                }

                CHECK_SUCCESS_RETURN_VAL(hr, hr);

                //Process Database Files
                for(unsigned i = 0; i < pInfo->cDatabases; i++)
                {
                    IVssWMFiledesc *pFileDesc = NULL;
                    CHECK_SUCCESS_RETURN_VAL(pComponent->GetDatabaseFile (i, &pFileDesc), hr);
                    CHECK_SUCCESS_RETURN_VAL(ProcessWMFiledescEx(pFileDesc, Component, Component.DataBaseFiles), hr);
                }

                CHECK_SUCCESS_RETURN_VAL(hr, hr);

                //Process Database LogFiles
                for(unsigned i = 0; i < pInfo->cLogFiles; i++)
                {
                    IVssWMFiledesc *pFileDesc = NULL;
                    CHECK_SUCCESS_RETURN_VAL(pComponent->GetDatabaseLogFile (i, &pFileDesc), hr);               
                    CHECK_SUCCESS_RETURN_VAL(ProcessWMFiledescEx(pFileDesc, Component, Component.DataBaseLogFiles), hr);                
                }
                CHECK_SUCCESS_RETURN_VAL(hr, hr);
            }

            //Process Regular Files

            pComponent->FreeComponentInfo(pInfo);

            pComponent.Release();
            pComponent = NULL;
            
            for(unsigned i=0; i<Component.affectedVolumes.size(); i++)
            {
                const char *volumeName;
                DWORD volIndex = 0;
                DWORD volBitMask = 0;

                volumeName = Component.affectedVolumes[i].c_str();

                bool validateMountPoint = false;
                CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeNameEx(volumeName, &volIndex, validateMountPoint), hr);
                volBitMask = (1 << volIndex);

                Component.dwDriveMask |= volBitMask;

                if ((App.dwDriveMask & volBitMask) == 0)
                {
                    App.dwDriveMask |= volBitMask;
                    App.affectedVolumes.push_back(volumeName);
                    App.totalVolumes++;
                }
                
            }
            
            for( unsigned j = 0; j < Component.volMountPoint_info.dwCountVolMountPoint; j++)
            {
                string volumeName;
                DWORD volIndex = 30;
                DWORD volBitMask = 0;

                volBitMask = (1 << volIndex);
                Component.dwDriveMask |= volBitMask;

                App.dwDriveMask |= volBitMask;
                
                if(!IsVolMountAlreadyExistInApp(Component.volMountPoint_info.vVolumeMountPoints[j],App))
                {
                    App.volMountPoint_info.dwCountVolMountPoint++;
                    App.volMountPoint_info.vVolumeMountPoints.push_back(Component.volMountPoint_info.vVolumeMountPoints[j]);
                    App.totalVolumes++;
                }
            }
            
            App.ComponentsInfo.push_back(Component);

            CHECK_SUCCESS_RETURN_VAL(hr, hr);
        }

        if (affectedVolume)
        {
            delete[] affectedVolume;
            affectedVolume = NULL;
        }

        CHECK_SUCCESS_RETURN_VAL(hr, hr);

        DiscoverTopLevelComponents(App.ComponentsInfo);

    } while(FALSE);

    XDebugPrintf("EXITED: InMageVssRequestor::FillInVssAppInfo\n");
    return hr;
}

HRESULT    InMageVssRequestor::ProcessWMFiledesc(IVssWMFiledesc *pFileDesc, ComponentInfo_t &Component, vector<const char *> &FilePathVector)
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;
    DWORD retval = 0;

    char *affectedVolume = new (nothrow) char[MAX_PATH + 1];

    if (affectedVolume == NULL)
    {
        DebugPrintf("%s: failed to allocate memory for affected volume.\n", __FUNCTION__);
        return VACP_MEMORY_ALLOC_FAILURE;
    }

    do
    {
        CComBSTR bstrPath;

        CHECK_SUCCESS_RETURN_VAL(pFileDesc->GetPath(&bstrPath), hr);

        //do not validate volume mount points
        bool validateMountPoint = false;

        CHECK_SUCCESS_RETURN_VAL(GetVolumeRootPath(W2CA(bstrPath), affectedVolume, MAX_PATH, validateMountPoint), hr);

        DWORD volIndex = 0;
        DWORD volBitMask = 0;

        CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeName(affectedVolume, &volIndex, validateMountPoint), hr);

        volBitMask = (1<<volIndex);
        if ((Component.dwDriveMask & volBitMask) == 0)
        {
            Component.dwDriveMask |= volBitMask;
            Component.affectedVolumes.push_back(affectedVolume);
            Component.totalVolumes++;
        }

    } while(FALSE);

    if (affectedVolume)
    {
        delete[] affectedVolume;
        affectedVolume = NULL;
    }

    return hr;
}

HRESULT InMageVssRequestor::GetVssWritersList(vector<VssAppInfo_t> &AppInfo,CLIRequest_t &cliRequest,bool bAllApp)
{
    using namespace std;
    USES_CONVERSION;
    HRESULT hr = S_OK;
    CComPtr<IVssBackupComponentsEx> pvbEx = NULL;
    CComPtr<IVssBackupComponents> pvb = NULL;
    CComPtr<IVssAsync> pva = NULL;
    unsigned int nWriters = 0;

    XDebugPrintf("ENTERED: InMageVssRequestor::GetVssWritersList\n");

    do
    {
        if (!IsInitialized())
        {
            CHECK_SUCCESS_RETURN_VAL(Initialize(), hr);
        }
        
        CHECK_SUCCESS_RETURN_VAL(CreateVssBackupComponents(&pvb), hr);
            
        pvb->QueryInterface(IID_IVssBackupComponentsEx,(void**)&pvbEx);

        CHECK_SUCCESS_RETURN_VAL(pvbEx->InitializeForBackup(), hr);
            
        CHECK_SUCCESS_RETURN_VAL(pvbEx->SetBackupState (false,true,VSS_BT_COPY,false), hr);

        if (!gbPersist)
        {
            CHECK_SUCCESS_RETURN_VAL(pvbEx->SetContext(VSS_CTX_BACKUP | VSS_VOLSNAP_ATTR_NO_AUTORECOVERY), hr);
        }
        else
        {
            CHECK_SUCCESS_RETURN_VAL(pvbEx->SetContext(VSS_CTX_APP_ROLLBACK), hr);    //new    
        }

        CHECK_SUCCESS_RETURN_VAL(pvbEx->GatherWriterMetadata(&pva), hr);

        CHECK_FAILURE_RETURN_VAL(WaitAndCheckForAsyncOperation(pva), hr);

        CHECK_SUCCESS_RETURN_VAL(pvbEx->GetWriterMetadataCount(&nWriters), hr);
        DWORD dwAppTobeAdded = true;
        CComPtr<IVssExamineWriterMetadataEx> pInmMetadata = NULL;

        for (unsigned int iWriter = 0; iWriter < nWriters; iWriter++)
        {
            VSS_ID idInstance = GUID_NULL;
            VssAppInfo_t VssAppInfo;
            CHECK_SUCCESS_RETURN_VAL(pvbEx->GetWriterMetadataEx(iWriter, &idInstance, &pInmMetadata), hr);

            VSS_ID idWriter   = GUID_NULL;
            CComBSTR bstrWriterName;
            CComBSTR bstrWriterInstanceName;
            VSS_USAGE_TYPE usage = VSS_UT_UNDEFINED;
            VSS_SOURCE_TYPE source = VSS_ST_UNDEFINED;
            CHECK_SUCCESS_RETURN_VAL(pInmMetadata->GetIdentityEx(  &idInstance,
                                                        &idWriter,
                                                        &bstrWriterName,
                                                        &bstrWriterInstanceName,
                                                        &usage,
                                                        &source),hr);
            unsigned char *pszInstanceId;
            unsigned char *pszWriterId;
                
            //Convert GUIDs to rpc string
            UuidToString(&idInstance, &pszInstanceId);
            UuidToString(&idWriter, &pszWriterId);
                
            //Initialize the VsswriterName
            VssAppInfo.AppName = W2CA(bstrWriterName);//here WriterName is treated as AppName in our code!
            VssAppInfo.idInstance = (const char *)pszInstanceId;
            VssAppInfo.idWriter = (const char *)pszWriterId;
            if (bstrWriterInstanceName)
                VssAppInfo.szWriterInstanceName = W2CA(bstrWriterInstanceName);

            AppInfo.push_back(VssAppInfo);
                
            //Free rpc strings
            RpcStringFree(&pszInstanceId);
            RpcStringFree(&pszWriterId);

            bstrWriterName.Empty();
            bstrWriterInstanceName.Empty();

            pInmMetadata.Release();
            pInmMetadata = NULL;
        }//end of for loop for writers

        if (pInmMetadata)
        {
            pInmMetadata.Release();
            pInmMetadata = NULL;
        }

    } while(false);


    XDebugPrintf("EXITED: InMageVssRequestor::GetVssWritersList\n");
    return hr;
}

HRESULT InMageVssRequestor::PrepareVolumesForConsistency(CLIRequest_t CmdRequest,bool bDoFullBackup)
{
    XDebugPrintf("ENTERED: InMageVssRequestor::PrepareVolumesForConsistency\n");
    using namespace std;
    USES_CONVERSION;
    HRESULT hr = S_OK;

    VSS_BACKUP_TYPE backupType;
    
    //Set default backup type
    backupType = VSS_BT_COPY; 

    if (bDoFullBackup == true)
    {
        backupType = VSS_BT_FULL;
    }
    
    do
    {
        if (!IsInitialized())
        {
            CHECK_SUCCESS_RETURN_VAL(Initialize(), hr);
        }

        ////Erase all the entry from VssWriters. it was initially filled by GetRequiredWritersTobeEnabled()
        ////It is required to be empty as we need to refill VssWriters with new set of writers
        ////if we do not erase, its impacts compes into picture when we have  there are more then once 
        ////vacp instance are running simultaneously. so in NotifyWritersOnBackupComplete() funciton also notify 
        ////which are part of other instance
        //VssWriters.erase(VssWriters.begin(),VssWriters.end());

        ////CHECK_FAILURE_RETURN_VAL(ValidateSnapshotVolumes(m_dwProtectedDriveMask), hr);

        //// Cleanup previously taken snapshots
        //CHECK_SUCCESS_RETURN_VAL(CleanupSnapShots(), hr);

        //CHECK_SUCCESS_RETURN_VAL(CreateVssBackupComponents(&pVssBackupComponents), hr);
        //    
        //pVssBackupComponents->QueryInterface(IID_IVssBackupComponentsEx,(void**)&pVssBackupComponentsEx);

        //CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->InitializeForBackup(), hr);
        //    
        //CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->SetBackupState (true,true,backupType,false), hr);


  //      // SetContext function always return error on Windows XP. So this code must be
  //      // enabled for Windows 2003 and above only.
        ////The following Shadow Copy Contexts will create Persistent Shadow Copies...
        ////VSS_CTX_CLIENT_ACCESSIBLE
        ////VSS_CTX_APP_ROLLBACK
        ////VSS_CTX_NAS_ROLLBACK
        ////Out of the above three, VSS_CTX_APP_ROLLBACK is considered best for the time being            
        //if(!gbPersist)
        //{
        //    CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->SetContext(VSS_CTX_BACKUP | VSS_VOLSNAP_ATTR_NO_AUTORECOVERY), hr);    
        //}
        //else
        //{
        //    CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->SetContext(VSS_CTX_APP_ROLLBACK), hr);    //new    
        //}

        ////Enable only those writers which are mentioned in -a option and InmageVSSWriter
        ////we need do not reuired to include any other writers. This Enablewriter is required 
        ////to support multiple instance of VACP. Each VACP has differnt GUID but same name
        ////If we do not use EnableWriterClasses, it generates Error in event log saying
        ////InmageVssWriter is not responding to GatherWriterStatus.

        //if (!gbUseInMageVssProvider)
        //{
        //    EnableVSSWritersID[0] = InMageVssWriterGUID;
        //}
        //    
        //// CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->EnableWriterClasses(EnableVSSWritersID, dwNumberofWritersTobeEnabled), hr);

        //CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->GatherWriterMetadata(&pAsync), hr);

        //CHECK_FAILURE_RETURN_VAL(WaitAndCheckForAsyncOperation(pAsync), hr);
     //       
        //CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->GetWriterMetadataCount(&cWriters), hr);
        //DWORD dwAppTobeAdded = true;
  //      CComPtr<IVssExamineWriterMetadataEx> pInmMetadataEx = NULL;

        //for (unsigned int iWriter = 0; iWriter < cWriters; iWriter++)
        //{
        //    VSS_ID idInstance = GUID_NULL;
        //    VssAppInfo_t VssAppInfo;

        //    CHECK_SUCCESS_RETURN_VAL(pVssBackupComponentsEx->GetWriterMetadataEx(iWriter, &idInstance, &pInmMetadataEx), hr);

        //    do
        //    {
  //              CHECK_SUCCESS_RETURN_VAL(FillInVssAppInfoEx(pInmMetadataEx, VssAppInfo,dwAppTobeAdded), hr);
        //        
  //              if(dwAppTobeAdded)
        //        {
        //            DiscoverExcludedWriters(VssAppInfo);
        //            VssWriters.push_back(VssAppInfo);
        //        }
        //    }while(false);//this do while block is introduced to fix the bug Bug#26968 to avoid breaking out totally from the loop

  //          pInmMetadataEx.Release();

        //}//end of loop for writers
        //
  //      pInmMetadataEx.Release();
  //      pAsync.Release();

  //      CHECK_SUCCESS_RETURN_VAL(hr, hr);

        PrintWritersInfoEx(cWriters);
        
        MarkComponentsForBackup();
        SelectComponentsForBackup();
        
        int retryCount = gdwMaxSnapShotInProgressRetryCount;
        
         while(!g_Quit->IsSignalled())
         {
            DebugPrintf("Starting snapshot set\n");
            hr = pVssBackupComponents->StartSnapshotSet(&pSnapshotid);
            if (hr == S_OK)
            {
                unsigned char *pszSnapshotId;
                UuidToString(&pSnapshotid, &pszSnapshotId);//Convert GUIDs to rpc string                
                DebugPrintf("\nSnapshotSetId  GUID %s\n", pszSnapshotId);
                RpcStringFree(&pszSnapshotId);
                break;
            }
        
            if(gbDeleteSnapshotSets == false)
            {
              if (hr == VSS_E_SNAPSHOT_SET_IN_PROGRESS)
              {
                  retryCount--;
                  if (retryCount < 0)
                  {
                      DebugPrintf("Another snapshot is already in progress.\n");
                      DebugPrintf("FAILED: *** Excceed %d retries, Please re-run the command again\n", gdwMaxSnapShotInProgressRetryCount);
                      DebugPrintf("FAILED: *** If the problem persists, Please restart Volume Shadow Copy (vss) service\n\n"); 
                      break;
                  }
                  inm_printf("Another snapshot is already in progress(retriable error), sleeping 10 sec...\n");
                  g_Quit->Wait(10);
              }
              else
              {
                  break;
              }
            }
        }//end of while

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            std::stringstream ss;
            ss << "IVssBackupComponents::StartSnapshotSet() failed.";
            DebugPrintf("FAILED: %s hr = 0x%08x \n.", ss.str().c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            break;
        }
    
        if(gbDeleteSnapshotSets == false )
            snapShotInProgress = true; //after startSnapshotset, backup can be aborted before DoSnapShotSet() returnes


        //Call the InMageVssProvider specific code here...
        if (gbUseInMageVssProvider)
        {
            if (gptrVssAppConsistencyProvider)
            {
                hr = gptrVssAppConsistencyProvider->HandleInMageVssProvider();
                if (hr != S_OK)
                {
                    inm_printf("FAILED: VssAppConsistencyProvider::HandleInMageVssProvider() failed. hr = 0x%08x \n.", hr);
                    std::stringstream ss;
                    ss << "HandleInMageVssProvider() failed.";
                    AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
                    AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                    break;
                }
            }
            else
            {
                hr = ERROR_FAILED_TO_INITIALIZE_VSS_PROVIDER;
                inm_printf("FAILED: gptrVssAppConsistencyProvider is null.\n", hr);
                std::stringstream ss;
                ss << "VssAppConsistencyProvider is null.";
                AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                break;
            }
        }


        CHECK_SUCCESS_RETURN_VAL(AddVolumesToSnapshotSetEx(), hr);

    } while(false);

    XDebugPrintf("EXITED: InMageVssRequestor::PrepareVolumesForConsistency\n");
    return hr;
}

HRESULT InMageVssRequestor::SelectComponentsForBackup()
{

    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::SelectComponentsForBackup\n");

    for(unsigned int iWriter = 0; iWriter < VssWriters.size(); iWriter++)
    {
        VssAppInfo_t &VssApp = VssWriters[iWriter];

        if (VssApp.isExcluded == true)
        {
            DebugPrintf("%s: excluded Writer: %s\n\n", FUNCTION_NAME, VssApp.AppName.c_str());
            continue;
        }

        for(unsigned int iComponent = 0; iComponent < VssApp.ComponentsInfo.size(); iComponent++)
        {
            ComponentInfo_t &Component = VssApp.ComponentsInfo[iComponent];
            
            VSS_ID idInstance = GUID_NULL;
            VSS_ID idWriter = GUID_NULL;
            
            if (Component.isSelectable == false && Component.isTopLevel == false)
            {
                if(!VssApp.szWriterInstanceName.empty())
                {
                    XDebugPrintf("Skpping Non-Selectable/Non-TopLevel Component :  AppName = %s  WriterInstanceName = %s ComponentName = %s ComponentCaption = %s \n", VssApp.AppName.c_str(),VssApp.szWriterInstanceName.c_str(), Component.componentName.c_str(), Component.captionName.c_str());                    
                }
                else
                {
                    XDebugPrintf("Skpping Non-Selectable/Non-TopLevel Component :  AppName = %s  ComponentName = %s ComponentCaption = %s \n", VssApp.AppName.c_str(), Component.componentName.c_str(), Component.captionName.c_str());                    
                }
                continue;
            }

            if (VssApp.includeSelectedComponentsOnly == true && Component.isSelected == false)
            {
                if(!VssApp.szWriterInstanceName.empty())
                {
                    XDebugPrintf("Skpping UnSelected Component :  AppName = %s WriterInstanceName = %s  ComponentName = %s ComponentCaption = %s \n", VssApp.AppName,VssApp.szWriterInstanceName.c_str(), Component.componentName.c_str(), Component.captionName.c_str());                    
                }
                else
                {
                    XDebugPrintf("Skpping UnSelected Component :  AppName = %s  ComponentName = %s ComponentCaption = %s \n", VssApp.AppName.c_str(), Component.componentName.c_str(), Component.captionName.c_str());
                }
                continue;
            }
            
            hr = UuidFromString((unsigned char*)VssApp.idInstance.c_str(), &idInstance);

            if (hr != RPC_S_OK)
            {
                XDebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                if(!VssApp.szWriterInstanceName.empty())
                {
                    XDebugPrintf("FAILED: UuidFromString() failed. VssApp.idInstance = %s WriterInstanceName = %s, hr = 0x%x\n\n", VssApp.idInstance.c_str(), VssApp.szWriterInstanceName.c_str(), hr);
                }
                else
                {
                    XDebugPrintf("FAILED: UuidFromString() failed. VssApp.idInstance = %s, hr = 0x%x\n\n", VssApp.idInstance.c_str(), hr);
                }
                break;
            }

            hr = UuidFromString((unsigned char *)VssApp.idWriter.c_str(), &idWriter);

            if (hr != RPC_S_OK)
            {
                XDebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                if(!VssApp.szWriterInstanceName.empty())
                {
                    XDebugPrintf("FAILED: UuidFromString() failed. VssApp.idWriter = %s WriterInstanceName = %s, hr = 0x%x\n\n", VssApp.idWriter.c_str(), VssApp.szWriterInstanceName.c_str(), hr);
                }
                else
                {
                    XDebugPrintf("FAILED: UuidFromString() failed. VssApp.idWriter = %s, hr = 0x%x\n\n", VssApp.idWriter.c_str(), hr);
                }
                break;
            }

            bool bAddThisComponent = true;
            //The following block is commented to enable components which have volumes like 
            //"\\?\Volume{fa7b5ace-ada7-11e2-a9f5-806e6f6e6963}\Boot" but don't have any
            //mount points. This will help some ASR writer components and SharePoint writer components to 
            //get added in the snapshot set. It applies to all the writers which satisfy the criteria of
            //having volumes but no mount points.

            /*if(0 == Component.affectedVolumes.size())
            {
              if(0 == Component.volMountPoint_info.vVolumeMountPoints.size())
              {
                  bAddThisComponent = false;
                  continue;
              }
            }*/
            if(gbSkipUnProtected)
            {
                if(IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
                {
                    //before adding the component for backup, check if the volume on which the component is protected or not.
                    //if it is not protected then don't add the component(as it's volume is not protected). Just give a warning
                    char szVolumePathName[4096] = {0};
                    //Check if each of these affected volumes is protected or not. If not then don't add the component to VSS Backup Components.
                    std::vector<std::string>::const_iterator iterAffectedVols = Component.affectedVolumes.begin();
                    
                    for(;iterAffectedVols != Component.affectedVolumes.end();iterAffectedVols++)
                    {
                        if (!IsValidVolumeEx(iterAffectedVols->c_str(), true, pVssBackupComponentsEx))
                        {
                            hr = S_FALSE;
                            inm_printf("\n %s is Invalid Volume.",iterAffectedVols->c_str());
                            bAddThisComponent = false;
                            break;
                        }
                        if(!GetVolumePathName(iterAffectedVols->c_str(),szVolumePathName,4096))
                        {
                            hr = S_FALSE;
                            inm_printf("\nCould not get the Volume Path Name for %s and hence not selecting this component for backup! \n",iterAffectedVols->c_str());
                            bAddThisComponent = false;
                            break;
                        }
                        //convert the VolumePathName to Volume GUID
                        unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
                        WCHAR *VolumeGUID1 = new (nothrow) WCHAR[GUID_SIZE_IN_CHARS];
                        if(VolumeGUID1 == NULL)
                        {
                            bAddThisComponent = false;
                            break;
                        }
                        
                        memset((void*)VolumeGUID1,0x00,GuidSize);
                        if(!MountPointToGUID((PTCHAR)szVolumePathName,VolumeGUID1,GuidSize))
                        {
                            bAddThisComponent = false;
                            delete VolumeGUID1;
                            VolumeGUID1 = NULL;
                            break;
                        }
                        //Now check if this Volume GUID is protected or not! or a vsnap or not even existing!    
                        etWriteOrderState DriverWriteOrderState;
                        DWORD dwLastError = 0;
                        if(!CheckDriverStat(ghControlDevice,VolumeGUID1,&DriverWriteOrderState,&dwLastError))
                        {
                            bAddThisComponent = false;
                            if(VolumeGUID1)
                            {
                                delete VolumeGUID1;
                                VolumeGUID1 = NULL;
                            }
                            inm_printf("\nWarning:Component %s is in Volume %s is not protected. @Time: %s \n", Component.componentName.c_str(), szVolumePathName, GetLocalSystemTime().c_str());
                            break;
                        }
                        
                        if(VolumeGUID1)
                        {
                            delete VolumeGUID1;
                            VolumeGUID1 = NULL;
                        }
                    }//end of for loop for affected volumes

                    if(bAddThisComponent)
                    {
                        vector<VOLMOUNTPOINT>::iterator iterVolMountPoints = Component.volMountPoint_info.vVolumeMountPoints.begin();
                        for(;iterVolMountPoints != Component.volMountPoint_info.vVolumeMountPoints.end();iterVolMountPoints++)
                        {
                          if(!IsValidVolumeEx((*iterVolMountPoints).strVolumeMountPoint.c_str(),
                                              true,
                                              pVssBackupComponentsEx))
                          {
                              hr = S_FALSE;
                              inm_printf("\n %s is Invalid Volume.",(*iterVolMountPoints).strVolumeMountPoint.c_str());
                              bAddThisComponent = false;
                              break;
                          }
                            //Check if this MountPoint is protected or not! or a vsnap or not even existing!
                            etWriteOrderState DriverWriteOrderState;
                            DWORD dwLastError = 0;
                            std::string strGuid = ((*iterVolMountPoints).strVolumeMountPoint).substr(11,GUID_SIZE_IN_CHARS);
                            //if(!CheckDriverStat(ghControlDevice,A2W((*iterVolMountPoints).strVolumeMountPoint.c_str()),&DriverWriteOrderState,&dwLastError))
                            if(!CheckDriverStat(ghControlDevice,A2W(strGuid.c_str()),&DriverWriteOrderState,&dwLastError))
                            {
                                bAddThisComponent = false;
                                inm_printf("\nWarning:This Mount Point %s is not protected or not-existing or it is a vsnap. @Time: %s \n",szVolumePathName,GetLocalSystemTime().c_str());
                                break;
                            }
                        }//end of for loop
                    }
                }//end of if(IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
            }
            
            if(bAddThisComponent)
            {
              if(!VssApp.szWriterInstanceName.empty())
              {
                  inm_printf("\nAdding Component :  AppName = %s\n\t WriterInstanceName = %s\n\t  ComponentName = %s\n\t  ComponentCaption = %s \n\n\n", VssApp.AppName.c_str(), VssApp.szWriterInstanceName.c_str(), Component.componentName.c_str(), Component.captionName.c_str());
              }
              else
              {
                  inm_printf("\nAdding Component :  AppName = %s\n\t  ComponentName = %s\n\t  ComponentCaption = %s \n\n\n", VssApp.AppName.c_str(), Component.componentName.c_str(), Component.captionName.c_str());
              }
              hr = pVssBackupComponents->AddComponent(
                                      idInstance,
                                      idWriter,
                                      Component.componentType,
                                      A2CW(Component.logicalPath.c_str()),
                                      A2CW(Component.componentName.c_str()));

              if ( hr != S_OK)
              {
                  if (hr == VSS_E_OBJECT_ALREADY_EXISTS)
                  {
                      XDebugPrintf("Component already exists.\n");
                      continue;
                  }

                  XDebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                  if(!VssApp.szWriterInstanceName.empty())
                  {
                      inm_printf("FAILED: AddComponent() failed. Writer = %s WriterInstanceName = %s, hr = 0x%x\n.", VssApp.AppName.c_str(), VssApp.szWriterInstanceName.c_str(), hr);
                  }
                  else
                  {
                      inm_printf("FAILED: AddComponent() failed. Writer = %s, hr = 0x%x\n.", VssApp.AppName.c_str(), hr);
                  }
                  break;
              }
              else
              {
                  //XDebugPrintf("Successfully Added Component to snapshot set\n");
              }
          }
        }
        if (hr != S_OK)
            break;
    }

    XDebugPrintf("EXITED: InMageVssRequestor::SelectComponentsForBackup\n");
    return hr;
}

HRESULT InMageVssRequestor::MarkComponentsForBackup()
{

    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::MarkComponentsForBackup\n");

    for(unsigned int iWriter = 0; iWriter < VssWriters.size(); iWriter++)
    {
        VssAppInfo_t &VssApp  = VssWriters[iWriter];


        //if (stricmp(VssApp.AppName, STR_INMAGE_VSS_WRITER) == 0)
        if (!gbUseInMageVssProvider && (stricmp(VssApp.idWriter.c_str(), (char*)gpInMageVssWriterGUID) == 0))
        {
            VssApp.includeSelectedComponentsOnly = false; //add all components
            VssApp.isExcluded = false;
            continue;
        }

        if (VssApp.isExcluded == true)
        {
            DebugPrintf("%s: excluded Writer %s\n", FUNCTION_NAME, VssApp.AppName.c_str());
            continue;
        }

        for(unsigned int iApp = 0; iApp < m_ACSRequest.Applications.size(); iApp++)
        {
            AppSummary_t &ASum = m_ACSRequest.Applications[iApp];
            
            const char *wName = MapApplication2VssWriter(ASum.m_appName.c_str());

            if (wName == NULL || (stricmp(wName, VssApp.AppName.c_str()) != 0))
                continue;

            
            VssApp.includeSelectedComponentsOnly = true;
        
            if (ASum.includeAllComponents || ASum.m_vComponents.size() == 0)
            {
                VssApp.includeSelectedComponentsOnly = false;
                break;
            }

            for(unsigned int iWriterComp = 0; iWriterComp < VssApp.ComponentsInfo.size(); iWriterComp++)
            {

                ComponentInfo_t &wComp = VssApp.ComponentsInfo[iWriterComp];

                wComp.isSelected = false;
                
                for (unsigned int iAppComp = 0; iAppComp < ASum.m_vComponents.size(); iAppComp++)
                {
                    ComponentSummary_t &CSum = ASum.m_vComponents[iAppComp];

                    if (stricmp(wComp.componentName.c_str(), CSum.ComponentName.c_str()) == 0)
                        wComp.isSelected = true;                        
                }
            }
            
        }
    }

    XDebugPrintf("EXITED: InMageVssRequestor::MarkComponentsForBackup\n");
    return hr;
}

void InMageVssRequestor::GetExcludedVolumeList()
{
    std::string     excludedVolumes;
    VacpConfigPtr pVacpConf = VacpConfig::getInstance();
    if (NULL != pVacpConf.get()) {
        if (pVacpConf->getParamValue(VACP_CONF_EXCLUDED_VOLUMES, excludedVolumes)) {
            DebugPrintf("\nExcluded volumes: %s.\n", excludedVolumes.c_str());
            vector<string> volumes;
            boost::split(volumes, excludedVolumes, boost::is_any_of(","));
            if (!volumes.empty()) {
                gExcludedVolumes.insert(volumes.begin(), volumes.end());
            }
        }
    }
}


HRESULT InMageVssRequestor::AddVolumesToSnapshotSet()
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::AddVolumesToSnapshotSet\n");

    GetExcludedVolumeList();

    do
    {
        //Get UUID of MS Software Shadow copy provider or Custom provider
        if(!GetProviderUUID())
        {
            hr = E_FAIL;
            break;
        };
        
        char szVolumeName[4];

        szVolumeName[0] = 'X';
        szVolumeName[1] = ':';
        szVolumeName[2] = '\\';
        szVolumeName[3] = '\0';
        
        for( int i = 0; i < MAX_DRIVES; i++ )
        {
            VSS_ID SnapshotID;
            DWORD dwDriveMask = 1<<i;

            if(0 == ( m_dwProtectedDriveMask &  dwDriveMask ) ) 
            {
                continue;
            }

            szVolumeName[0] = 'A' + i;

            if (gExcludedVolumes.find(std::string(szVolumeName).substr(0,2)) != gExcludedVolumes.end()) {
                DebugPrintf("%s: Excluded volume %s from the SnapshotSet \n", FUNCTION_NAME, szVolumeName);
                continue;
            }

            DebugPrintf("Added volume %s to the SnapshotSet \n", szVolumeName);
            
            hr = pVssBackupComponents->AddToSnapshotSet(A2W(szVolumeName), uuidProviderID, &SnapshotID);
            
            if (hr != S_OK)
            {
                if (VSS_E_OBJECT_ALREADY_EXISTS == hr)
                {
                    DebugPrintf("FILE: %s, LINE %d. hr = 0x%08x. Volume %s is already added to the snapshot set.\n", __FILE__, __LINE__, hr, szVolumeName);
                    hr = S_OK;
                    continue;
                }
                DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf("FAILED: IVssBackupComponents::AddToSnapshotSet() failed for volume %s, hr = 0x%08x \n.", szVolumeName, hr);
                break;
            }
            //If a Snapshot is added to the SnapshotSet, then add the SnapshotId to the vector of SnapshotIds.
            if(S_OK == hr)
            {
                m_vSnapshotIds.push_back(SnapshotID);
            }
        }
    }while(0);

    XDebugPrintf("EXITED: InMageVssRequestor::AddVolumesToSnapshotSet\n");
    return hr;
}


HRESULT InMageVssRequestor::NotifyWritersOnBackupComplete(bool succeeded = false)
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::NotifyWritersOnBackupComplete\n");

    if (succeeded == true)
    {
        DebugPrintf("Marking all applications as succesfully backed up...\n");
    }
    else
    {
        DebugPrintf("Backup failed. Marking all applications as not succesfully backed up... \n");
    }

    for(unsigned int iWriter = 0; iWriter < VssWriters.size(); iWriter++)
    {
        VssAppInfo_t VssApp;

        VssApp = VssWriters[iWriter];

        if (VssApp.isExcluded == true)
                continue;

        for(unsigned int iComponent = 0; iComponent < VssApp.ComponentsInfo.size(); iComponent++)
        {
            ComponentInfo_t Component = VssApp.ComponentsInfo[iComponent];

            //if (Component.notifyOnBackupComplete)
            {

                //Check if the component was explicitely added during backup
                if (Component.isSelectable == false && Component.isTopLevel == false)
                        continue;

                // Do NOT notify if the component was explicitly selected for snapshot
                if (VssApp.includeSelectedComponentsOnly == true && Component.isSelected == false)
                        continue;

                VSS_ID idInstance = GUID_NULL;
                VSS_ID idWriter = GUID_NULL;
                
                hr = UuidFromString((unsigned char*)VssApp.idInstance.c_str(), &idInstance);

                if (hr != RPC_S_OK)
                {
                    XDebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                    XDebugPrintf("FAILED: UuidFromString() failed. VssApp.idInstance = %s, hr = 0x%x\n.",VssApp.idInstance, hr);    
                    XDebugPrintf("Skipping NotifyBackupComplete() for writer = %s, component = %s\n\n", VssApp.AppName, Component.componentName);
                    continue;
                }

                hr = UuidFromString((unsigned char *)VssApp.idWriter.c_str(), &idWriter);

                if (hr != RPC_S_OK)
                {
                    XDebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                    XDebugPrintf("FAILED: UuidFromString() failed. VssApp.idWriter = %s, hr = 0x%x\n.", VssApp.idWriter.c_str(), hr);
                    XDebugPrintf("Skipping NotifyOnBackupComplete() for writer = %s, component = %s\n\n", VssApp.AppName.c_str(), Component.componentName.c_str());
                    continue;
                }

                hr = pVssBackupComponents->SetBackupSucceeded(
                                        idInstance,
                                        idWriter,
                                        Component.componentType,
                                        A2CW(Component.logicalPath.c_str()),
                                        A2CW(Component.componentName.c_str()),
                                        succeeded);

                if (hr != S_OK)
                {
                    if(gbSkipUnProtected)
                    {
                        if(!VssApp.szWriterInstanceName.empty())
                        {
                            DebugPrintf("NotifyOnBackupComplete:Skipped this component as it is un-protected: Component = %s Writer = %s WriterInstanceName = %s, hr = 0x%x @Time: %s \n.", Component.componentName.c_str(), VssApp.AppName.c_str(), VssApp.szWriterInstanceName.c_str(), hr, GetLocalSystemTime().c_str());
                        }
                        else
                        {
                            DebugPrintf("NotifyOnBackupComplete:Skipped this component as it is un-protected: Component = %s Writer = %s, hr = 0x%x @Time: %s \n.", Component.componentName.c_str(), VssApp.AppName.c_str(), hr, GetLocalSystemTime().c_str());
                        }
                    }
                    else
                    {
                    XDebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                    if(!VssApp.szWriterInstanceName.empty())
                    {
                        DebugPrintf("FAILED: SetBackupSucceeded() for Component = %s Writer = %s WriterInstanceName = %s, hr = 0x%x\n.", Component.componentName.c_str(), VssApp.AppName.c_str(), VssApp.szWriterInstanceName.c_str(), hr);
                    }
                    else
                    {
                        DebugPrintf("FAILED: SetBackupSucceeded() for Component = %s Writer = %s, hr = 0x%x\n.", Component.componentName.c_str(), VssApp.AppName.c_str(), hr);
                        }
                    }
                }
                else if(succeeded)
                {
                    if(!VssApp.szWriterInstanceName.empty())
                    {
                        DebugPrintf("SUCCESS: SetBackupSucceeded() for Component = %s Writer = %s WriterInstanceName = %s, hr = 0x%x\n", Component.componentName.c_str(), VssApp.AppName.c_str(), VssApp.szWriterInstanceName.c_str(), hr);
                    }
                    else
                    {
                        DebugPrintf("SUCCESS: SetBackupSucceeded() for Component = %s Writer = %s, hr = 0x%x\n", Component.componentName.c_str(), VssApp.AppName.c_str(), hr);
                    }
                }
            }

        }
    }

    XDebugPrintf("EXITED: InMageVssRequestor::NotifyWritersOnBackupComplete\n");
    return hr;
}

HRESULT InMageVssRequestor::Initialize()
{
    HRESULT hr=S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::Initialize\n");

    do
    {
        if(IsInitialized() == true)
            break;
    
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (hr != S_OK)
        {
            if (hr == S_FALSE)
            {
                hr = S_OK;
                bCoInitializeSucceeded = true;
                break;                
            }
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        hr = CoInitializeSecurity
            (
            NULL,
            -1,
            NULL,
            NULL,
            RPC_C_AUTHN_LEVEL_CONNECT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE,
            NULL
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

        bCoInitializeSucceeded = true;                
    
    } while(FALSE);

    XDebugPrintf("EXITED: InMageVssRequestor::Initialize\n");
    return hr;
}

HRESULT InMageVssRequestor::FreezeVolumesForConsistency()
{
    HRESULT hr = S_OK;
    HRESULT hrResult = S_OK;
    std::string errmsg;
    
    XDebugPrintf("ENTERED: InMageVssRequestor::FreezeVolumesForConsistency\n");

    do
    {
        hr = pVssBackupComponents->PrepareForBackup(&pAsync);
        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            errmsg = "Failed: IVssBackupComponents::PrepareForBackup().";
            DebugPrintf("FAILED: %s hr = 0x%x\n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            hrResult = hr;
            break;
        }

        hr = WaitAndCheckForAsyncOperation(pAsync);
        if(hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            DebugPrintf("FAILED: IVssBackupComponents::PrepareForBackup() failed. hr = 0x%08x \n.", hr);
            hrResult = hr;
            break;
        }

        pAsync.Release();
        pAsync = NULL;

        hr = CheckWriterStatus(L"After PrepareForBackup");
        if(FAILED(hr))
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            errmsg = "FAILED: CheckWriterStatus().";
            DebugPrintf("FAILED: %s hr = 0x%x\n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            hrResult = hr;
            break;
        }

        //snapShotInProgress is used in CleanupSnapshots.
        //VACP should not call AbortBackup after DoSnapshotSet call if any error occurs
        snapShotInProgress = false;

        DebugPrintf("Commiting shadow copy for the set...\n");
        hr = pVssBackupComponents->DoSnapshotSet(&pAsync);
        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            errmsg = "FAILED: IVssBackupComponents::DoSnapshotSet().";
            DebugPrintf("FAILED: %s hr = 0x%x\n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            hrResult = hr;
            break;
        }


        hr = WaitAndCheckForAsyncOperation(pAsync);
        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            DebugPrintf("FAILED: LoopWait() failed. hr = 0x%08x \n.", hr);
            hrResult = hr;
            break;
        }

        pAsync.Release();
        pAsync = NULL;

        hr = CheckWriterStatus(L"After DoSnapshotSet");
        if (FAILED(hr))
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            errmsg = "FAILED: CheckWriterStatus().";
            DebugPrintf("FAILED: %s hr = 0x%x\n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            hrResult = hr;

            // continue to notify the writers about backup complete
            // in case of failure also
        }

        (void)NotifyWritersOnBackupComplete(SUCCEEDED(hrResult));

        hr = pVssBackupComponents->BackupComplete(&pAsync);
        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            errmsg = "FAILED: IVssBackupComponents::BackupComplete().";
            DebugPrintf("FAILED: %s hr = 0x%x\n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            if (SUCCEEDED(hrResult))
                hrResult = hr;
            break;
        }

        hr = WaitAndCheckForAsyncOperation(pAsync);
        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            DebugPrintf("FAILED: LoopWait() failed. hr = 0x%08x \n.", hr);
            if (SUCCEEDED(hrResult))
                hrResult = hr;
            break;
        }

        pAsync.Release();
        pAsync = NULL;

        hr = CheckWriterStatus(L"After BackupComplete");
        if (FAILED(hr))
        {
            DebugPrintf("FILE: %s, LINE %d\n", __FILE__, __LINE__);
            errmsg = "FAILED: CheckWriterStatus().";
            DebugPrintf("FAILED: %s hr = 0x%x\n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;

            if (SUCCEEDED(hrResult))
                hrResult = hr;
            break;
        }

        if(g_bDistributedTagRevokeRequest && g_bDistributedVacpFailed)
        {
          hr = E_FAIL;
          errmsg = "Distributed Tag will be REVOKED.";
          DebugPrintf("FAILED: %s\n", errmsg.c_str());
          AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
          AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
          if (SUCCEEDED(hrResult))
              hrResult = hr;
        }
        else if(gbTagSend)
        {
            if (SUCCEEDED(hrResult))
            {
                hr = S_OK;
                DebugPrintf("Marked all applications as successfully backed up\n");
            }
        }
        else
        {
          hr = E_FAIL;
          errmsg = "Failed to send the tag to the driver.";
          DebugPrintf("FAILED: %s\n", errmsg.c_str());
          AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
          AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
          if (SUCCEEDED(hrResult))
              hrResult = hr;
        }

    } while (false);

    if (SUCCEEDED(hrResult))
    {
        SnapshotSucceeded = true;
    }
    XDebugPrintf("EXITED: InMageVssRequestor::FreezeVolumesForConsistency\n");
    return hrResult;
}

HRESULT InMageVssRequestor::CleanupSnapShots()
{
  HRESULT hr = S_OK;
  LONG TotalSnapshotsDeleted;
  VSS_ID SnapshotsNotDeleted[MAX_DRIVES] = {0};
  ULONG refCnt = 0;
  std::string errmsg;

  XDebugPrintf("ENTERED: InMageVssRequestor::CleanupSnapShots\n");

    do
    {
        if (SnapshotSucceeded == false)
        {

            hr = S_OK;

            if (pVssBackupComponents)
            {
                //AbortBackup() method must be called if a backup operation terminates after the 
                //creation of a shadow copy set with IVssBackupComponents::StartSnapshotSet and 
                //before IVssBackupComponents::DoSnapshotSet returns.
                //If AbortBackup is called and no shadow copy or backup operations are underway, it is ignored.
                if (snapShotInProgress)
                {
                    HRESULT abtHr = pVssBackupComponents->AbortBackup();
                    if (abtHr != S_OK)
                    {
                        errmsg = "FAILED: IVssBackupComponents::AbortBackup().";
                        DebugPrintf("%s failed. hr = 0x%08x \n.", errmsg.c_str(), abtHr);
                    }
                }
                else
                {
                    pAsync.Release();
                    pAsync = NULL;

                    (void)NotifyWritersOnBackupComplete(false);
                    hr = pVssBackupComponents->BackupComplete(&pAsync);
                    if (hr != S_OK)
                    {
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        errmsg = "FAILED: IVssBackupComponents::BackupComplete().";
                        DebugPrintf("%s failed. hr = 0x%08x \n.", errmsg.c_str(), hr);
                        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
                        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                        break;
                    }

                    hr = WaitAndCheckForAsyncOperation(pAsync);
                    if(hr != S_OK)
                    {
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        DebugPrintf("FAILED: LoopWait() failed. hr = 0x%08x \n.", hr);
                        break;
                    }

                    pAsync.Release();
                    pAsync = NULL;

                    hr = CheckWriterStatus(L"After BackupComplete [NOTIFIED FAILED BACKUP]");
                    if(FAILED(hr))
                    {
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        errmsg = "FAILED: CheckWriterStatus().";
                        DebugPrintf("%s failed. hr = 0x%08x \n.", errmsg.c_str(), hr);
                        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
                        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                        break;
                    }
                                    
                    //(void)pVssBackupComponents->BackupComplete(&pAsync); // required really??
                }
            }
            //DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            //DebugPrintf("Snapshots does NOT exist.\n. hr = 0x%x", hr);
            break;
        }

        if(pVssBackupComponents)
        {
            inm_printf("Deleting snapshot set(Shadow copies)\n");
            unsigned char *pszSnapshotId;
            UuidToString(&pSnapshotid, &pszSnapshotId);//Convert GUIDs to rpc string                
            XDebugPrintf("\nSnapshotSetId  GUID %s.\n", pszSnapshotId);
            RpcStringFree(&pszSnapshotId);
            hr = pVssBackupComponents->DeleteSnapshots(pSnapshotid, VSS_OBJECT_SNAPSHOT_SET, true, &TotalSnapshotsDeleted, SnapshotsNotDeleted);
            if(hr != S_OK)
            {
                DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                std::stringstream ss;
                ss << "FAILED: IVssBackupComponents::DeleteSnapshots(). m_dwProtectedDriveMask = " << std::hex << m_dwProtectedDriveMask << ", TotalSnapshotsDeleted = " << SnapshotsNotDeleted;
                DebugPrintf("%s . hr = 0x%08x \n.", ss.str().c_str(), hr);
                AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                break;
            }
            inm_printf("Successfully deleted snapshot set(Shadow copies)\n");
        }

    } while (false);

    SnapshotSucceeded = false;
    snapShotInProgress = false;

    if (pAsync)
    {
        pAsync.Release();
        pAsync = NULL;
    }

    if (pVssBackupComponentsEx)
    {
        pVssBackupComponentsEx.Release();
        pVssBackupComponentsEx = NULL;
    }

    if (pVssBackupComponents)
    {
        pVssBackupComponents.Release();
        pVssBackupComponents = NULL;
    }
    
    

    XDebugPrintf("EXITED: InMageVssRequestor::CleanupSnapShots\n");
    return hr;
}

HRESULT InMageVssRequestor::CheckWriterStatus(LPCWSTR wszOperation)
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;
    HRESULT hrResult = S_OK;
    unsigned int cWriters;
    CComPtr<IVssAsync> pVssAsync;

    XDebugPrintf("ENTERED: InMageVssRequestor::CheckWriterStatus %s\n", W2CA(wszOperation));

    do
    {
        hr = pVssBackupComponents->GatherWriterStatus(&pVssAsync);
        if(FAILED(hr))
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssBackupComponents::GatherWriterStatus() failed. hr = 0x%08x\n", hr);
            break;
        }

        CHECK_FAILURE_RETURN_VAL(WaitAndCheckForAsyncOperation(pVssAsync), hr);

        hr = pVssBackupComponents->GetWriterStatusCount(&cWriters);
        if(FAILED(hr))
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssBackupComponents::GetWriterStatusCount() failed. hr = 0x%08x\n", hr);
            break;
        }

        XDebugPrintf("Number of writers  %s = %d \n", W2CA(wszOperation), cWriters);
        XDebugPrintf("Status of each writer:\n" );
        XDebugPrintf("======================\n");

        for(unsigned iWriter = 0; iWriter < cWriters; iWriter++)
        {
            VSS_ID              idInstance;
            VSS_ID              idWriter;
            VSS_WRITER_STATE    status;
            CComBSTR            bstrWriter;
            HRESULT             hrWriterFailure;

    
            hr = pVssBackupComponents->GetWriterStatus (iWriter, &idInstance, &idWriter, &bstrWriter, &status, &hrWriterFailure);
            if (hr != S_OK) 
            {
                if(!IsWriterExcluded(W2CA(bstrWriter), idWriter, idInstance))
                {
                    DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                    DebugPrintf("FAILED: IVssBackupComponents::GetWriterStatus() failed for writer:%s. hr = 0x%08x\n",W2CA(bstrWriter), hr);
                    break;
                }

                DebugPrintf("SKIPPED: IVssBackupComponents::GetWriterStatus() failed for writer:%s. hr = 0x%08x\n", W2CA(bstrWriter), hr);
                continue;
            }

            std::string strWriter = W2CA(bstrWriter);

            bstrWriter.Empty();

            if (!WriterStateFailed(status))
            {
                continue;
            }

            // skip writers that failed but not participating in snapshot
            if (IsWriterExcluded(strWriter.c_str(), idWriter, idInstance))
            {
                DebugPrintf("Skipping excluded writer:%s\n", strWriter.c_str());
                DebugPrintf("  State:[%d] %s \n", status, gszVSSWriterState[status].c_str());
                DebugPrintf("  Last error: %s \n\n", gVssErrorString[hrWriterFailure].c_str());
                continue;
            }

            // if the writer error is not set but the writer failed, set error as non-retryable
            if (!FAILED(hrWriterFailure))
            {
                DebugPrintf("Writer name: %s\n", strWriter.c_str());
                DebugPrintf("  State:[%d] %s \n", status, gszVSSWriterState[status].c_str());
                DebugPrintf("  Last error: %s \n\n", gVssErrorString[hrWriterFailure].c_str());
                DebugPrintf("  Setting Writer Failure as VSS_E_WRITERERROR_NONRETRYABLE.\n");
                hrWriterFailure = VSS_E_WRITERERROR_NONRETRYABLE;
            }

            if (FAILED(hrWriterFailure))
            {
                unsigned char *pszInstanceId;
                unsigned char *pszWriterId;

                UuidToString(&idInstance, &pszInstanceId);
                UuidToString(&idWriter, &pszWriterId);

                std::stringstream   errorCode;
                errorCode << std::string("0x")<<std::hex << hrWriterFailure;

                std::wstring        wstrOperation(wszOperation);
                std::string     strOperation(wstrOperation.begin(), wstrOperation.end());

                std::map<std::string, std::string>      errors
                {
                    //Adding VSS Writer errors to Health Issues
                    {AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::WriteName, strWriter},
                    {AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::ErrorCode, errorCode.str()},
                    {AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::Operation, strOperation}
                };

                DebugPrintf("Calling ErrorInfo to add errors");
                AppConsistency::Get().m_vacpErrorInfo.Add(
                    AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::HealthCode,
                    errors);

                //Adding VSS Writer errors to Telemetry
                boost::replace_all(strWriter, " ", "_");
                AppConsistency::Get().m_tagTelInfo.Add(AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::WriteName, strWriter);
                AppConsistency::Get().m_tagTelInfo.Add(AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::ErrorCode, errorCode.str());
                //Operation will have spaces and Telemetry will skip the second word with space. So replace space with _
                boost::replace_all(strOperation, " ", "_");
                AppConsistency::Get().m_tagTelInfo.Add(AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::Operation, strOperation);

                RpcStringFree(&pszInstanceId);
                RpcStringFree(&pszWriterId);

                DebugPrintf("Writer name: %s\n", strWriter.c_str());
                DebugPrintf("  State:[%d] %s \n", status, gszVSSWriterState[status].c_str());
                DebugPrintf("  Last error: %s \n\n", gVssErrorString[hrWriterFailure].c_str());
            }

            switch(hrWriterFailure)
            {
                case S_OK:
                    gbRetryable = false;
                    break;

                case VSS_E_WRITERERROR_INCONSISTENTSNAPSHOT:
                    DebugPrintf("The error suggests that VACP might require additional volumes to the shadow copy.\n");
                    DebugPrintf("Can not continue.\n"); 
                    gbRetryable = false;
                    break;

                case VSS_E_WRITERERROR_OUTOFRESOURCES:
                    DebugPrintf("The error indicates that system load be reduced prior to retrying\n");
                    gbRetryable = true;
                    break;

                case VSS_E_WRITERERROR_TIMEOUT :
                    DebugPrintf("The error indicates that system load be reduced prior to retrying\n");
                    gbRetryable = true;
                    break;

                case VSS_E_WRITERERROR_RETRYABLE :
                    DebugPrintf("The error indicates that retrying without reconfiguration might work.\n");
                    gbRetryable = true;
                    break;

                case VSS_E_WRITERERROR_NONRETRYABLE:
                    DebugPrintf("The error indicates a writer error is so severe as to preclude trying to backup its data with VSS.\n");
                    DebugPrintf("Can not continue.\n"); 
                    gbRetryable = false;
                    break;

                case VSS_E_WRITER_NOT_RESPONDING:
                    // see below for special handling
                    gbRetryable = false;
                    break;

                default:
                    DebugPrintf("Status for writer %s is :\n", strWriter.c_str());
                    DebugPrintf("State:%d, Last Error:0x%08x\n", status, hrWriterFailure);
                    gbRetryable = false;
                    break;
            }

            // A writer did not respond to a GetWriterStatus call.  This means that
            // the process containing the writer died or is stopped responding.
            if (hrWriterFailure == VSS_E_WRITER_NOT_RESPONDING)
            {
                unsigned char *pszInstanceId;
                unsigned char *pszWriterId;

                UuidToString(&idInstance, &pszInstanceId);
                UuidToString(&idWriter, &pszWriterId);
            
                DebugPrintf("\nNOTE: Application \"%s\" is either died or stopped responding.\n", strWriter.c_str());
                XDebugPrintf("NOTE: WriterId = %s \n", pszWriterId);
                XDebugPrintf("NOTE: InstanceId = %s \n", pszInstanceId);            
                DebugPrintf("NOTE: It did not respond for GetWriterStatus(status = %d). Skipping it...\n\n",status);

                RpcStringFree(&pszInstanceId);
                RpcStringFree(&pszWriterId);
                                
                hrWriterFailure = S_OK;
            }

            if (FAILED(hrWriterFailure) && (IsWriterExcluded(strWriter.c_str(), idWriter, idInstance) == false))
            {
                if (SUCCEEDED(hrResult))
                    hrResult = hrWriterFailure;

                // fail at the first wtiter that has error
                break; 
            }
        }
    }while(false);

    pVssBackupComponents->FreeWriterStatus();

    XDebugPrintf("EXITED: InMageVssRequestor::CheckWriterStatus\n");
    return FAILED(hrResult) ? hrResult : hr;
}

HRESULT InMageVssRequestor::PrintWritersInfo(unsigned cWriters)
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::PrintWritersInfo\n");
    XDebugPrintf("Number of writers = %d \n", cWriters);

    for(unsigned iWriter = 0; iWriter < cWriters; iWriter++)
    {
        CComPtr<IVssExamineWriterMetadata> pInmMetadata;
        VSS_ID idInstance;

        hr = pVssBackupComponents->GetWriterMetadata(iWriter, &idInstance, &pInmMetadata);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssBackupComponents::GetWriterMetadata() failed. hr = 0x%08x \n.", hr);
            break;
        }

        VSS_ID idInstanceT;
        VSS_ID idWriter;
        CComBSTR bstrWriterName;
        VSS_USAGE_TYPE usage;
        VSS_SOURCE_TYPE source;

        hr = pInmMetadata->GetIdentity(&idInstanceT, &idWriter, &bstrWriterName, &usage, &source);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssExamineWriterMetadata::GetIdentity() failed. hr = 0x%08x \n.", hr);
            break;
        }

        if (memcmp(&idInstance, &idInstanceT, sizeof(VSS_ID)) != 0)
        {
            hr = E_FAIL;
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: Writer Instance id mismatch\n");
            break;
        }

        XDebugPrintf("\nWriter = %d WriterName = %s usage = %d source = %d\n", iWriter, W2CA(bstrWriterName), usage, source);

        unsigned cIncludeFiles, cExcludeFiles, cComponents;

        hr = pInmMetadata->GetFileCounts (&cIncludeFiles, &cExcludeFiles, &cComponents);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssExamineWriterMetadata::GetFileCounts() failed. hr = 0x%08x \n.", hr);
            break;
        }

        XDebugPrintf("IncludeFiles = %d ExcludeFiles = %d Components = %d \n",cIncludeFiles, cExcludeFiles, cComponents);
/*
        // GetBackupSchema member function is NOT defined in 
        // VSS SDK for Windows XP. It is defined in VSS SDK
        // for Windows 2003. Hence this code is commented out,
        // So that Build for Windows XP will not fail.
        
        DWORD schema = 0;
        hr = pInmMetadata->GetBackupSchema(&schema);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssExamineWriterMetadata::GetBackupSchema() failed. hr = 0x%08x \n.", hr);
            break;
        }

        DebugPrintf("BackupSchema = 0x%x \n", schema);         
*/
        for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
        {
            CComPtr<IVssWMComponent> pComponent;
            PVSSCOMPONENTINFO pInfo;

            hr = pInmMetadata->GetComponent(iComponent, &pComponent);
            if (hr != S_OK)
            {
                DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf("FAILED: IVssExamineWriterMetadata::GetComponent() failed. hr = 0x%08x \n.", hr);
                pComponent->FreeComponentInfo(pInfo);
                break;
            }

            hr = pComponent->GetComponentInfo(&pInfo);
            if (hr != S_OK)
            {
                DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf("FAILED: IVssExamineWriterMetadata::GetComponentInfo() failed. hr = 0x%08x \n.", hr);
                pComponent->FreeComponentInfo(pInfo);
                break;
            }
                
            //pInfo->cDependencies is defined in VSS SDK for Windows 2003 and above only.
            XDebugPrintf("[%d],\n\t ComponentType %d\n\t ComponentName %s\n\t ComponentCaption %s\n\t LogicalPath %s\n\t Dependents %d\n\t FileCount %d\n\t Databases %d\n\t LogFiles %d \n\n\n", iComponent, pInfo->type, W2CA(pInfo->bstrComponentName), ((pInfo->bstrCaption != NULL) ? W2CA(pInfo->bstrCaption) : "NULL"), ((pInfo->bstrLogicalPath != NULL) ? W2CA(pInfo->bstrLogicalPath) : "NULL"), pInfo->cDependencies, pInfo->cFileCount, pInfo->cDatabases, pInfo->cLogFiles);
            
            pComponent->FreeComponentInfo(pInfo);

            pComponent.Release();
            pComponent = NULL;
        }
    }

    XDebugPrintf("EXITED: InMageVssRequestor::PrintWritersInfo\n");
    return hr;
}

HRESULT InMageVssRequestor::WaitAndCheckForAsyncOperation(IVssAsync *pva)
{
    HRESULT hr = S_OK;
    HRESULT hrStatus = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::WaitAndCheckForAsyncOperation\n");

    do
    {
        CHECK_SUCCESS_RETURN_VAL(pva->Wait(), hr);

        CHECK_SUCCESS_RETURN_VAL(pva->QueryStatus(&hrStatus, NULL), hr);

    } while(FALSE);

    if ((hr == S_OK) && (hrStatus != VSS_S_ASYNC_FINISHED))
    {
        hr = E_FAIL;
        std::stringstream ss;
        ss << "Async::QueryStatus() returned async op not finished. hrStatus = " << std::hex << hrStatus
           << ". hr = " << std::hex << hr << ".";
        DebugPrintf("%s\n", ss.str().c_str());
        AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
    }

    XDebugPrintf("EXITED: InMageVssRequestor::WaitAndCheckForAsyncOperation\n");
    return hr;
}

void InMageVssRequestor::DiscoverExcludedWriters(VssAppInfo_t &VssAppInfo)
{
    XDebugPrintf("ENTERED: InMageVssRequestor::DiscoverExcludedWriters\n");
    
    if ((VssAppInfo.ComponentsInfo.size() ==0 ) && 
       (m_dwProtectedDriveMask == 0 || VssAppInfo.dwDriveMask == 0))
    {
        DebugPrintf("%s: Writer Name : %s, excluded because there are no components or volumes.\n", FUNCTION_NAME, VssAppInfo.AppName.c_str());
        VssAppInfo.isExcluded = true; //exclude by default
        return;
    }

    if((VssAppInfo.idWriter.empty()) || 
      (VssAppInfo.AppName.empty()) )
    {
        DebugPrintf("%s: Writer excluded because there are no name.\n", FUNCTION_NAME);
        VssAppInfo.isExcluded = true;
        return;
    }

    if (!gbUseInMageVssProvider && (stricmp(VssAppInfo.idWriter.c_str(), (char*)gpInMageVssWriterGUID) == 0) )
    {
        VssAppInfo.isExcluded = false;
        return;
    }

    // Do NOT include applications unless if they are specified using -a or -w option.
    for(unsigned int iApp = 0; iApp < m_ACSRequest.Applications.size(); iApp++)
    {
         AppSummary_t &ASum = m_ACSRequest.Applications[iApp];

        if(m_ACSRequest.bWriterInstance)
        {
            const char *wName = MapWriterName2VssWriter(ASum.m_appName.c_str());
            if(!VssAppInfo.szWriterInstanceName.empty())
                continue;

            if (wName == NULL || (stricmp(wName, VssAppInfo.szWriterInstanceName.c_str()) != 0))
                    continue;
        }
        else
        {
            const char *wName = MapApplication2VssWriter(ASum.m_appName.c_str());
            if (wName == NULL || (stricmp(wName, VssAppInfo.AppName.c_str()) != 0))
                    continue;
        }

        VssAppInfo.isExcluded = false;
        XDebugPrintf("\nApplication: %s is included in the snapshot\n", ASum.m_appName.c_str());
        dwNumberofWritersTobeEnabled++;
        return;
    }
    
    XDebugPrintf("EXITED: InMageVssRequestor::DiscoverExcludedWriters\n");
}

bool InMageVssRequestor::IsWriterExcluded(const char *WriterName, VSS_ID idWriter, VSS_ID idInstance)
{
    XDebugPrintf("ENTERED: InMageVssRequestor::IsWriterExcluded\n");

    bool isExcluded = false;

    for(unsigned int index = 0; index < VssWriters.size(); index++)
    {
        VssAppInfo_t &VssApp = VssWriters[index];

        if (stricmp(WriterName, VssApp.AppName.c_str()) == 0)
        {
            unsigned char *pszInstanceId;
            unsigned char *pszWriterId;

            UuidToString(&idInstance, &pszInstanceId);
            UuidToString(&idWriter, &pszWriterId);

            if (stricmp((const char *)pszWriterId, VssApp.idWriter.c_str()) == 0 &&
                stricmp((const char *)pszInstanceId, VssApp.idInstance.c_str()) == 0)
            {
                RpcStringFree(&pszInstanceId);
                RpcStringFree(&pszWriterId);

                isExcluded = VssApp.isExcluded;
                break;
            }

            RpcStringFree(&pszInstanceId);
            RpcStringFree(&pszWriterId);        
        }
    }

    XDebugPrintf("EXITED: InMageVssRequestor::IsWriterExcluded\n");

    return isExcluded;
}


void InMageVssRequestor::DiscoverTopLevelComponents(vector<ComponentInfo_t> &ComponentsInfo)
{
    XDebugPrintf("ENTERED: InMageVssRequestor::DiscoverTopLevelComponents\n");

    for(unsigned i = 0; i < ComponentsInfo.size(); i++)
    {
        ComponentsInfo[i].isTopLevel = true;
        for(unsigned int j = 0; j < ComponentsInfo.size(); j++)
        {
            if (ComponentsInfo[j].IsAncestorOf(ComponentsInfo[i]))
            {
                ComponentsInfo[i].isTopLevel = false;
                ComponentsInfo[j].dwDriveMask |= ComponentsInfo[i].dwDriveMask;

                //XDebugPrintf("Component %s is descendent of Component %s\n", (ComponentsInfo[i].captionName ? ComponentsInfo[i].captionName : ComponentsInfo[i].componentName), (ComponentsInfo[j].captionName ? ComponentsInfo[j].captionName : ComponentsInfo[j].componentName));

                for (unsigned int iVol = 0; iVol < ComponentsInfo[i].affectedVolumes.size(); iVol++)
                    ComponentsInfo[j].affectedVolumes.push_back(ComponentsInfo[i].affectedVolumes[iVol]);

                for (unsigned int iFiles = 0; iFiles < ComponentsInfo[i].FileGroupFiles.size(); iFiles++)
                    ComponentsInfo[j].FileGroupFiles.push_back(ComponentsInfo[i].FileGroupFiles[iFiles]);

                for (unsigned int iFiles = 0; iFiles < ComponentsInfo[i].DataBaseFiles.size(); iFiles++)
                    ComponentsInfo[j].DataBaseFiles.push_back(ComponentsInfo[i].DataBaseFiles[iFiles]);

                for (unsigned int iFiles = 0; iFiles < ComponentsInfo[i].DataBaseLogFiles.size(); iFiles++)
                    ComponentsInfo[j].DataBaseLogFiles.push_back(ComponentsInfo[i].DataBaseLogFiles[iFiles]);

                for (unsigned int iMountPoint = 0; iMountPoint < ComponentsInfo[i].volMountPoint_info.dwCountVolMountPoint; iMountPoint++)
                {
                    AddVolumeMoutPointInfo(ComponentsInfo[j].volMountPoint_info,ComponentsInfo[i].volMountPoint_info.vVolumeMountPoints[iMountPoint]);
                }
            }
        }
    }

    XDebugPrintf("EXITED: InMageVssRequestor::DiscoverTopLevelComponents\n");
}

HRESULT    InMageVssRequestor::ProcessWMFiledescEx(IVssWMFiledesc *pFileDesc, ComponentInfo_t &Component, std::vector<std::string> &FilePathVector)
{
    using namespace std;
    USES_CONVERSION;
    HRESULT hr = S_OK;
    DWORD retval = 0;

    char *affectedVolume = new (nothrow) char[MAX_PATH + 1];
    if (affectedVolume == NULL)
    {
        DebugPrintf("%s: failed to allocate memory for affected volume.\n", __FUNCTION__);
        hr = VACP_MEMORY_ALLOC_FAILURE;
        return hr;
    }

    CComBSTR bstrPath;
    do
    {
        CHECK_SUCCESS_RETURN_VAL(pFileDesc->GetPath(&bstrPath), hr);
        
        memset((void*)affectedVolume,0x00,MAX_PATH + 1);
        //do not validate volume mount points
        bool validateMountPoint = false;

        CHECK_SUCCESS_RETURN_VAL(GetVolumeRootPathEx(W2CA(bstrPath), affectedVolume, MAX_PATH, validateMountPoint), hr);

        DWORD volIndex = 0;
        DWORD volBitMask = 0;
        VOLMOUNTPOINT volMP;
        char baseVolPath[MAX_PATH+1] = {0};
        GetVolumePathName(W2CA(bstrPath),baseVolPath,MAX_PATH);
        if(strstr(baseVolPath,"\\\\?\\Volume"))
        {
            DWORD dwVolPathSize = 0;
            char szVolumePath[MAX_PATH+1] = {0};

            GetVolumePathNamesForVolumeName(baseVolPath,szVolumePath,MAX_PATH,&dwVolPathSize);
            memset(baseVolPath,0,MAX_PATH+1);
            inm_strcpy_s(baseVolPath,ARRAYSIZE(baseVolPath),szVolumePath);
        }
        if(strstr(baseVolPath,"\\\\?\\GLOBALROOT"))
        {
            DWORD dwVolPathSize = 0;
            char szVolumePath[MAX_PATH+1] = {0};

            GetVolumePathNamesForVolumeName(baseVolPath,szVolumePath,MAX_PATH,&dwVolPathSize);
            memset(baseVolPath,0,MAX_PATH+1);
            inm_strcpy_s(baseVolPath,ARRAYSIZE(baseVolPath),szVolumePath);
        }
        volMP.strVolumeName = baseVolPath;
        volMP.strVolumeMountPoint = affectedVolume;
        if(validateMountPoint)
        {
            Component.AddVolumeMoutPointInfo(volMP);
        }
        else
        {
            //BUG#5604: we need to consider only drive letter and \\?\volume\<GUID> else has to be skipped.
            //CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeName(affectedVolume, &volIndex, validateMountPoint), hr);
            CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeNameEx(affectedVolume, &volIndex,validateMountPoint), hr);
        
            //FilePathVector.push_back(W2CA(bstrPath));
            if( 0 != volIndex)
            {
              volBitMask = (1<<volIndex);
              if ((Component.dwDriveMask & volBitMask) == 0)
              {
                  Component.dwDriveMask |= volBitMask;
                  Component.affectedVolumes.push_back(affectedVolume);
                  Component.totalVolumes++;
              }
            }
        }
    } while(FALSE);

    bstrPath.Empty();
           
    if (affectedVolume)
    {
        delete[] affectedVolume;
        affectedVolume = NULL;
    }

    return hr;
}


HRESULT InMageVssRequestor::AddVolumesToSnapshotSetEx()
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::AddVolumesToSnapshotSetEx\n");

    GetExcludedVolumeList();

    do{
        //Get UUID of MS Software Shadow copy provider or Custom provider
        if(!GetProviderUUID())
        {
            hr = E_FAIL;
            break;
        }

        char szVolumeName[MAX_PATH];

        szVolumeName[0] = 'X';
        szVolumeName[1] = ':';
        szVolumeName[2] = '\\';
        szVolumeName[3] = '\0';
        
        for( int i = 0; i < MAX_DRIVES; i++ )
        {
            VSS_ID SnapshotID;
            DWORD dwDriveMask = 1<<i;

            if(0 == ( m_dwProtectedDriveMask &  dwDriveMask ) ) 
            {
                continue;
            }

            szVolumeName[0] = 'A' + i;

            if(gbSkipUnProtected)
            {
                if (!IsValidVolumeEx(szVolumeName, true, pVssBackupComponentsEx))
                {
                    hr = S_FALSE;
                    inm_printf("\n %s is an Invalid Volume.",szVolumeName);
                    continue;
                }
                char szVolumePathName[MAX_PATH] = {0};
                if(!GetVolumePathName((LPCSTR)szVolumeName,szVolumePathName,MAX_PATH))
                {
                    hr = S_FALSE;
                    inm_printf("\nCould not get the Volume Path Name for %s and hence not selecting this component for Backup!",szVolumeName);
                    continue;
                }
                //convert the VolumePathName to Volume GUID
                unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
                WCHAR *VolumeGUID1 = new (nothrow) WCHAR[GUID_SIZE_IN_CHARS];
                if(VolumeGUID1 == NULL)
                {
                    continue;
                }
                
                memset((void*)VolumeGUID1,0x00,GuidSize);
                if(!MountPointToGUID((PTCHAR)szVolumePathName,VolumeGUID1,GuidSize))
                {
                    delete VolumeGUID1;
                    VolumeGUID1 = NULL;
                    continue;
                }

                // for disk filter driver include the volumes by default 
                if (!gbUseDiskFilter)
                {
                    //Add this volume only if it is protected,otherwise don't add; To determine if a volume is protected
                    if (IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
                    {
                        etWriteOrderState DriverWriteOrderState;
                        DWORD dwLastError = 0;
                        if (!CheckDriverStat(ghControlDevice, VolumeGUID1, &DriverWriteOrderState, &dwLastError))
                        {
                            inm_printf("\nVolume %s is not protected @Time: %s \n", szVolumeName, GetLocalSystemTime().c_str());
                            continue;
                        }
                    }
                }
                if(VolumeGUID1)
                {
                    delete VolumeGUID1;
                    VolumeGUID1 = NULL;
                }
            }
           
            if (gExcludedVolumes.find(std::string(szVolumeName).substr(0, 2)) != gExcludedVolumes.end()) {
                DebugPrintf("%s: Excluded volume %s from the SnapshotSet \n", FUNCTION_NAME, szVolumeName);
                continue;
            }

            DebugPrintf("Added volume %s to the SnapshotSet \n", szVolumeName);

            hr = pVssBackupComponents->AddToSnapshotSet(A2W(szVolumeName), uuidProviderID, &SnapshotID);
            if (hr != S_OK)
            {
                std::string strVssError;
                auto iterVssErrorStr = gVssErrorString.find(hr);
                if (iterVssErrorStr != gVssErrorString.end())
                {
                    strVssError = iterVssErrorStr->second;
                }

                auto iterVssVolSkippedReason = gVssVolumeSkippedReason.find(hr);
                if (iterVssVolSkippedReason != gVssVolumeSkippedReason.end())
                {
                    DebugPrintf("%s: Volume %s is  skipped as hr = 0x%08x (%s).\n",
                        __FUNCTION__, szVolumeName, hr, strVssError.c_str());

                    hr = S_OK;

                    std::stringstream errmsg;
                    errmsg << "Volume " << szVolumeName << " skipped, reason " << strVssError;
                    ADD_TO_VACP_FAIL_MSGS(hr, errmsg.str(), hr);
                    continue;
                }

                DebugPrintf("%s: IVssBackupComponents::AddToSnapshotSet() failed for volume %s, hr = 0x%08x (%s).\n.",
                    __FUNCTION__, szVolumeName, hr, strVssError.c_str());
                break;
            }
            m_ACSRequest.vApplyTagToTheseVolumes.push_back((std::string)szVolumeName);

            //If a Snapshot is added to the SnapshotSet, then add the SnapshotId to the vector of SnapshotIds.
            if(S_OK == hr)
            {
                m_vSnapshotIds.push_back(SnapshotID);
            }
        }

        string strVolumeName;
        string strVolumeMountPointGuid;
        DWORD dwDriveMask = 1<<30;
        if( m_dwProtectedDriveMask &  dwDriveMask ) 
        {
            for(DWORD j = 0; j < m_ACSRequest.volMountPoint_info.dwCountVolMountPoint; j++)
            {
                VSS_ID SnapshotID;
                
                strVolumeName = m_ACSRequest.volMountPoint_info.vVolumeMountPoints[j].strVolumeName;
                strVolumeMountPointGuid = m_ACSRequest.volMountPoint_info.vVolumeMountPoints[j].strVolumeMountPoint;

                size_t stringSize = strVolumeName.length();
                if(strVolumeName.at(stringSize-1) != '\\')
                {
                    strVolumeName = strVolumeName + "\\";
                }

                if (gbSkipUnProtected && !gbUseDiskFilter)
                {
                    if(IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
                    {
                        etWriteOrderState DriverWriteOrderState;
                        DWORD dwLastError = 0;
                        std::string strGuid = strVolumeMountPointGuid.substr(11,GUID_SIZE_IN_CHARS);
                        //Add this volume only if it is protected,otherwise don't add; To determine if a volume is protected
                        if(!CheckDriverStat(ghControlDevice,A2W(strGuid.c_str()),&DriverWriteOrderState,&dwLastError))
                        {
                            inm_printf("\n Volume %s is not protected @Time: %s \n", strVolumeName.c_str(),GetLocalSystemTime().c_str());
                            continue;
                        }
                    }
                }

                if (gExcludedVolumes.find(strVolumeName.substr(0,2)) != gExcludedVolumes.end()) {
                    DebugPrintf("%s: Excluded volume %s from the SnapshotSet \n", FUNCTION_NAME, strVolumeName.c_str());
                    continue;
                }


                DebugPrintf("Added volume %s to the SnapshotSet \n", strVolumeName.c_str());
                hr = pVssBackupComponents->AddToSnapshotSet(A2W(strVolumeName.c_str()), uuidProviderID, &SnapshotID);                
                if (hr != S_OK)
                {
                    std::string strVssError;
                    auto iterVssErrorStr = gVssErrorString.find(hr);
                    if (iterVssErrorStr != gVssErrorString.end())
                    {
                        strVssError = iterVssErrorStr->second;
                    }

                    auto iterVssVolSkippedReason = gVssVolumeSkippedReason.find(hr);
                    if (iterVssVolSkippedReason != gVssVolumeSkippedReason.end())
                    {
                        DebugPrintf("%s: Volume %s is  skipped as hr = 0x%08x (%s).\n",
                            __FUNCTION__, strVolumeName.c_str(), hr, strVssError.c_str());

                        hr = S_OK;

                        std::stringstream errmsg;
                        errmsg << "Volume " << strVolumeName << " skipped, reason " << (strVssError.empty() ? std::to_string(hr) : strVssError);
                        ADD_TO_VACP_FAIL_MSGS(hr, errmsg.str(), hr);
                        continue;
                    }

                    DebugPrintf("%s: IVssBackupComponents::AddToSnapshotSet() failed for volume %s, hr = 0x%08x (%s).\n.",
                        __FUNCTION__, strVolumeName.c_str(), hr, strVssError.c_str());
                    break;
                    
                }
                AddVolumeMoutPointInfo(m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info,m_ACSRequest.volMountPoint_info.vVolumeMountPoints[j]);

                //If a Snapshot is added to the SnapshotSet, then add the SnapshotId to the vector of SnapshotIds.
                if(S_OK == hr)
                {
                    m_vSnapshotIds.push_back(SnapshotID);
                }        
            }
        }
    }while(0);

    XDebugPrintf("EXITED: InMageVssRequestor::AddVolumesToSnapshotSetEx\n");
    return hr;
}

HRESULT InMageVssRequestor::AddVolumeMoutPointInfo(VOLMOUNTPOINT& volMP)
{
    HRESULT hr = E_FAIL;
    if(!IsVolMountAlreadyExist(volMP))
    {
        m_ACSRequest.volMountPoint_info.vVolumeMountPoints.push_back(volMP);  
        m_ACSRequest.volMountPoint_info.dwCountVolMountPoint++;
        hr = S_OK;
    }
    return hr;
}

HRESULT InMageVssRequestor::AddVolumeMoutPointInfo(VOLMOUNTPOINT_INFO& volumeMountPoint_info,VOLMOUNTPOINT& volMP)
{
    HRESULT hr = E_FAIL;
    if(!IsVolMountAlreadyExist(volumeMountPoint_info,volMP))
    {
        volumeMountPoint_info.vVolumeMountPoints.push_back(volMP);  
        volumeMountPoint_info.dwCountVolMountPoint++;
        hr = S_OK;
    }
    return hr;
}

bool InMageVssRequestor::IsVolMountAlreadyExist(VOLMOUNTPOINT& volMP )
{
    bool ret = false;
    string strVolName;
    for(DWORD  i = 0; i <m_ACSRequest.volMountPoint_info.dwCountVolMountPoint; i++)
    {
        strVolName = m_ACSRequest.volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint;
        if(!strnicmp(volMP.strVolumeMountPoint.c_str(), strVolName.c_str(),strVolName.size()))
        {
            ret = true;
        }
    }
    return ret;
}

bool InMageVssRequestor::IsVolMountAlreadyExist(VOLMOUNTPOINT_INFO& volMountPoint_info, VOLMOUNTPOINT& volMP )
{
    bool ret = false;
    string strVolName;
    for(DWORD  i = 0; i <volMountPoint_info.dwCountVolMountPoint; i++)
    {
        strVolName = volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint;
        if(!strnicmp(volMP.strVolumeMountPoint.c_str(), strVolName.c_str(),strVolName.size()))
        {
            ret = true;
        }
    }
    return ret;
}


bool InMageVssRequestor::IsVolMountAlreadyExistInApp(VOLMOUNTPOINT& volMP,VssAppInfo_t &App)
{
    bool ret = false;
    string strVolName;
    for(DWORD  i = 0; i <App.volMountPoint_info.dwCountVolMountPoint; i++)
    {
        strVolName = App.volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint;
        if(!strnicmp(volMP.strVolumeMountPoint.c_str(), strVolName.c_str(),strVolName.size()))
        {
            ret = true;
        }
    }
    return ret;
}

HRESULT InMageVssRequestor::FillInVssAppInfoEx(CComPtr<IVssExamineWriterMetadataEx> pInmMetadataEx, 
                                               VssAppInfo_t &App,
                                               DWORD &dwAppTobeAdded,
                                               bool bSkipEnumeration)
{
    using namespace std;
    USES_CONVERSION;
    dwAppTobeAdded  = true;
    HRESULT hr = S_OK;
  do
    {
        VSS_ID idInstance = GUID_NULL;
        VSS_ID idWriter = GUID_NULL;
        CComBSTR bstrWriterName;
        CComBSTR bstrWriterInstanceName;
        VSS_USAGE_TYPE usage = VSS_UT_UNDEFINED;
        VSS_SOURCE_TYPE source= VSS_ST_UNDEFINED;
        
        // Get writer identity information
        CHECK_SUCCESS_RETURN_VAL(pInmMetadataEx->GetIdentityEx(
            &idInstance,
            &idWriter,
            &bstrWriterName,
            &bstrWriterInstanceName,
            &usage,
            &source),
            hr);

        if(0 == stricmp(W2CA(bstrWriterName),"System Writer"))
            bSkipEnumeration = !gbEnumSW;

#ifdef VACP_SERVER
    if(wcscmp(bstrWriterName.m_str, L"Microsoft Hyper-V VSS Writer") && wcscmp(bstrWriterName.m_str, L"InMageVSSWriter"))
    {
        dwAppTobeAdded = false;
        break;
    }
#endif
        unsigned char *pszInstanceId;
        unsigned char *pszWriterId;
        
        //Convert GUIDs to rpc string
        UuidToString(&idInstance, &pszInstanceId); 
        UuidToString(&idWriter, &pszWriterId);
        
        //Initialize the VsswriterName
        App.AppName = W2CA(bstrWriterName);//here WriterName is treated as AppName in our code!
        App.idInstance = (const char *)pszInstanceId;
        App.idWriter = (const char *)pszWriterId;
        if (bstrWriterInstanceName)
            App.szWriterInstanceName = W2CA(bstrWriterInstanceName);
        
        bstrWriterName.Empty();
        bstrWriterInstanceName.Empty();

        //Free rpc strings
        RpcStringFree(&pszInstanceId);
        RpcStringFree(&pszWriterId);

        // Get file counts      
        unsigned cIncludeFiles = 0;
        unsigned cExcludeFiles = 0;
        unsigned cComponents = 0;

        CHECK_SUCCESS_RETURN_VAL(pInmMetadataEx->GetFileCounts(
            &cIncludeFiles,
            &cExcludeFiles, 
            &cComponents),
            hr);

        char *affectedVolume = new (nothrow) char[MAX_PATH + 1];
        if (affectedVolume == NULL)
        {
            DebugPrintf("%s: failed to allocate memory for affected volume.\n", __FUNCTION__);
            hr = VACP_MEMORY_ALLOC_FAILURE;
            CHECK_SUCCESS_RETURN_VAL(hr, hr);
        }

        // Enumerate components
        for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
        {
            // Get component
            CComPtr<IVssWMComponent> pComponent = NULL;
            CHECK_SUCCESS_RETURN_VAL(pInmMetadataEx->GetComponent(
                iComponent, 
                &pComponent),
                hr);

            // Get the component info
            PVSSCOMPONENTINFO pInfo = NULL;
            CHECK_SUCCESS_RETURN_VAL(pComponent->GetComponentInfo(&pInfo), hr);

            ComponentInfo_t Component;

            Component.componentName = W2CA(pInfo->bstrComponentName);
            if (pInfo->bstrLogicalPath)
                Component.logicalPath = W2CA(pInfo->bstrLogicalPath);
            if (pInfo->bstrCaption)
                Component.captionName = W2CA(pInfo->bstrCaption);
            Component.componentType = pInfo->type;
            Component.notifyOnBackupComplete = pInfo->bNotifyOnBackupComplete;
            Component.isSelectable = pInfo->bSelectable;
            Component.fullPath = Component.GetFullPath();
            Component.dwDriveMask = 0;
            Component.totalVolumes = 0;

            bool validateMountPoint = false;

            memset((void*)affectedVolume,0x00,MAX_PATH + 1);
            
            if(bSkipEnumeration)
            {
              char bufSysVol[4096] = {0};
              DWORD nRet = GetWindowsDirectory(bufSysVol,4096);
              if(!nRet)
              {
                inm_printf("\nError in getting the system volume for the system writer.");
                hr = VACP_E_ERROR_GETTING_SYSVOL_FOR_SYSTEMWRITER;

                if (affectedVolume)
                {
                    delete[] affectedVolume;
                    affectedVolume = NULL;
                }

                return hr;
              }
              else
              {
                CHECK_SUCCESS_RETURN_VAL(GetVolumeRootPathEx(bufSysVol, affectedVolume, MAX_PATH, validateMountPoint), hr);

                    DWORD volIndex = 0;
                    DWORD volBitMask = 0;
                    VOLMOUNTPOINT volMP;
                    char baseVolPath[MAX_PATH+1] = {0};
                    GetVolumePathName(bufSysVol,baseVolPath,MAX_PATH);
                    if(strstr(baseVolPath,"\\\\?\\Volume"))
                    {
                        DWORD dwVolPathSize = 0;
                        char szVolumePath[MAX_PATH+1] = {0};

                        GetVolumePathNamesForVolumeName(baseVolPath,szVolumePath,MAX_PATH,&dwVolPathSize);
                        memset(baseVolPath,0,MAX_PATH+1);
                        inm_strcpy_s(baseVolPath,ARRAYSIZE(baseVolPath),szVolumePath);      
                    }
                  
                    if(validateMountPoint)
                    {
                        volMP.strVolumeName = baseVolPath;
                        volMP.strVolumeMountPoint = affectedVolume;
                        Component.AddVolumeMoutPointInfo(volMP);
                    }
                    else
                    { 
                        CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeNameEx(affectedVolume, &volIndex,validateMountPoint), hr);                  
                        if( 0 != volIndex)
                        {
                          volBitMask = (1<<volIndex);
                          if ((Component.dwDriveMask & volBitMask) == 0)
                          {
                              Component.dwDriveMask |= volBitMask;
                              Component.affectedVolumes.push_back(affectedVolume);
                              Component.totalVolumes++;
                          }
                        }
                    }//not a mount point
              }//end of got the System Volume form Windows API instead of VSS's System Writer
            }//end of bSkipEnumeration
            else
            {
              //Process Regular Files
              for(unsigned i = 0; i < pInfo->cFileCount; i++)
              {
                  IVssWMFiledesc *pFileDesc = NULL;
                  CHECK_SUCCESS_RETURN_VAL(pComponent->GetFile (i, &pFileDesc), hr);
                  CHECK_SUCCESS_RETURN_VAL(ProcessWMFiledescEx(pFileDesc, Component, Component.FileGroupFiles), hr);
                  pFileDesc->Release();
                  pFileDesc = NULL;
              }

              CHECK_SUCCESS_RETURN_VAL(hr, hr);

              //Process Database Files
              for(unsigned i = 0; i < pInfo->cDatabases; i++)
              {
                  IVssWMFiledesc *pFileDesc = NULL;
                  CHECK_SUCCESS_RETURN_VAL(pComponent->GetDatabaseFile (i, &pFileDesc), hr);
                  CHECK_SUCCESS_RETURN_VAL(ProcessWMFiledescEx(pFileDesc, Component, Component.DataBaseFiles), hr);
                  pFileDesc->Release();
                  pFileDesc = NULL;
              }

              CHECK_SUCCESS_RETURN_VAL(hr, hr);

              //Process Database LogFiles
              for(unsigned i = 0; i < pInfo->cLogFiles; i++)
              {
                  IVssWMFiledesc *pFileDesc = NULL;
                  CHECK_SUCCESS_RETURN_VAL(pComponent->GetDatabaseLogFile (i, &pFileDesc), hr);
                  CHECK_SUCCESS_RETURN_VAL(ProcessWMFiledescEx(pFileDesc, Component, Component.DataBaseLogFiles), hr);
                  pFileDesc->Release();
                  pFileDesc = NULL;
              }
              CHECK_SUCCESS_RETURN_VAL(hr, hr);
            }

            
            pComponent->FreeComponentInfo(pInfo);

            for(unsigned i=0; i<Component.affectedVolumes.size(); i++)
            {
                const char *volumeName;
                DWORD volIndex = 0;
                DWORD volBitMask = 0;

                volumeName = Component.affectedVolumes[i].c_str();

                bool validateMountPoint = false;
                CHECK_SUCCESS_RETURN_VAL(GetVolumeIndexFromVolumeNameEx(volumeName, &volIndex, validateMountPoint), hr);

                volBitMask = (1 << volIndex);

                Component.dwDriveMask |= volBitMask;

                if ((App.dwDriveMask & volBitMask) == 0)
                {
                    App.dwDriveMask |= volBitMask;
                    App.affectedVolumes.push_back(volumeName);
                    App.totalVolumes++;
                }
            }
            for( unsigned j = 0; j < Component.volMountPoint_info.dwCountVolMountPoint; j++)
            {
                string volumeName;
                DWORD volIndex = 30;
                DWORD volBitMask = 0;

                volBitMask = (1 << volIndex);
                Component.dwDriveMask |= volBitMask;

                App.dwDriveMask |= volBitMask;
                
                if(!IsVolMountAlreadyExistInApp(Component.volMountPoint_info.vVolumeMountPoints[j],App))
                {
                    App.volMountPoint_info.dwCountVolMountPoint++;
                    App.volMountPoint_info.vVolumeMountPoints.push_back(Component.volMountPoint_info.vVolumeMountPoints[j]);
                    App.totalVolumes++;
                }
            }
            App.ComponentsInfo.push_back(Component);

            pComponent.Release();
            pComponent = NULL;

            CHECK_SUCCESS_RETURN_VAL(hr, hr);
        }

        if (affectedVolume)
        {
            delete[] affectedVolume;
            affectedVolume = NULL;
        }


        CHECK_SUCCESS_RETURN_VAL(hr, hr);

        DiscoverTopLevelComponents(App.ComponentsInfo);

    } while(FALSE);

    return hr;
}

HRESULT InMageVssRequestor::PrintWritersInfoEx(unsigned cWriters)
{
    using namespace std;
    USES_CONVERSION;

    HRESULT hr = S_OK;

    XDebugPrintf("ENTERED: InMageVssRequestor::PrintWritersInfoEx\n");
    XDebugPrintf("Number of writers = %d \n", cWriters);
    CComPtr<IVssExamineWriterMetadataEx> pInmMetadataEx;

    for(unsigned iWriter = 0; iWriter < cWriters; iWriter++)
    {
        VSS_ID idInstance;

        hr = pVssBackupComponentsEx->GetWriterMetadataEx(iWriter, &idInstance, &pInmMetadataEx);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssBackupComponentsEx::GetWriterMetadataEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        VSS_ID idInstanceT;
        VSS_ID idWriter;
        CComBSTR bstrWriterName;
        CComBSTR bstrWriterInstanceName;
        VSS_USAGE_TYPE usage;
        VSS_SOURCE_TYPE source;

        hr = pInmMetadataEx->GetIdentityEx(&idInstanceT, &idWriter, &bstrWriterName,&bstrWriterInstanceName, &usage, &source);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssExamineWriterMetadataEx::GetIdentityEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        if (memcmp(&idInstance, &idInstanceT, sizeof(VSS_ID)) != 0)
        {
            hr = E_FAIL;
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: Writer Instance id mismatch\n");
            break;
        }

        if(bstrWriterInstanceName)
        {
            XDebugPrintf("\nWriter = %d WriterName = %s WriterInstanceName = %s usage = %d source = %d\n\n", iWriter, W2CA(bstrWriterName), W2CA(bstrWriterInstanceName),usage, source);
        }
        else
        {
            XDebugPrintf("\nWriter = %d WriterName = %s usage = %d source = %d\n\n", iWriter, W2CA(bstrWriterName), usage, source);
        }

        unsigned cIncludeFiles, cExcludeFiles, cComponents;

        hr = pInmMetadataEx->GetFileCounts (&cIncludeFiles, &cExcludeFiles, &cComponents);

        if (hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssExamineWriterMetadataEx::GetFileCounts() failed. hr = 0x%08x \n.", hr);
            break;
        }

        XDebugPrintf("IncludeFiles = %d ExcludeFiles = %d Components = %d \n",cIncludeFiles, cExcludeFiles, cComponents);

        for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
        {
            CComPtr<IVssWMComponent> pComponent;
            PVSSCOMPONENTINFO pInfo;

            hr = pInmMetadataEx->GetComponent(iComponent, &pComponent);
            if (hr != S_OK)
            {
                DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf("FAILED: IVssExamineWriterMetadataEx::GetComponent() failed. hr = 0x%08x \n.", hr);
                pComponent->FreeComponentInfo(pInfo);
                break;
            }

            hr = pComponent->GetComponentInfo(&pInfo);
            if (hr != S_OK)
            {
                DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf("FAILED: IVssWMComponent::GetComponentInfo() failed. hr = 0x%08x \n.", hr);
                pComponent->FreeComponentInfo(pInfo);
                break;
            }
                
            //pInfo->cDependencies is defined in VSS SDK for Windows 2003 and above only.
            XDebugPrintf("[%d],\n\t ComponentType %d \
                         \n\t ComponentName %s \
                         \n\t ComponentCaption %s \
                         \n\t LogicalPath %s \
                         \n\t Dependents %d \
                         \n\t FileCount %d\
                         \n\t Databases %d \
                         \n\t LogFiles %d \
                         \n\t NotifyOnBackupComplete %d \
                         \n\t Selectable %d \
                         \n\n\n",
                         iComponent,
                         pInfo->type,
                         W2CA(pInfo->bstrComponentName),
                         ((pInfo->bstrCaption != NULL) ? W2CA(pInfo->bstrCaption) : "NULL"),
                         ((pInfo->bstrLogicalPath != NULL) ? W2CA(pInfo->bstrLogicalPath) : "NULL"),
                         pInfo->cDependencies,
                         pInfo->cFileCount,
                         pInfo->cDatabases,
                         pInfo->cLogFiles,
                         pInfo->bNotifyOnBackupComplete,
                         pInfo->bSelectable);
         
            bstrWriterName.Empty();
            bstrWriterInstanceName.Empty();

            pComponent->FreeComponentInfo(pInfo);

            pComponent.Release();
            pComponent = NULL;
        }
    
        pInmMetadataEx.Release();
        pInmMetadataEx = NULL;
    }
    
    if (pInmMetadataEx)
    {
        pInmMetadataEx.Release();
        pInmMetadataEx = NULL;
    }

    XDebugPrintf("EXITED: InMageVssRequestor::PrintWritersInfoEx\n");
    return hr;
}

HRESULT InMageVssRequestor::DiscoverWritersTobeEnabled(CLIRequest_t CmdRequest)
{
    using namespace std;
    USES_CONVERSION;
    HRESULT hr = S_OK;

    CComPtr<IVssAsync>              pva;
    CComPtr<IVssBackupComponents>   pvb;
    CComPtr<IVssBackupComponentsEx> pvbEx;

    unsigned int nWriters = 0;

    XDebugPrintf("ENTERED: InMageVssRequestor::DiscoverWritersTobeEnabled\n");

    // while using the VSS Provider start at index 0 as InMage VSS Writer
    // is not included.
    if (gbUseInMageVssProvider)
        dwNumberofWritersTobeEnabled = 0;

    do
    {
        if (!IsInitialized())
        {
            CHECK_SUCCESS_RETURN_VAL(Initialize(), hr);
        }

        CHECK_SUCCESS_RETURN_VAL(CreateVssBackupComponents(&pvb), hr);
            
        pvb->QueryInterface(IID_IVssBackupComponentsEx,(void**)&pvbEx);

        CHECK_SUCCESS_RETURN_VAL(pvbEx->InitializeForBackup(), hr);
            
        CHECK_SUCCESS_RETURN_VAL(pvbEx->SetBackupState (false,true,VSS_BT_COPY,false), hr); 

        if (!gbPersist)
        {
            CHECK_SUCCESS_RETURN_VAL(pvbEx->SetContext(VSS_CTX_BACKUP | VSS_VOLSNAP_ATTR_NO_AUTORECOVERY), hr);
        }
        else
        {
            CHECK_SUCCESS_RETURN_VAL(pvbEx->SetContext(VSS_CTX_APP_ROLLBACK), hr);
        }

        CHECK_SUCCESS_RETURN_VAL(pvbEx->GatherWriterMetadata(&pva), hr);

        CHECK_FAILURE_RETURN_VAL(WaitAndCheckForAsyncOperation(pva), hr);
                   
        CHECK_SUCCESS_RETURN_VAL(pvbEx->GetWriterMetadataCount(&nWriters), hr);
        DWORD dwAppTobeAdded = true;

        for (unsigned int iWriter = 0; iWriter < nWriters; iWriter++)
        {
            CComPtr<IVssExamineWriterMetadataEx>    pInmMetadataEx;
            VSS_ID                                  idInstance = GUID_NULL;
            VssAppInfo_t                            VssAppInfo;

            CHECK_SUCCESS_RETURN_VAL(pvbEx->GetWriterMetadataEx(iWriter, &idInstance, &pInmMetadataEx), hr);

            CHECK_SUCCESS_RETURN_VAL(FillInVssAppInfoEx(pInmMetadataEx, VssAppInfo,dwAppTobeAdded), hr);
            if(dwAppTobeAdded)
            {
                DiscoverExcludedWriters(VssAppInfo);
                VssWriters.push_back(VssAppInfo);
            }
        }
    } while(false);

    XDebugPrintf("EXITED: InMageVssRequestor::DiscoverWritersTobeEnabled\n");
    return hr;
}

//Get UUID of MS Software Shadow copy provider or thirdparty provider.
//default is MS Software Shadow copy provider.
bool InMageVssRequestor::GetProviderUUID()
{
    RPC_STATUS status = NULL;
    if(m_ACSRequest.bProvider)
    {

        //if ProvierID passed as parameter, it must have exact 36 character and must be in correct UUID string format
        //if it is less or more or not in the exact format, UuidFromString() will fail. When it fails VACP should stop
        //proceeding
        status = UuidFromString((unsigned char*)(m_ACSRequest.strProviderID.c_str()),(UUID*)&uuidProviderID);
        if (status == RPC_S_INVALID_STRING_UUID)
        {
            inm_printf("\n==============================================\n");
            inm_printf("ERROR:[%s] is an invalid custom provider ID.\n", m_ACSRequest.strProviderID.c_str());
            inm_printf("==============================================\n");
            return false;
        }
        else
        {
            if(0 == stricmp(m_ACSRequest.strProviderID.c_str(),gstrInMageVssProviderGuid.c_str()))
            {
                inm_printf("\nUsing InMageVssProvider: %s\n", m_ACSRequest.strProviderID.c_str());
            }
            else
            {
                inm_printf("\nUsing VSS(custom) provider: %s\n", m_ACSRequest.strProviderID.c_str());
            }
            g_ProviderGuid = uuidProviderID;
            return true;
        }
    }
    else
    {
        if(gbUseInMageVssProvider)
        {
            status = UuidFromString((unsigned char*)gstrInMageVssProviderGuid.c_str(),(UUID*)&uuidProviderID);
            inm_printf("\nUsing InMageVSSProvider: %s\n", gstrInMageVssProviderGuid.c_str());
        }
        else
        {
            status = UuidFromString((unsigned char*)MS_SOFTWARE_SHADOWCOPY_PROVIDER,(UUID*)&uuidProviderID);
            inm_printf("\nUsing MS Software Shadow Copy provider: %s\n", MS_SOFTWARE_SHADOWCOPY_PROVIDER);
        }
        //status = UuidFromString((unsigned char*)MS_SOFTWARE_SHADOWCOPY_PROVIDER,(UUID*)&uuidProviderID);
        //status = UuidFromString((unsigned char*)gstrInMageVssProviderGuid.c_str(),(UUID*)&uuidProviderID);
        //inm_printf("\nUsing MS Software Shadow Copy provider: %s\n", MS_SOFTWARE_SHADOWCOPY_PROVIDER);
        //inm_printf("\nUsing VSS Provider: %s\n", gstrInMageVssProviderGuid.c_str());
        return true;
    }
}



std::string MapVssErrorCode2String(const HRESULT vssErrorCode)
{
  auto iter = gVssErrorString.find(vssErrorCode);
  if (iter != gVssErrorString.end())
      return iter->second;
  
  return std::string();
}


HRESULT InMageVssRequestor::ExposeShadowCopiesLocally(string strShadowCopyMountVolume)
{   
    int nShadowCopyTrackId = 0;
    char buf[MAX_PATH] = {0};
    USES_CONVERSION;
    XDebugPrintf("ENTERED: InMageVssRequestor::ExposeShadowCopiesLocally\n");
    HRESULT hr = S_OK;
    std::string errmsg;
    do
    {
        if(false == SnapshotSucceeded)
        {
            hr = S_OK;
            if(pVssBackupComponents)
            {
                if(snapShotInProgress)
                {
                    (void)pVssBackupComponents->AbortBackup();
                }
                else
                {
                    (void)NotifyWritersOnBackupComplete(false);
                    hr = pVssBackupComponents->BackupComplete(&pAsync);
                    if(hr != S_OK)
                    {
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        errmsg = "FAILED: IVssBackupComponents::BackupComplete().";
                        DebugPrintf("%s failed. hr = 0x%08x \n.", hr);
                        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
                        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                        break;
                    }
                    hr = WaitAndCheckForAsyncOperation(pAsync);
                    if(hr != S_OK)
                    {
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        DebugPrintf("FAILED: LoopWait() failed. hr = 0x%08x \n.", hr);
                        break;
                    }
                    hr = CheckWriterStatus(L"After BackupComplete [NOTIFIED FAILED BACKUP]");
                    if(FAILED(hr))
                    {
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        errmsg = "FAILED: CheckWriterStatus().";
                        DebugPrintf("%s failed. hr = 0x%08x \n.", hr);
                        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
                        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                        break;
                    }
                }
            }
            break;
        }
        if(pVssBackupComponents)
        {        
            //Expose each of the Snapshots which are added to the SnapshotSet
            inm_printf("\n\n\t E X P O S I N G S N A P S H O T \n\n");
            LPWSTR wszExposed;
            LPWSTR pwszExposedSnapshotVol;            
            vector<VSS_ID>::iterator snapshotIterator = m_vSnapshotIds.begin();            
            vector<VSS_ID>::iterator snapshotIteratorEnd = m_vSnapshotIds.end();
            while(snapshotIterator != snapshotIteratorEnd)
            {                
                //First Create a New Folder to use it to expose the snapshot/shadow copy.                
                nShadowCopyTrackId += 1;
                inm_printf("\nExposing the Snapshot %d in the Snapshot Set",nShadowCopyTrackId);
                memset((void*)buf,0x00,sizeof(buf));
                itoa(nShadowCopyTrackId,buf,10);    
                string strShadowCopyName = buf;
                string strNewShadowCopyMountVolume = strShadowCopyMountVolume + "\\" + strShadowCopyName;
                wszExposed = (VSS_PWSZ)(A2W(strNewShadowCopyMountVolume.c_str()));
                if(ENOENT == mkdir(strNewShadowCopyMountVolume.c_str()))
                {                    
                    DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                    inm_printf("\n Error in creating a directory %s to expose the snapshot",strShadowCopyMountVolume.c_str());
                    break;
                }
                else
                {
                    //expose the snapshot
                    hr = pVssBackupComponents->ExposeSnapshot((VSS_ID)*snapshotIterator,
                                    NULL,
                                    VSS_VOLSNAP_ATTR_EXPOSED_LOCALLY,
                                    wszExposed,
                                    &pwszExposedSnapshotVol
                                    );
                    if(hr != S_OK)
                    {
                        DWORD dwErr = GetLastError();
                        inm_printf("\n GetLastError = %d",dwErr);
                        DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
                        errmsg = "FAILED: IVssBackupComponents::ExposeShadowCopiesLocally().";
                        DebugPrintf("%s failed. hr = 0x%08x \n.", hr);
                        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
                        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                        break;
                    }
                    unsigned char *pszSnapshotId;
                    UuidToString(&pSnapshotid, &pszSnapshotId);//Convert GUIDs to rpc string                
                    inm_printf("\nSuccessfully Exposed and Mounted Snapshot %d with GUID %s\nat %s \n",nShadowCopyTrackId,pszSnapshotId,strShadowCopyMountVolume.c_str());
                    RpcStringFree(&pszSnapshotId);
                    snapshotIterator++;
                }
            }            
        }
    }while(false);

    SnapshotSucceeded = false;
    snapShotInProgress = false;
    if(pVssBackupComponents)
    {
        pVssBackupComponents.Release();
        pVssBackupComponents = NULL;
    }

    if(pVssBackupComponentsEx)
    {
        pVssBackupComponentsEx.Release();
        pVssBackupComponentsEx = NULL;
    }

    if(pAsync)
    {
        pAsync.Release();
        pAsync = NULL;
    }
    
    XDebugPrintf("EXITED: InMageVssRequestor::ExposeShadowCopiesLocally\n");
    return hr;
}

HRESULT InMageVssRequestor::ProcessDeleteSnapshotSets()
{    
    XDebugPrintf("ENTERED: InMageVssRequestor::ProcessDeleteSnapshotSets\n");
    inm_printf("\n Deleting Persistent Snapshot Sets...\n\n");
    //First get the list of available Snapshot Sets available in the system.
    HRESULT hr = S_OK;
    CComPtr<IVssEnumObject> pEnumSnapshotSets;

    do
    {
        CHECK_SUCCESS_RETURN_VAL(pVssBackupComponents->Query(GUID_NULL,
                                 VSS_OBJECT_NONE,
                                 VSS_OBJECT_SNAPSHOT,
                                 (IVssEnumObject**)&pEnumSnapshotSets),hr);

        VSS_OBJECT_PROP  VssObjProp;
        ULONG fetchedCount;
        if(pEnumSnapshotSets.p != NULL)
        {
            while((pEnumSnapshotSets.p)->Next(1,&VssObjProp,&fetchedCount) == S_OK )
            {    
                gvSnapshotSets.push_back(VssObjProp.Obj.Snap.m_SnapshotSetId);                
            }
            //Free the VSS_SNAPSHOT_PROP structure
            VssFreeSnapshotProperties(&VssObjProp.Obj.Snap);
        }
        else
        {
            hr = -1;
            std::stringstream ss;
            ss << "FILE " << __FILE__ << ": LINE " << __LINE__ << ", There are no Persistent Application Snapshots available in the system to delete!";
            DebugPrintf("%s\n");
            AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            return hr;
        }
        //Then delete each Snapshot Set available in the system one by one.    
        vector<VSS_ID>::iterator iter = gvSnapshotSets.begin();
        while(iter != gvSnapshotSets.end())
        {
            hr = DeleteSnapshotSets(*iter);
            iter++;
        }
    }while(FALSE);
    XDebugPrintf("EXITED: InMageVssRequestor::ProcessDeleteSnapshotSets\n");
    return hr;
}

HRESULT InMageVssRequestor::DeleteSnapshotSets(VSS_ID snapshotSetId)
{
    HRESULT hr = S_OK;    
    LONG TotalSnapshotsDeleted = 0;
    VSS_ID SnapshotsNotDeleted[64/*MAX_SIMULTANEOUS_SNAPSHOTS_POSSIBLE*/] = {0};
    do
    {
        hr = pVssBackupComponents->DeleteSnapshots(snapshotSetId, VSS_OBJECT_SNAPSHOT_SET, true, &TotalSnapshotsDeleted, SnapshotsNotDeleted);
        if(hr != S_OK)
        {
            DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf("FAILED: IVssBackupComponents::DeleteSnapshots() failed. hr = 0x%08x \n.",hr);
            break;
        }
        unsigned char *pszSnapshotId;
        UuidToString(&snapshotSetId, &pszSnapshotId);//Convert GUIDs to rpc string        
        inm_printf("Succefully deleted snapshot set %s\n",pszSnapshotId);
        RpcStringFree(&pszSnapshotId);
    }while(FALSE);
    return hr;
}