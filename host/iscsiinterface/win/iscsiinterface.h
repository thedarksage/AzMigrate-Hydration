
//
//  File: iscsiinterface.h
//
//  Description:
//

#ifndef ISCSIINTERFACE_H
#define ISCSIINTERFACE_H

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <iscsierr.h>
#include <iscsidsc.h>
#include <string>
#include <set>
#include <vector>
#include <list>
#include <sstream>

#include "volumegroupsettings.h"
int const DEFAULT_BUFFER_SIZE = 1024;

class IScsi {
public:
    typedef std::list<ISCSI_SESSION_INFO*> sessionInfos_t;
    typedef std::list<ISCSI_TARGET_PORTAL_INFO_EX*> targetPortalInfosEx_t;

    IScsi();

    ~IScsi();

    void addPortal(std::string const& initiatorName, unsigned long initiatorInterfacePort, std::string const& portalIpAddress, std::string const& portalPort);
    void removeAllForPortal(std::string const& portalIpAddress, std::string const& portalPort);

    void loginTarget(std::string const& initiatorName, unsigned long initiatorInterfacePort, std::string const& targetName);

   

    void getSessionInfos(sessionInfos_t& sessionInfos);
    void getTargetPortalsEx(targetPortalInfosEx_t& targetPortalInfosEx);

    void getIScsiInitiatorNodeName(std::string& nodeName);

    void bindAllPersistentVolumes();

protected:
    typedef ISDSC_STATUS (WINAPI *AddIScsiSendTargetPortal_t)(PTCHAR InitiatorName,
                                                              ULONG InitiatorPortNumber,
                                                              PISCSI_LOGIN_OPTIONS LoginOptions,
                                                              ISCSI_SECURITY_FLAGS SecurityFlags,
                                                              PISCSI_TARGET_PORTAL Portal);

    typedef ISDSC_STATUS (WINAPI *ClearPersistentIScsiDevices_t)();

    typedef ISDSC_STATUS (WINAPI *GetDevicesForIScsiSession_t)(PISCSI_UNIQUE_SESSION_ID UniqueSessionId,
                                                               PULONG DeviceCountPtr,
                                                               PISCSI_DEVICE_ON_SESSION Devices);

    typedef ISDSC_STATUS (WINAPI *GetIScsiSessionList_t)(ULONG *BufferSize,
                                                         ULONG *SessionCountPtr,
                                                         PISCSI_SESSION_INFO SessionInfo);

    typedef ISDSC_STATUS (WINAPI *GetIScsiTargetInformation_t)(PTCHAR TargetName,
                                                               PTCHAR DiscoveryMechanism,
                                                               TARGET_INFORMATION_CLASS InfoClass,
                                                               PULONG BufferSize,
                                                               PVOID Buffer);

    typedef ISDSC_STATUS (WINAPI *LoginIScsiTarget_t)(PTCHAR TargetName,
                                                      BOOLEAN IsInformationalSession,
                                                      PTCHAR InitiatorName,
                                                      ULONG InitiatorPortNumber,
                                                      PISCSI_TARGET_PORTAL TargetPortal,
                                                      ISCSI_SECURITY_FLAGS SecurityFlags,
                                                      PISCSI_TARGET_MAPPING Mappings,
                                                      PISCSI_LOGIN_OPTIONS LoginOptions,
                                                      ULONG KeySize,
                                                      PCHAR Key,
                                                      BOOLEAN IsPersistent,
                                                      PISCSI_UNIQUE_SESSION_ID UniqueSessionId,
                                                      PISCSI_UNIQUE_CONNECTION_ID UniqueConnectionId);

    typedef ISDSC_STATUS (WINAPI *LogoutIScsiTarget_t)(PISCSI_UNIQUE_SESSION_ID  UniqueSessionId);

    typedef ISDSC_STATUS (WINAPI *RemoveIScsiPersistentTarget_t)(PTCHAR InitiatorName,
                                                                 ULONG InitiatorPortNumber,
                                                                 PTCHAR TargetName,
                                                                 PISCSI_TARGET_PORTAL Portal);

