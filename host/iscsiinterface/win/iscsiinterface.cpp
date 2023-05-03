
//
//  File: iscsiinterface.cpp
//
//  Description:
//

#include <boost/lexical_cast.hpp>

#include "errorexception.h"
#include "iscsiinterface.h"
#include "inmsafecapis.h"

IScsi::IScsi()
    : m_resultStr(0),
      m_iscsidscDll(0)
{
    char systemDir[MAX_PATH];

    UINT result = GetSystemDirectory(systemDir, sizeof(systemDir));
    if (0 == result) {
        throw ERROR_EXCEPTION << "GetSystemDirectory failed: " << GetLastError() << '\n';
    }

    std::string iscsidscDllName(systemDir);
    iscsidscDllName += "\\iscsidsc.dll";
    m_iscsidscDll = LoadLibrary(iscsidscDllName.c_str());
    if (0 == m_iscsidscDll) {
        throw ERROR_EXCEPTION << "LoadLibrary " << iscsidscDllName << " failed: " << GetLastError();
    }

    m_AddIScsiSendTargetPortal = getProcAddress<AddIScsiSendTargetPortal_t>(m_iscsidscDll, "AddIScsiSendTargetPortalA");
    m_ClearPersistentIScsiDevices = getProcAddress<ClearPersistentIScsiDevices_t>(m_iscsidscDll, "ClearPersistentIScsiDevices");
    m_GetDevicesForIScsiSession = getProcAddress<GetDevicesForIScsiSession_t>(m_iscsidscDll, "GetDevicesForIScsiSessionA");
    m_GetIScsiSessionList = getProcAddress<GetIScsiSessionList_t>(m_iscsidscDll, "GetIScsiSessionListA");
    m_GetIScsiTargetInformation = getProcAddress<GetIScsiTargetInformation_t>(m_iscsidscDll, "GetIScsiTargetInformationA");
    m_LoginIScsiTarget = getProcAddress<LoginIScsiTarget_t>(m_iscsidscDll, "LoginIScsiTargetA");
    m_LogoutIScsiTarget = getProcAddress<LogoutIScsiTarget_t>(m_iscsidscDll, "LogoutIScsiTarget");
    m_RemoveIScsiPersistentTarget = getProcAddress<RemoveIScsiPersistentTarget_t>(m_iscsidscDll, "RemoveIScsiPersistentTargetA");
    m_RemoveIScsiSendTargetPortal = getProcAddress<RemoveIScsiSendTargetPortal_t>(m_iscsidscDll, "RemoveIScsiSendTargetPortalA");
    m_ReportIScsiTargets = getProcAddress<ReportIScsiTargets_t>(m_iscsidscDll, "ReportIScsiTargetsA");
    m_SetupPersistentIScsiDevices = getProcAddress<SetupPersistentIScsiDevices_t>(m_iscsidscDll, "SetupPersistentIScsiDevices");
    m_ReportIScsiSendTargetPortalsEx = getProcAddress<ReportIScsiSendTargetPortalsEx_t>(m_iscsidscDll, "ReportIScsiSendTargetPortalsExA");
    m_GetIScsiInitiatorNodeName = getProcAddress<GetIScsiInitiatorNodeName_t>(m_iscsidscDll, "GetIScsiInitiatorNodeNameA");
}

IScsi::~IScsi()
{
    if (0 != m_resultStr) {
        LocalFree(m_resultStr);
    }

    if (0 != m_iscsidscDll) {
        FreeLibrary(m_iscsidscDll);
    }
}

template <typename T>
T IScsi::getProcAddress(HMODULE module, LPCSTR procName)
{
    T procAddress = reinterpret_cast<T>(GetProcAddress(module, procName));
    if (0 == procAddress) {
        throw ERROR_EXCEPTION << "GetProcAddress for " << procName << " failed: " << GetLastError() << '\n';
    }

    return procAddress;
}

void IScsi::getIScsiInitiatorNodeName(std::string& nodeName)
{
    std::vector<char> name(MAX_ISCSI_NAME_LEN+1);
    ISDSC_STATUS status = (*m_GetIScsiInitiatorNodeName)(&name[0]);
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "GetIScsiInitiatorNodeName failed: " << status << '\n';
    }

    nodeName = &name[0];
}

