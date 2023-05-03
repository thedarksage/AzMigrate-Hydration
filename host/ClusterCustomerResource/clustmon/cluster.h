// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : cluster.h
//
// Description:
//

#ifndef CLUSTER_H
#define CLUSTER_H

#include <windows.h>
#include <clusapi.h>
#include <resapi.h>
#include <ctype.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <set>

#include "volumegroupsettings.h"

#define MAX_CHARS                         256
#define MAX_DRIVES_HEX_ENCODED              9
#define MAX_GUID                           56
#define AVAILABLE_DISK_SIZE              1024
#define SIGNATURE_OFFSET_LENGTH            12
#define DISK_SIGNATURE_LENGTH               4
#define DISK_OFFSET_LENGTH                  8
#define DISK_LENGTH_LENGTH                  8
#define DISK_NUMBER_LENGTH                  4
#define CLUSTERDISK_GUID_LENGTH            16
#define MAX_SIGNATURE_OFFSET_LENGTH        (CLUSTERDISK_GUID_LENGTH * 2)
#define VERITAS_DISK_OFFSET_LENGTH         (DISK_OFFSET_LENGTH + DISK_LENGTH_LENGTH + DISK_NUMBER_LENGTH)

typedef unsigned char byte;
typedef std::vector<WCHAR> WCHAR_VECTOR;
typedef std::vector<byte> BYTE_VECTOR;

// ----------------------------------------------------------------------------
// Mounted volumes support
// ----------------------------------------------------------------------------
struct MountVolumeInfoHeader {
    byte m_Signature[DISK_SIGNATURE_LENGTH];

    unsigned int m_VolumeCount;
};

struct MountVolumeInfo {
    byte m_StartOffset[DISK_OFFSET_LENGTH];
    byte m_Length[DISK_LENGTH_LENGTH];
    byte m_Number[DISK_NUMBER_LENGTH];
    byte m_Type;
    byte m_DriveLetter;
    byte m_Padding[2];
};

typedef std::map<std::wstring, std::wstring> SignatureOffsetMountPoints_t;

// ----------------------------------------------------------------------------
// Windows 2008 cluster disk information
// ----------------------------------------------------------------------------

/* TODO: msdn mentions that for 32 bit windows, volume count is only 4 bytes and there is no padding.
 *       But verified on a test setup, and found that it is still 8 bytes. Hence keeping 8 bytes for now,
 *       but we will have to revisit this and as per some posts, accessing disk volume info structure is 
 *       not correct and we should use CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO */
struct DiskVolumeInfoHeader {
    unsigned long long m_VolumeCount;
};

struct DiskVolumeInfo {
    byte m_StartOffset[DISK_OFFSET_LENGTH];
    byte m_Length[DISK_LENGTH_LENGTH];
    byte m_Number[DISK_NUMBER_LENGTH];

    unsigned int m_Drive;
    byte m_Guid[CLUSTERDISK_GUID_LENGTH];
};

// ----------------------------------------------------------------------------
// Forward declerations
// ----------------------------------------------------------------------------
class ClusterTrackingInfo;
class GroupTrackingInfo;

// ----------------------------------------------------------------------------
// ResourceTrackingInfo
// ----------------------------------------------------------------------------
class ResourceTrackingInfo {
public:
    typedef std::map<std::wstring, std::wstring> Dependencies_t;

    ResourceTrackingInfo()
        : m_hResource(0)
        { }

    std::wstring const & Id() const {
        return m_Id;
    }

    std::wstring const & Name() const {
        return m_Name;
    }

    bool Tracked() const {
        return (!m_Volumes.empty() || IsNetworkName() || (!IsPhysicalDisk() && !m_Dependencies.empty()));
    }

    bool IsVeritasVolumeManagerDiskGroup();
    bool IsPhysicalDisk() const;
    bool IsSqlServer() const;
    bool IsExchangeSystemAttendant() const;
    bool IsNetworkName() const;

    bool DependenciesEmpty() {
        return m_Dependencies.empty();
    }

    bool operator==(ResourceTrackingInfo const & rhs) const;

