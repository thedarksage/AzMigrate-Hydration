#ifndef _INMAGE_VACP_COMMON_H
#define _INMAGE_VACP_COMMON_H

#pragma once 

#include "stdafx.h"
#include <map>
#include "util.h"

#include "..\\config\\vacp_tag_info.h"
#include "VacpCommon.h"

typedef struct _VOLMOUNTPOINT
{
    std::string strVolumeName;
    std::string strVolumeMountPoint;
}VOLMOUNTPOINT;

typedef struct _VOLMOUNTPOINT_INFO
{
    DWORD			dwCountVolMountPoint;
    std::vector<VOLMOUNTPOINT> vVolumeMountPoints;

}VOLMOUNTPOINT_INFO;

typedef struct _ComponentSummary
{
    _ComponentSummary()
    {
        volumeBitMask = 0;
        IncludedForSnapshot = false;
        ComponentType = 2; // 0 is Undefined, 1 is DataBase, 2 File Group
        volMountPoint_info.dwCountVolMountPoint = 0;
        bIsSelectable = false;
    }

    std::string ComponentName;
    std::string ComponentCaption;
    std::string ComponentLogicalPath;
    std::string UserPreferredName;
    int ComponentType;
    std::vector<std::string>  FileGroupFiles;
    std::vector<std::string>  DatabaseFiles;
    std::vector<std::string>  DataBaseLogFiles;
    std::vector <std::string> ComponentVolumes;
    DWORD volumeBitMask;
    bool IncludedForSnapshot;
    bool bIsSelectable;
    std::string fullPath;
    VOLMOUNTPOINT_INFO volMountPoint_info;

}ComponentSummary_t;

typedef struct _AppSummary
{
    _AppSummary()
    {
        volumeBitMask = 0;
        includeAllComponents = false;
        volMountPoint_info.dwCountVolMountPoint = 0;
        totalVolumes = 0;
    }

    _AppSummary(const char *Name,const char *InstanceName, DWORD volBitMask, bool includeAll = true)
        :m_appName(Name),
        m_writerInstanceName(InstanceName),
        volumeBitMask(volBitMask),
        includeAllComponents(includeAll)
    {
        volMountPoint_info.dwCountVolMountPoint = 0;
        totalVolumes = 0;
    }

    _AppSummary(VOLMOUNTPOINT_INFO volMP_info, const char *Name,const char *InstanceName, DWORD volBitMask, bool includeAll = true)
        : m_appName(Name),
        m_writerInstanceName(InstanceName),
        volumeBitMask(volBitMask),
        includeAllComponents(includeAll)
    {
        volMountPoint_info.dwCountVolMountPoint = volMP_info.dwCountVolMountPoint;
        for(unsigned int i = 0; i < volMP_info.dwCountVolMountPoint; i++)
        {
            volMountPoint_info.vVolumeMountPoints.push_back(volMP_info.vVolumeMountPoints[i]);
            totalVolumes++;
        }
    }

    std::string m_appName;
    std::string m_writerInstanceName;

    std::vector<ComponentSummary_t> m_vComponents;
    DWORD volumeBitMask;
    bool includeAllComponents;
    VOLMOUNTPOINT_INFO volMountPoint_info;
    bool bHasValidComponent;
    unsigned int totalVolumes;

}AppSummary_t;

typedef struct _AppParams
{
    _AppParams(){}

    _AppParams(const char *aName)
    :m_applicationName(aName)
    {
    }

    _AppParams(const char *aName,const char *instanceName)
        :m_applicationName(aName),
        m_writerInstanceName(instanceName)
    {
    }

    std::string m_applicationName;
    std::string m_writerInstanceName;
    std::vector<std::string> m_vComponentNames;

}AppParams_t;