void IScsi::addPortal(std::string const& initiatorName, unsigned long initiatorInterfacePort, std::string const& portalIpAddress, std::string const& portalPort)
{
    ISCSI_TARGET_PORTAL targetPortal;

    setPortal(portalIpAddress, portalPort, targetPortal);

    // TODO: login options and security flags
    ISDSC_STATUS status = (m_AddIScsiSendTargetPortal)((char*)initiatorName.c_str(), initiatorInterfacePort, 0, 0, &targetPortal);
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "AddIScsiSendTargetPortal for portal " << portalIpAddress << ':' << portalPort << " failed: " << statusToString(status) << '\n';
    }
}

void IScsi::removeAllForPortal(std::string const& portalIpAddress, std::string const& portalPort)
{
    ISCSI_TARGET_PORTAL targetPortal;

    setPortal(portalIpAddress, portalPort, targetPortal);

    IScsi::sessionInfos_t sessionInfos;

    getSessionInfos(sessionInfos);

    IScsi::sessionInfos_t::iterator iter(sessionInfos.begin());
    IScsi::sessionInfos_t::iterator iterEnd(sessionInfos.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        if (0 == (*iter)->ConnectionCount || portalIpAddress == (*iter)->Connections->TargetAddress) {
            removeDevices((*iter)->SessionId, targetPortal);
            // HACK: for timing issues seen
            Sleep(100);
        }
    }
    // HACK: for timing issues seen
    Sleep(100);
}



void IScsi::removePortal(ISCSI_TARGET_PORTAL& targetPortal)
{
    ISDSC_STATUS status = (m_RemoveIScsiSendTargetPortal)(0, ISCSI_ALL_INITIATOR_PORTS, &targetPortal);
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "RemoveIScsiSendTargetPortal for portal " << targetPortal.Address << ':' << targetPortal.Socket << " failed: " << statusToString(status) << '\n';
    }
}

void IScsi::loginTarget(std::string const& initiatorName, unsigned long initiatorPort, std::string const& targetName, BOOLEAN persistent)
{
    ISCSI_UNIQUE_SESSION_ID sessionId;
    ISCSI_UNIQUE_CONNECTION_ID connectionId;
    ISCSI_LOGIN_OPTIONS options;

    memset(&options, 0, sizeof(options));
    options.LoginFlags = ISCSI_LOGIN_FLAG_MULTIPATH_ENABLED;

    ISDSC_STATUS status = (m_LoginIScsiTarget)(const_cast<char*>(targetName.c_str()),
                                               FALSE,
                                               NULL,
                                               ISCSI_ANY_INITIATOR_PORT,
                                               0,
                                               0,
                                               NULL,
                                               &options,
                                               0,
                                               NULL,
                                               persistent,
                                               &sessionId,
                                               &connectionId);
    if (ERROR_SUCCESS != status && ISDSC_TARGET_ALREADY_LOGGED_IN != status) {
        throw ERROR_EXCEPTION << "LoginIScsiTarget for target " << targetName << " failed: " << statusToString(status) << '\n';
    }
}

void IScsi::loginTarget(std::string const& initiatorName, unsigned long initiatorPort, std::string const& targetName)
{
    loginTarget(initiatorName, initiatorPort, targetName, FALSE);
    loginTarget(initiatorName, initiatorPort, targetName, TRUE);
}

void IScsi::logoutTarget(std::string const& targetName, ISCSI_UNIQUE_SESSION_ID& sessionId)
{
    ISDSC_STATUS status = (m_LogoutIScsiTarget)(&sessionId);
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "LogoutIScsiTarget for target " << targetName << " failed: " << statusToString(status) << '\n';
    }
}

void IScsi::removePersistentTarget(char* initiatorName, char* targetName, ISCSI_TARGET_PORTAL& portal)
{
    ISDSC_STATUS status = (m_RemoveIScsiPersistentTarget)(initiatorName, ISCSI_ALL_INITIATOR_PORTS, targetName, &portal);
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "RemoveIScsiPersistentTarget for target " << targetName << " failed: " << statusToString(status) << '\n';
    }
}