    bool SetNameIdType(WCHAR const * name);
    bool BuildInfo(WCHAR const * name);
    bool GetMountPointSignatureOffsetForVolumes(HKEY hKey, GroupTrackingInfo* group);
    bool ResourceAdded(ClusterInfo & clusterInfo, ClusterGroupInfo & group);
    bool ResourceNameChanged(ClusterInfo & clusterInfo, ClusterGroupInfo & group, ResourceTrackingInfo const & resource);
    bool ResourceDependencyChanged(ClusterInfo & clusterInfo, ClusterGroupInfo & group, ResourceTrackingInfo const & resource, ResourceTrackingInfo::Dependencies_t & deletedDependencies, bool & dependenciesAdded);
    bool UpdateResourceDependencyName(ResourceTrackingInfo const & resource);
    bool Save(HKEY hKey);
    bool Delete(HKEY hKey);
    bool Load(HKEY hKey, std::wstring id);
    bool DeleteResourceDependency(HKEY hKey, ResourceTrackingInfo::Dependencies_t const & dependencies);
    bool HasVolume(std::wstring const &volumeName) const;
    bool AddMountPointInfo(std::wstring const & signatureOffset, std::wstring const & mountPoint);
    bool GetVirtualServerName(std::wstring & name);
    bool GoingOffline() const;
    bool Online() const;
    bool MissingVolumeNames() const;

    void GetInfoForCx(ClusterResourceInfo & info);
    void UpdateMountPointsUnderWrongGroup();
    void SetIdName(std::wstring const & id, std::wstring const & name) {
        m_Id = id;
        m_Name = name;
    }

protected:
    typedef std::map<std::wstring, std::wstring> Volumes_t;
    typedef std::vector<WCHAR> signatureOffset_t;


    template <class Container> bool SaveData(HKEY hKey, Container data);
    template <class Container> bool DeleteData(HKEY hKey, Container data);
    template <class Container> bool LoadData(HKEY hKey, std::wstring id, Container & data);

    bool SaveVolumes(HKEY hKey);
    bool SaveDependencies(HKEY hKey);
    bool DeleteVolumes(HKEY hKey);
    bool DeleteDependencies(HKEY hKey);
    bool LoadVolumes(HKEY hKey);
    bool LoadDependencies(HKEY hKey);
    bool ResourceControl(WCHAR_VECTOR& data, DWORD & size, DWORD request);
    bool GetDiskInfo();
    bool GetVeritasCluterVolumeInfo();
    bool AddVeritasClusterInfo(std::string const& dskGroupInfo);
    bool GetVeritaslDiskGroupInfo(std::string& dskGroupInfo);
    std::string GetVeritasVxdgCmd();
    bool GetDiskVolumeInfo(bool& needToCollectVeritasVolumes);
    bool GetMountVolumeInfo(bool& needToCollectVeritasVolumes);
    bool GetDependsOn();
    bool GetMountPointSignatureOffset(HKEY hKey, std::wstring const & mountPoint, GroupTrackingInfo* group);
    bool SetId();
    bool GetDiskSignatureForGuid(WCHAR const * guid, signatureOffset_t& signatureOffset);
    bool AddMountPointInfoUsingGuid(std::wstring const & guid, std::wstring const & mountPoint);

    void SetResourceType();
    void FormatGuid(std::wstring const& guid, std::wstring& formattedGuid);
    void DumpSignatureOffset(char* type, signatureOffset_t& signatureOffset);

private:
    HRESOURCE m_hResource;

    WCHAR_VECTOR m_ResourceType;

    std::wstring m_Name;
    std::wstring m_Id;

    Volumes_t m_Volumes;

    Dependencies_t m_Dependencies;
};

// ----------------------------------------------------------------------------
// GroupTrackingInfo
// ----------------------------------------------------------------------------
class GroupTrackingInfo {
public:
    typedef std::map<std::wstring, ResourceTrackingInfo> ResourceTrackingInfos_t;
    typedef std::pair<bool, std::wstring> FindResourceResult_t;