// a struct to hold the input parameters to the VACP runtime
typedef struct _CLIRequest
{
    /// \brief the applications that are specified in the command line
    /// not used with the systemlevel tags options
    vector<AppParams_t> m_applications;

    std::set<std::string> Volumes;

    vector<const char *> VolumeGuids;
    vector<const char *> BookmarkEvents;
    vector<const char *> vDeleteSnapshotIds;
    vector<const char *> vDeleteSnapshotSetIds;
    string              strHydrationInfo;
    bool DoNotIncludeWriters;
    bool bFullBackup;
    bool bPrintAppList;
    bool bSkipChkDriverMode;
    bool bSkipUnProtected;
    bool bWriterInstance;
    bool bApplication;
    bool bTag;
    bool bDoNotIssueTag;
    DWORD dwTagTimeout;
    bool bTagTimeout;
    bool bSyncTag;
    TAG_TYPE TagType;

    bool bProvider;
    string strProviderID;
    vector<const char *> vServerDeviceID;
    string strServerIP;
    bool bServerDeviceID;
    bool bServerIP;
    unsigned short usServerPort;
    bool bRemote;
    bool bVerify;
    bool bPersist;
    bool bDelete;
    //Configuration Server
    bool bCSIP;//IP Address of the Configuration Server (Cx Server)used to query the license information
    string strCSServerIP;
    unsigned short usCSServerPort;

    //life time related
    bool bTagLifeTime;
    bool bTagLTForEver;
    bool bIgnoreNonDataMode;
    bool bEnumSW;//Enumerate System Writer

    unsigned long long ulMins;
    unsigned long long ulHours;
    unsigned long long ulDays;
    unsigned long long ulWeeks;
    unsigned long long ulMonths;
    unsigned long long ulYears;

    bool bDeleteAllVacpPersitedSnapshots;
    bool bDeleteSnapshotIds;
    bool bDeleteSnapshotSets;
    bool bDeleteSnapshots;

    bool bDistributed;
    bool bMasterNode;
    bool bCoordNodes;
    bool bSharedDiskClusterContext;

    unsigned short usDistributedPort;
    std::string strMasterHostName;
    std::vector<std::string> vCoordinatorNodes;

    std::set<std::string> vDisks;
    std::set<std::wstring> vDiskGuids;
    std::map<std::wstring, std::string> m_diskGuidMap;

    GUID    distributedTagGuid;
    std::string distributedTagUniqueId;
    std::vector<const char *> vDistributedBookmarkEvents;
    
    GUID    localTagGuid;

    std::string strRcmSettingsPath;
    std::string strProxySettingsPath;
    std::string strMasterNodeFingerprint;

    unsigned long long        m_lastAppScheduleTime;
    unsigned long long        m_lastCrashScheduleTime;
    std::string               m_strScheduleFilePath;
//#endif

}CLIRequest_t;

//class ACSTagIdentifier;