void IScsi::removeDevices(ISCSI_UNIQUE_SESSION_ID& sessionId, ISCSI_TARGET_PORTAL& portal)
{
    deviceInfos_t deviceInfos;
    getDevicesForSession(sessionId, deviceInfos);

    if (!deviceInfos.empty()) {
        deviceInfos_t::iterator iter(deviceInfos.begin());
        deviceInfos_t::iterator iterEnd(deviceInfos.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            removePersistentTarget((*iter)->InitiatorName, (*iter)->TargetName, portal);
            // HACK: for timing issues seen
            Sleep(100);
        }
    }
}

bool IScsi::cleanupDevices(ISCSI_UNIQUE_SESSION_ID& sessionId, ISCSI_TARGET_PORTAL& portal, std::string& results)
{
    deviceInfos_t deviceInfos;
    try {
        getDevicesForSession(sessionId, deviceInfos);
    } catch (std::exception& e) {
        results = e.what();
        return false;
    }

    if (!deviceInfos.empty()) {
        deviceInfos_t::iterator iter(deviceInfos.begin());
        deviceInfos_t::iterator iterEnd(deviceInfos.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            try {
                removePersistentTarget((*iter)->InitiatorName, (*iter)->TargetName, portal);
            } catch (std::exception& e) {
                if (!results.empty()) {
                    results += ';';
                }
                results += e.what();
            }
            // HACK: for timing issues seen
            Sleep(100);
        }
    }

    return results.empty();
}

void IScsi::bindAllPersistentVolumes()
{
    ISDSC_STATUS status = (m_SetupPersistentIScsiDevices)();
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "SetupPersistentIScsiDevices failed: " << statusToString(status) << '\n';
    }
}

void IScsi::unbindAllPersistentVolumes()
{
    ISDSC_STATUS status = (m_ClearPersistentIScsiDevices)();
    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "ClearPersistentIScsiDevices failed: " << statusToString(status) << '\n';
    }
}

void IScsi::reportTargets(targetList_t& targets, bool forceUpdate)
{
    unsigned long bufferSize = DEFAULT_BUFFER_SIZE;
    std::vector<char> buffer;

    ISDSC_STATUS status;

    do {
        buffer.resize(bufferSize);
        status = (m_ReportIScsiTargets)(forceUpdate, &bufferSize, &buffer[0]);
    } while (ERROR_INSUFFICIENT_BUFFER == status);

    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "ReportIScsiTargets failed: " << statusToString(status) << '\n';
    }

    char* target = &buffer[0];
    char* targetEnd = target;
    while (0 != *target) {
        while (0 != *targetEnd) {
            ++targetEnd;
        }
        targets.insert(target);
        ++targetEnd;
        target = targetEnd;
    }
}

void IScsi::getSessionInfos(sessionInfos_t& sessionInfos)
{
    unsigned long count;
    unsigned long bufferSize = DEFAULT_BUFFER_SIZE;

    ISDSC_STATUS status;

    do {
        m_sessionInfoBuffer.resize(bufferSize);
        status = (m_GetIScsiSessionList)(&bufferSize, &count, reinterpret_cast<ISCSI_SESSION_INFO*>(&m_sessionInfoBuffer[0]));
    } while (ERROR_INSUFFICIENT_BUFFER == status);

    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "GetIScsiSessionList failed: " << statusToString(status) << '\n';
    }

    ISCSI_SESSION_INFO* sessionInfo = reinterpret_cast<ISCSI_SESSION_INFO*>(&m_sessionInfoBuffer[0]);

    for (unsigned long i = 0; i < count; ++i, ++sessionInfo) {
        sessionInfos.push_back(sessionInfo);
    }
}

void IScsi::getTargetPortalsEx(targetPortalInfosEx_t& targetPortalInfosEx)
{
    unsigned long count;
    unsigned long bufferSize = DEFAULT_BUFFER_SIZE;

    ISDSC_STATUS status;

    do {
        m_targetPortalInfoExBuffer.resize(bufferSize);
        status = (m_ReportIScsiSendTargetPortalsEx)(&count, &bufferSize, reinterpret_cast<ISCSI_TARGET_PORTAL_INFO_EX*>(&m_targetPortalInfoExBuffer[0]));
    } while (ERROR_INSUFFICIENT_BUFFER == status);

    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "ReportIScsiSendTargetPortals failed: " << statusToString(status) << '\n';
    }

    ISCSI_TARGET_PORTAL_INFO_EX* targetPortalInfoEx = reinterpret_cast<ISCSI_TARGET_PORTAL_INFO_EX*>(&m_targetPortalInfoExBuffer[0]);
    for (unsigned long i = 0; i < count; ++i, ++targetPortalInfoEx) {
        targetPortalInfosEx.push_back(targetPortalInfoEx);
    }
}