    GroupTrackingInfo()
        : m_hGroup(0),
          m_SqlVirtualServer(false),
          m_ExchangeVirtualServer(false)
        {}

    GroupTrackingInfo(WCHAR const * groupName)
        : m_hGroup(0),
          m_Name(groupName),
          m_SqlVirtualServer(false),
          m_ExchangeVirtualServer(false)
        {}

    std::wstring const & Id() const {
        return m_Id;
    }

    std::wstring const & Name() const {
        return m_Name;
    }

    FindResourceResult_t FindResourceNameForVolume(std::wstring const & volume) const;

    bool SetNameId(WCHAR const * name);
    bool BuildInfo();
    bool BuildInfo(WCHAR const * name);
    bool UpdateName(std::wstring const & name) {
        m_Name = name;
    }
    bool Track() {
        return (!m_Resources.empty());
    }

    bool operator<(GroupTrackingInfo const & rhs) const {
        return m_Id < rhs.m_Id;
    }

    bool ResourcesEmpty() {
        return m_Resources.empty();
    }

    void UpdateName(WCHAR const * name) {
        m_Name = name;
    }

    void SetIdName(std::wstring const & id, std::wstring const & name) {
        m_Id = id;
        m_Name = name;
    }

    ResourceTrackingInfos_t const & Resources() {
        return m_Resources;
    }

    bool GroupResourceAdded(GroupTrackingInfo & group) const {
        return (m_Resources.size() < group.m_Resources.size());
    }

    bool GroupTrackingInfo::GroupResourceExists(std::wstring const & resourceId) const {
        return (m_Resources.end() != m_Resources.find(resourceId));
    }


    bool ResourceAdded(ClusterInfo & clusterInfo, ResourceTrackingInfo const & resource);
    bool ResourceDeleted(ClusterInfo & clusterInfo, std::wstring const & name);
    bool ResourceNameChanged(ClusterInfo & clusterInfo, ResourceTrackingInfo const & resource);
    bool ResourceDependencyChanged(ClusterInfo & clusterInfo, ResourceTrackingInfo const & resource, ResourceTrackingInfo::Dependencies_t & deletedDependencies, bool & dependenciesAdded);
    bool GroupNameChanged(ClusterInfo & clusterInfo, std::wstring const & name);
    bool NetworkNameChanged(ResourceTrackingInfo const & resource);
    bool VirtualServerNameChanged(ClusterInfo & clusterInfo, ResourceTrackingInfo & resource);
    bool Save(HKEY hKey);
    bool Delete(HKEY hKey);
    bool DeleteResource(HKEY hKey, std::wstring const & name);
    bool DeleteResourceDependency(HKEY hKey, ResourceTrackingInfo resource, ResourceTrackingInfo::Dependencies_t const & dependencies);
    bool Load(HKEY hKey, std::wstring id);
    bool AddMountPointInfo(std::wstring const & signatureOffset, std::wstring const & mountPoint);
    bool MissingVolumeNames() const;

    void GetInfoForCx(ClusterGroupInfo & info);
    void UpdateMountPointsUnderWrongGroup();

protected:
    bool SetId();
    bool GetResources();
    bool GetResourceDiskDependencies(ResourceTrackingInfo const & dependee);
    bool GetMountPointSignatureOffset();
    bool CreateKeys();
    bool SaveVirtualServerInfo(HKEY hKey);

private:
    HGROUP m_hGroup;

    std::wstring m_Name;
    std::wstring m_Id;

    bool m_SqlVirtualServer;
    bool m_ExchangeVirtualServer;

    std::wstring m_NetworkName;
    std::wstring m_NetworkNameId;
    std::wstring m_VirtualServerName;

    ResourceTrackingInfos_t m_Resources;
};

// ----------------------------------------------------------------------------
// ClusterTrackingInfo
// ----------------------------------------------------------------------------
class ClusterTrackingInfo {
public:
    ClusterTrackingInfo() : m_Upgrade(false), m_OfflineCallback(0) {}
    ~ClusterTrackingInfo()
	{
            delete m_OfflineCallback;
            m_OfflineCallback = NULL;
        }