    typedef ISDSC_STATUS (WINAPI *RemoveIScsiSendTargetPortal_t)(PTCHAR HBAName,
                                                                 ULONG InitiatorPortNumber,
                                                                 PISCSI_TARGET_PORTAL Portal);

    typedef ISDSC_STATUS (WINAPI *ReportIScsiTargets_t)(BOOLEAN ForceUpdate,
                                                        PULONG BufferSize,
                                                        PTCHAR Buffer);

    typedef ISDSC_STATUS (WINAPI *SetupPersistentIScsiDevices_t)();

    typedef ISDSC_STATUS (WINAPI *ReportIScsiSendTargetPortalsEx_t)(PULONG PortalCount,
                                                                    PULONG PortalInfoSize,
                                                                    PISCSI_TARGET_PORTAL_INFO_EX PortalInfo);

    typedef ISDSC_STATUS (WINAPI *GetIScsiInitiatorNodeName_t)(PTCHAR  InitiatorNodeName);

    typedef std::set<std::string> targetList_t;
    typedef std::list<ISCSI_DEVICE_ON_SESSION*> deviceInfos_t;

    template <typename T>
    T getProcAddress(HMODULE module, LPCSTR procName);

    bool cleanupDevices(ISCSI_UNIQUE_SESSION_ID& sessionId, ISCSI_TARGET_PORTAL& portal, std::string& results);

    void removePortal(ISCSI_TARGET_PORTAL& portal);

    void loginTarget(std::string const& initiatorName, unsigned long initiatorPort, std::string const& targetName, BOOLEAN persistent);
    void logoutTarget(std::string const& targetName, ISCSI_UNIQUE_SESSION_ID& sessionId);
    void removePersistentTarget(char* initiatorName, char* targetName, ISCSI_TARGET_PORTAL& portal);
    void removeDevices(ISCSI_UNIQUE_SESSION_ID& sessionId, ISCSI_TARGET_PORTAL& portal);

    void unbindAllPersistentVolumes();

    void reportTargets(targetList_t& targets, bool forceUpdate = false);
    void getDevicesForSession(ISCSI_UNIQUE_SESSION_ID& sessionId, deviceInfos_t& deviceInfos);

    bool targetFromPortal(std::string& targetName, ISCSI_TARGET_PORTAL& portal);

    char const* statusToString(unsigned long status);

    void setPortal(std::string const& portalIpAddress, std::string const& portalPort, ISCSI_TARGET_PORTAL& portal);

private:
    typedef std::vector<char> sessionInfoBuffer_t;
    typedef std::vector<char> deviceInfoBuffer_t;
    typedef std::vector<char> targetPortalInfoExBuffer_t;

    sessionInfoBuffer_t m_sessionInfoBuffer;
    deviceInfoBuffer_t m_deviceInfoBuffer;
    targetPortalInfoExBuffer_t m_targetPortalInfoExBuffer;

    char* m_resultStr;

    std::ostringstream m_stringNotFound;

    HMODULE m_iscsidscDll;

    // MS iSCSI APIs filled in with GetProcAddress
    // (note this is not all the iSCSI APIs just what we need)
    AddIScsiSendTargetPortal_t m_AddIScsiSendTargetPortal;
    ClearPersistentIScsiDevices_t m_ClearPersistentIScsiDevices;
    GetDevicesForIScsiSession_t m_GetDevicesForIScsiSession;
    GetIScsiSessionList_t m_GetIScsiSessionList;
    GetIScsiTargetInformation_t m_GetIScsiTargetInformation;
    LoginIScsiTarget_t m_LoginIScsiTarget;
    LogoutIScsiTarget_t m_LogoutIScsiTarget;
    RemoveIScsiPersistentTarget_t m_RemoveIScsiPersistentTarget;
    RemoveIScsiSendTargetPortal_t m_RemoveIScsiSendTargetPortal;
    ReportIScsiTargets_t m_ReportIScsiTargets;
    SetupPersistentIScsiDevices_t m_SetupPersistentIScsiDevices;
    ReportIScsiSendTargetPortalsEx_t m_ReportIScsiSendTargetPortalsEx;
    GetIScsiInitiatorNodeName_t m_GetIScsiInitiatorNodeName;
};

#endif // ISCSIINTERFACE_H