void IScsi::getDevicesForSession(ISCSI_UNIQUE_SESSION_ID& sessionId, deviceInfos_t& deviceInfos)
{
    unsigned long count = 1;

    ISDSC_STATUS status;

    do {
        m_deviceInfoBuffer.resize(count * sizeof(ISCSI_DEVICE_ON_SESSION));
        status = (m_GetDevicesForIScsiSession)(&sessionId, &count, reinterpret_cast<ISCSI_DEVICE_ON_SESSION*>(&m_deviceInfoBuffer[0]));
    } while (ERROR_INSUFFICIENT_BUFFER == status);

    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "GetDevicesForIScsiSession failed: " << statusToString(status) << '\n';
    }

    ISCSI_DEVICE_ON_SESSION* deviceInfo = reinterpret_cast<ISCSI_DEVICE_ON_SESSION*>(&m_deviceInfoBuffer[0]);
    for (unsigned long i =0; i < count; ++i, ++deviceInfo) {
        deviceInfos.push_back(deviceInfo);
    }

}

bool IScsi::targetFromPortal(std::string& targetName, ISCSI_TARGET_PORTAL& portal)
{
    ISDSC_STATUS status;

    std::vector<char> buffer;

    unsigned long bufferSize = DEFAULT_BUFFER_SIZE;

    do {
        buffer.resize(bufferSize);
        status = (m_GetIScsiTargetInformation)(const_cast<char*>(targetName.c_str()), 0, DiscoveryMechanisms, &bufferSize, &buffer[0]);
    } while (ERROR_INSUFFICIENT_BUFFER == status);

    if (ERROR_SUCCESS != status) {
        throw ERROR_EXCEPTION << "GetIScsiTargetInformation for " << targetName << " failed: " << statusToString(status) << '\n';
    }

    char* info = &buffer[0];
    while (0 != *info) {
        if (0 != strstr(&buffer[0], portal.Address)) {
            return true;
        }
        info += strlen(info) + 1;
    }

    return false;
}

char const* IScsi::statusToString(unsigned long status)
{
    unsigned long result;

    if (0 != m_resultStr) {
        LocalFree(m_resultStr);
        m_resultStr = 0;
    }

    result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
                           | FORMAT_MESSAGE_FROM_SYSTEM
                           | FORMAT_MESSAGE_IGNORE_INSERTS
                           | FORMAT_MESSAGE_FROM_HMODULE,
                           GetModuleHandle(TEXT("iscsidsc.dll")),
                           status,
                           0,
                           (LPSTR)&m_resultStr,
                           0, //FORMAT_MESSAGE_MAX_WIDTH_MASK,
                           NULL);
    if (result > 0) {
        return m_resultStr;
    } else {
        m_stringNotFound.str(std::string());
        m_stringNotFound.clear();
        m_stringNotFound << status << "(note: could not find matching error text)";
        return m_stringNotFound.str().c_str();
    }

}

void IScsi::setPortal(std::string const& portalIpAddress, std::string const& portalPort, ISCSI_TARGET_PORTAL& portal)
{
    if (portalIpAddress.size() > MAX_ISCSI_PORTAL_ADDRESS_LEN) {
        throw ERROR_EXCEPTION << "targer ip address length ("
                              << portalIpAddress.length()
                              << ") exceeds max length ("
                              << MAX_ISCSI_PORTAL_ADDRESS_LEN
                              << '\n';
    }

    portal.SymbolicName[0] = '\0';
    inm_strcpy_s(portal.Address, ARRAYSIZE(portal.Address), portalIpAddress.c_str());
    try {
        portal.Socket = boost::lexical_cast<unsigned short>(portalPort);
    } catch (...) {
        portal.Socket = 3260; // TODO: should not hard code this
    }
}