    bool BuildClusterTrackingInfo();
    bool Register();
    bool Upgrade() {
        return m_Upgrade;
    }

    bool VolumeAvailableForNode(std::wstring const & vol) const;
    bool IsClusterVolume(std::wstring const & vol) const;
    bool CleanUpIfNeeded();

    void ResourceStateChanged(WCHAR const * name);
    void ResourceAdded(WCHAR const * name);
    void ResourceAdded(ResourceTrackingInfo const & resource);
    void ResourceDeleted(WCHAR const * name);
    void ResourcePropertyChanged(WCHAR const * name);
    void GroupDeleted(WCHAR const * name);
    void GroupPropertyChanged(WCHAR const * name);
    void NodeAdded(WCHAR const * name);
    void NodePropertyChanged(WCHAR const * name);
    void NodeDeleted(WCHAR const * name);

    template <class T>
    void SetOfflineCallback(T& obj, void(T::*callback)()) {
        m_OfflineCallback = new ClusterResourceOfflineCallbackMemFunRef<T>(obj, callback);
    }

    void SetOfflineCallback(void (*callback)()) {
        m_OfflineCallback = new ClusterResourceOfflineCallbackFun(callback);
    }

protected:
    typedef std::vector<std::wstring> DeleteResourceIds_t;
    typedef std::map<std::wstring, GroupTrackingInfo> GroupTrackingInfos_t;
    typedef std::map<std::wstring, std::wstring> ClusterAvailableVolumeTrackingInfos_t;

    // ----------------------------------------------------------------------------
    // support for resource offline callback
    // so user don't have to write there own functors
    // they just use the StartMonitor
    // ----------------------------------------------------------------------------
    class ClusterResourceOfflineCallback {
    public:
        virtual void operator()() = 0;
    };

    // allows member function ref to be used
    template <class T>
    class ClusterResourceOfflineCallbackMemFunRef : public ClusterResourceOfflineCallback {
    public:
        ClusterResourceOfflineCallbackMemFunRef(T& obj, void(T::*callback)()) : m_Object(obj), m_Callback(callback) {}
        virtual void operator()() {
            (m_Object.*m_Callback)();
        }
    private:
        T& m_Object;
        void (T::*m_Callback)();
    };

    // allows stand alone function to be used
    class ClusterResourceOfflineCallbackFun : public ClusterResourceOfflineCallback {
    public:
        ClusterResourceOfflineCallbackFun(void (*callback)()) : m_Callback(callback) {}
        virtual void operator()() {
            (*m_Callback)();
        }
    private:
        void (*m_Callback)();
    };

    void GetInfoForCx(ClusterInfo & info);
    void UpdateResourceInfo(ResourceTrackingInfo const & resource);
    void ParseAvailableDisks(BYTE_VECTOR  & disks, DWORD listSize);
    void RegisterMovedResources(ClusterInfo & trackedInfo, ClusterInfo & clusterInfo);
    void RegisterDeletedGroups(ClusterInfo & trackedInfo, ClusterInfo const & clusterInfo);
    void RegisterDeletedResources(ClusterInfo & trackedInfo, ClusterInfo const & clusterInfo);
    void RegisterDeletedAvailableVolumes(ClusterTrackingInfo const & trackedInfo);
    void RegisterDeletedResourceDependencies(ClusterInfo & trackedInfo, ClusterInfo const & clusterInfo);
    void RegisterIfNeeded();
    void UpdateMountPointsUnderWrongGroup();