typedef struct _ACSRequest
{
    _ACSRequest()
    {
        TagType = TAG_TYPE::INVALID;
        RawVolumeBitMask = 0;
        volumeBitMask = 0;
        volMountPoint_info.dwCountVolMountPoint = 0;
        tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint = 0;
        RevocationTagLength = 0;
        RevocationTagDataPtr = NULL;	

        distributedTagGuid = GUID_NULL;
    }

    void DeleteTags()
    {
        for (unsigned int tagIndex = 0; tagIndex < TagLength.size(); tagIndex++)
        {
            if (TagLength[tagIndex] && TagData[tagIndex])
            {
                TagLength[tagIndex] = 0;
                delete[] TagData[tagIndex];
                TagData[tagIndex] = NULL;
            }
        }

        if (RevocationTagLength && RevocationTagDataPtr)
        {
            RevocationTagLength = 0;
            delete[] RevocationTagDataPtr;
            RevocationTagDataPtr = NULL;
        }

        TagLength.clear();
        TagData.clear();
        TagIdentifier.clear();

    }

    void DeleteRevocationTags()
    {
        for (unsigned int tagIndex = 0; tagIndex < vRevocTagLengths.size(); tagIndex++)
        {
            if (vRevocTagLengths[tagIndex] && vRevocTagsData[tagIndex])
            {
                vRevocTagLengths[tagIndex] = 0;
                delete[] vRevocTagsData[tagIndex];
                vRevocTagsData[tagIndex] = NULL;
            }				
        }

        //this block is commented as this cleanup is done in the DeleteTags() function itself
        /*if (RevocationTagLength && RevocationTagDataPtr)
        {
            RevocationTagLength = 0;
            delete[] RevocationTagDataPtr;
            RevocationTagDataPtr = NULL;
        }*/

        vRevocTagLengths.clear();
        vRevocTagsData.clear();
        vRevocTagIdentifiers.clear();
    }
  
    void Reset()
    {
        DeleteTags();
        DeleteRevocationTags();
        Applications.clear();
        TagIdentifier.clear();
        TagLength.clear();
        TagData.clear();
        vVolumes.clear();
        vProtectedVolumes.clear();
        vApplyTagToTheseVolumes.clear();
        vDeleteSnapshotIds.clear();
        vDeleteSnapshotSetIds.clear();

        RawVolumeBitMask = 0;
        volumeBitMask = 0;

        bSkipChkDriverMode = false;
        bSkipUnProtected = false;

        volMountPoint_info.dwCountVolMountPoint = 0;
        volMountPoint_info.vVolumeMountPoints.clear();

        tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint = 0;
        tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints.clear();

        bWriterInstance =  false;
        RevocationTagLength = 0;
        RevocationTagDataPtr = NULL;
        strHydrationInfo.clear();
        bTag = false;
        bDoNotIssueTag = false;
        DoNotIncludeWriters = false;
        bProvider = false;
        strProviderID.clear();
        bSyncTag = false;
        bTagTimeout = false;
        bVerify = false;
        dwTagTimeout = 0;
        bDelete = false;
        bPersist = false;
        TagType = TAG_TYPE::INVALID;
        

        vServerDeviceID.clear();
        TagName.clear();
        strServerIP.clear();
        usServerPort = 0;

        vacpServerCmdArg.bRevocationTag = false;
        vacpServerCmdArg.tagFlag = 0;
        vacpServerCmdArg.vDeviceId.clear();
        vacpServerCmdArg.vTagIdAndData.clear();
        vRevocTagIdentifiers.clear();
        vRevocTagLengths.clear();
        vRevocTagsData.clear();
        
        bTagLifeTime = false;
        bTagLTForEver = false;
        bIgnoreNonDataMode = false;
        bEnumSW = false;
        ulMins = 0;
        ulHours = 0;
        ulDays = 0;
        ulWeeks = 0;
        ulMonths = 0;
        ulYears = 0;

        bDeleteAllVacpPersitedSnapshots = false;
        bDeleteSnapshotIds = false;
        bDeleteSnapshotSets = false;

        bDistributed = false;
        bMasterNode = false;
        bCoordNodes = false;

        usDistributedPort = 0;
        strMasterHostName.clear();

        vCoordinatorNodes.clear();
        vDisks.clear();
        vProtectedDisks.clear();
        vDiskGuids.clear();
        m_diskGuidMap.clear();

        distributedTagGuid = GUID_NULL;
        distributedTagUniqueId.clear();
        vBookmarkEvents.clear();
        vDistributedBookmarkEvents.clear();

        localTagGuid = GUID_NULL;

    }

    bool GetDiskGuidFromDiskName(const std::string& diskName, std::wstring& diskSignature)
    {
        bool bRet = false;
        std::map<wstring, string>::iterator diskIter = m_diskGuidMap.begin();
        for (; diskIter != m_diskGuidMap.end(); diskIter++)
        {
            if (diskName.compare(diskIter->second) == 0)
            {
                diskSignature = diskIter->first;
                bRet = true;
                break;
            }
        }

        return bRet;
    }


    vector<AppSummary_t> Applications;
    vector<USHORT> TagIdentifier;
    vector<USHORT> TagLength;
    vector<void *> TagData;
    vector<std::string> vVolumes;
    vector<std::string> vProtectedVolumes;
    vector<std::string> vApplyTagToTheseVolumes;
    vector<std::string> vDeleteSnapshotIds;
    vector<std::string> vDeleteSnapshotSetIds;
    DWORD RawVolumeBitMask;
    DWORD volumeBitMask;
    bool bSkipChkDriverMode;
    bool bSkipUnProtected;
    VOLMOUNTPOINT_INFO volMountPoint_info;
    VOLMOUNTPOINT_INFO tApplyTagToTheseVolumeMountPoints_info;
    bool bWriterInstance;
    USHORT RevocationTagLength;
    void *RevocationTagDataPtr;
    string              strHydrationInfo;
    bool bTag;
    bool bDoNotIssueTag;
    bool DoNotIncludeWriters;
    bool bProvider;
    string strProviderID;
    bool bSyncTag;
    bool bTagTimeout;
    bool bVerify;
    DWORD dwTagTimeout;
    bool bDelete;
    bool bPersist;
    TAG_TYPE TagType;

    vector<std::string> vServerDeviceID;
    vector<std::string> TagName;
    string strServerIP;
    unsigned short usServerPort;

    //This is used when VACP client and VACP Server talked through Configurator. 
    VACP_SERVER_CMD_ARG vacpServerCmdArg; // 

    vector<USHORT> vRevocTagIdentifiers;
    vector<USHORT> vRevocTagLengths;
    vector<void *> vRevocTagsData;

    //life time related
    bool bTagLifeTime;
    bool bTagLTForEver;
    bool bIgnoreNonDataMode;
    bool bEnumSW;
    unsigned long long ulMins;
    unsigned long long ulHours;
    unsigned long long ulDays;
    unsigned long long ulWeeks;
    unsigned long long ulMonths;
    unsigned long long ulYears;

    bool bDeleteAllVacpPersitedSnapshots;
    bool bDeleteSnapshotIds;
    bool bDeleteSnapshotSets;

    bool bDistributed;
    bool bMasterNode;
    bool bCoordNodes;
    unsigned short usDistributedPort;
    std::string strMasterHostName;
    std::vector<std::string> vCoordinatorNodes;

    std::set<std::string> vDisks;
    std::set<std::string> vProtectedDisks;
    std::set<std::wstring> vDiskGuids;
    std::map<std::wstring, std::string> m_diskGuidMap;


    GUID    distributedTagGuid;
    std::string distributedTagUniqueId;
    std::vector<const char *> vBookmarkEvents;
    std::vector<const char *> vDistributedBookmarkEvents;

    GUID    localTagGuid;

    std::string m_contextGuid;
    std::string m_hydrationTag;
    std::string m_clusterTag;
}ACSRequest_t;

#endif /* _INMAGE_VACP_COMMON_H */