    bool GetClusterAvailableVolumes();
    bool GetClusterAvailableVolumes(LPCWSTR resourceType);
    bool GetGroups();
    bool GetNameId();
    bool ClusterRenamed();
    bool Save();
    bool SaveAvailableVolumes(HKEY hKey);
    bool LoadAvailableVolumes(HKEY hKey);
    bool SaveClusterName();
    bool Load();
    bool DeleteResource(std::wstring const & name);
    bool DeleteGroup(std::wstring const & name);
    bool DeleteGroup(HKEY hKey, std::wstring const & name);
    bool DeleteTrackingInfo(HKEY hKey, std::wstring const & keyName);
    bool DeleteTrackingInfo();
    bool DeleteNode();
    bool DeleteGroupResourceMoved(GroupTrackingInfo & group, DeleteResourceIds_t const & deleteResourceIds);
    bool DeleteResourceDependency(ResourceTrackingInfo const & resource, ResourceTrackingInfo::Dependencies_t const & dependencies);
    bool GroupResourceMoved(ClusterInfo & clusterInfo, GroupTrackingInfo & group);
    bool GroupResourceMoved(ClusterInfo & clusterInfo, GroupTrackingInfo & group, GroupTrackingInfo & changedGroup);
    bool GroupResourceMovedToNonTrackedGroup(ClusterInfo & clusterInfo, GroupTrackingInfo & changedGroup);
    bool VolumeAvailable(std::wstring const & resourceName) const;

private:

    std::wstring m_Name;

    std::wstring m_Id;

    GroupTrackingInfos_t m_Groups;

    ClusterAvailableVolumeTrackingInfos_t m_AvailableVolumes;

    bool m_Upgrade;

    ClusterResourceOfflineCallback* m_OfflineCallback;
};

// ----------------------------------------------------------------------------
// Cluster
// ----------------------------------------------------------------------------
class Cluster {
public:
    Cluster();
    ~Cluster() {
        DeleteCriticalSection(&m_CriticalSection);
    }

    static DWORD RegGetValue(HKEY         hKey,
                             WCHAR const* pValueName,
                             WCHAR*&      value,
                             DWORD&       valueType,
                             DWORD&       valueLength);


    bool VolumeAvailableForNode(std::string const & vol);
    bool IsClusterVolume(std::string const & vol);

    template <class T>
    DWORD StartMonitor(T& obj, void(T::*callback)()) {
        m_ClusterTrackingInfo.SetOfflineCallback(obj, callback);
        return StartMonitor();
    }

    DWORD StartMonitor(void (*callback)()) {
        m_ClusterTrackingInfo.SetOfflineCallback(callback);
        return StartMonitor();
    }

    void StopMonitor();

    void CollectAndExit() {
        m_CollectAndExit = true;
    }

    bool Exited() {
        return m_Exited;
    }

protected:

    DWORD StartMonitor();

    bool SetClusterHandle(HCLUSTER & hCluster);

    bool Monitor();

    static unsigned __stdcall MonitorThread(LPVOID pParam);

    bool QuitRequested(DWORD waitSeconds);
    bool WaitForMscs();
    bool HandleClusterNotifications();
    bool VerifyInCDskFl();
    bool VerifyInCDskFlLoaded();
    bool VerifyInCDskFlInstall();

    void RegistryValueChanged(WCHAR const * name);
    void ResourceStateChanged(WCHAR const * name);
    void ResourceAdded(WCHAR const * name);
    void ResourceDeleted(WCHAR const * name);
    void ResourcePropertyChanged(WCHAR const * name);
    void GroupDeleted(WCHAR const * name);
    void GroupPropertyChanged(WCHAR const * name);
    void NodeAdded(WCHAR const * name);
    void NodePropertyChanged(WCHAR const * name);
    void NodeDeleted(WCHAR const * name);

private:
    Cluster(Cluster const & cluster);

    DWORD m_MonitorEvents;

    HANDLE m_MonitorHandle;

    unsigned m_MonitorThreadId;

    HANDLE m_hQuitEvent;

    ClusterTrackingInfo m_ClusterTrackingInfo;

    CRITICAL_SECTION m_CriticalSection;

    bool m_CollectAndExit;
    bool m_Exited;

};

void DumpClusterInfo(char const * type, ClusterInfo const & info);
void RegisterClusterInfo(char * request, ClusterInfo const & info);
void PrintDiskVolumeInfo(DiskVolumeInfo *p);
void PrintDiskVolumeInfoHeader(DiskVolumeInfoHeader *p);

#endif // ifndef CLUSTER__H

