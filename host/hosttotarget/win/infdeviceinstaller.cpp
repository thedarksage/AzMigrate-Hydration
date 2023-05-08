
///
/// \file infdeviceinstaller.cpp
///
/// \brief
///

#include <windows.h>
#include <Cfgmgr32.h>

#include <set>

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include "infdeviceinstaller.h"
#include "strconvert.h"

namespace HostToTarget {

#define IS_FLAG_SET(flagToCheck, flagsToTest) (flagToCheck == (flagToCheck & flagsToTest))

    typedef boost::char_separator<wchar_t, std::wstring::traits_type> wcharSep_t;
    typedef boost::tokenizer<boost::char_separator<wchar_t, std::wstring::traits_type>, std::wstring::const_iterator, std::wstring> wtokenizer_t;

    struct SectionNameAndHkrReplacement {
        wchar_t* m_section;
        wchar_t* m_hkrReplacement;
    };

    SectionNameAndHkrReplacement g_sectionNamesToProcess[] =
    {
        { L"service", L"Control" },
        { L"hw", L"Enum" },
        { L"coinstaller", 0 },
        { L"logConfigOverride", 0 },
        { L"insterfaces", 0 },
        { L"factdef", 0 },
        { L"wmi", 0 },

        // must always be last;
        { 0, 0 }
    };

    /// \brief device classed that need to be installed
    ///
    /// these will be cheked against an INF class to see if
    /// the INF needs to be processed
    wchar_t* g_deviceClasses[] = {
        L"hdc",
        L"SCSIAdapter",

        // must be last
        0
    };

    InfDeviceInstaller::InfDeviceInstaller()
        : m_errorResult(ERROR_OK)
    {
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"AddInterface", boost::bind(&InfDeviceInstaller::addInterface, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"AddPowerSetting", boost::bind(&InfDeviceInstaller::addPowerSetting, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"AddProperty", boost::bind(&InfDeviceInstaller::addProperty, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"AddReg", boost::bind(&InfDeviceInstaller::addReg, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"AddService", boost::bind(&InfDeviceInstaller::addService, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"BitReg", boost::bind(&InfDeviceInstaller::bitReg, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"CopyFiles", boost::bind(&InfDeviceInstaller::copyFiles, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"CopyINF", boost::bind(&InfDeviceInstaller::copyINF, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"DelFiles", boost::bind(&InfDeviceInstaller::delFiles, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"DelProperty", boost::bind(&InfDeviceInstaller::delProperty, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"DelReg", boost::bind(&InfDeviceInstaller::delReg, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"DelService", boost::bind(&InfDeviceInstaller::delService, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"DriverVer", boost::bind(&InfDeviceInstaller::driverVer, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"FeatureScore", boost::bind(&InfDeviceInstaller::featureScore, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"HardwareId", boost::bind(&InfDeviceInstaller::hardwareId, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"Ini2Reg", boost::bind(&InfDeviceInstaller::ini2Reg, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"LogConfig", boost::bind(&InfDeviceInstaller::logConfig, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"ProfileItems", boost::bind(&InfDeviceInstaller::profileItems, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"RegisterDlls", boost::bind(&InfDeviceInstaller::registerDlls, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"RenFiles", boost::bind(&InfDeviceInstaller::renFiles, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"UnregisterDlls", boost::bind(&InfDeviceInstaller::unregisterDlls, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"UpdateIniFields", boost::bind(&InfDeviceInstaller::updateIniFields, this, _1, _2)));
        m_infDirectiveNameAndFunction.insert(std::make_pair(L"UpdateInis", boost::bind(&InfDeviceInstaller::updateInis, this, _1, _2)));
    }

    ERROR_RESULT InfDeviceInstaller::install(ORHKEY systemHiveKey, ORHKEY softwareHiveKey, char const* infDir, OsTypes osType)
    {
        m_osType = osType;
        std::wstring wideInfDir = toWide(infDir);
        m_systemHiveKey = systemHiveKey;
        m_softwareHiveKey = softwareHiveKey;
        bool startServiceOnly = false;
        switch (m_osType) {
            case OS_TYPE_VISTA_32:
            case OS_TYPE_VISTA_64:
            case OS_TYPE_W2K8_32:
            case OS_TYPE_W2K8_64:
            case OS_TYPE_W2K8_R2:
            case OS_TYPE_WIN7:
                startServiceOnly = true;
                break;
            default:
                startServiceOnly = false;
                break;
        }

        if (getHardwareIds()) {
            getHardwareIdInstallSectionInfName(wideInfDir.c_str());
            if (ERROR_OK == m_errorResult.m_err) {
                hardwareIdInstallSectionInfName_t hwIdsToInstall;
                getHardwareIdsToInstall(hwIdsToInstall);
                if (ERROR_OK == m_errorResult.m_err) {
                    // TODO: maybe optimize this so that only call
                    // when just seting service to start on boot and
                    // change the section processing order for fullDeviceInstall
                    // such that .Service is done first and skip it if serive
                    // that we do not care about that will avoid processing .Service section
                    // twice in fullDeviceInstall case
                    getServicesToStartOnBoot(hwIdsToInstall);
                    if (startServiceOnly) {
                        setDeviceServiceToStartOnBoot(hwIdsToInstall);
                    } else {
#if INF_FULL_INSTALL_ENABLED
                        fullDeviceInstall(hwIdsToInstall);
#else
                        m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER_NOT_IMPLEMENTED;
#endif
                    }
                }
            }
        }
        return m_errorResult;
    }

    bool InfDeviceInstaller::getDeviceRegistryProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA& deviceInfoData, DWORD prop, wbuffer_t& deviceProperty)
    {
        deviceProperty.clear();
        DWORD error;
        DWORD size;
        if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, &deviceInfoData, prop, 0, 0, 0, &size)) {
            error = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER != error) {
                return false;
            } else {
                DWORD dataType;
                deviceProperty.resize(size);
                if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, &deviceInfoData, prop, &dataType, (LPBYTE)&deviceProperty[0], size, &size)) {
                    m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER;
                    m_errorResult.m_system = GetLastError();
                    m_errorResult.m_msg = "getting property ";
                    m_errorResult.m_msg += boost::lexical_cast<std::string>(prop);
                    return false;
                }
            }
        }
        return true;
    }

    bool InfDeviceInstaller::getHardwareIds()
    {
        DWORD error;
        for (int i = 0; 0 != g_deviceClasses[i]; ++i) {
            DWORD size = 2;
            std::vector<GUID> guids(size);
            bool found = false;
            do {
                if (!SetupDiClassGuidsFromNameExW(g_deviceClasses[i], &guids[0], size, &size, 0, 0)) {
                    error = GetLastError();
                    if (ERROR_INSUFFICIENT_BUFFER != error) {
                        m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER;
                        m_errorResult.m_system = error;
                        m_errorResult.m_msg = "getting guid for ";
                        m_errorResult.m_msg += toNarrow(g_deviceClasses[i], '.');
                        found = false;
                        break;
                    }
                    guids.resize(size);
                } else {
                    found = true;
                    break;
                }
            } while (ERROR_INSUFFICIENT_BUFFER == error);

            if (!found) {
                continue;
            }
            classInsertResult_t classInsertResult = m_classDeviceHardwareIds.insert(std::make_pair(g_deviceClasses[i], deviceHardwareIds_t()));
            if (!classInsertResult.second) {
                continue;
            }
            HDEVINFO hDevInfo = INVALID_HANDLE_VALUE;
            std::vector<GUID>::iterator guidIter(guids.begin());
            std::vector<GUID>::iterator guidIterEnd(guids.end());
            for (/* empty */; guidIter != guidIterEnd; ++guidIter) {
                if (INVALID_HANDLE_VALUE != hDevInfo) {
                    SetupDiDestroyDeviceInfoList(hDevInfo);
                }
                hDevInfo = SetupDiGetClassDevsExW(&(*guidIter), 0, 0, DIGCF_PRESENT, 0, 0, 0);
                if (INVALID_HANDLE_VALUE == hDevInfo) {
                    continue;
                }
                SP_DEVINFO_DATA deviceInfoData;
                deviceInfoData.cbSize = sizeof(deviceInfoData);
                DWORD idx = 0;
                while (SetupDiEnumDeviceInfo(hDevInfo, idx, &deviceInfoData)) {
                    ++idx;
                    std::wstring instanceId = getDeviceInstanceId(hDevInfo, deviceInfoData);
                    wbuffer_t desc;
                    if (!getDeviceRegistryProperty(hDevInfo, deviceInfoData, SPDRP_DEVICEDESC, desc)) {
                        break;
                    }
                    instanceId += L" <:> ";
                    instanceId += &desc[0];
                    deviceInsertResult_t deviceInsertResult = classInsertResult.first->second.insert(std::make_pair(&instanceId[0], hardwareIds_t()));
                    if (!deviceInsertResult.second) {
                        continue;
                    }
                    wbuffer_t ids;
                    if (getDeviceRegistryProperty(hDevInfo, deviceInfoData, SPDRP_HARDWAREID, ids)) {
                        addHwids(deviceInsertResult, ids);
                    }
                    if (getDeviceRegistryProperty(hDevInfo, deviceInfoData, SPDRP_COMPATIBLEIDS, ids)) {
                        addHwids(deviceInsertResult, ids);
                    }
                }
                error = GetLastError();
            }
            if (INVALID_HANDLE_VALUE != hDevInfo) {
                SetupDiDestroyDeviceInfoList(hDevInfo);
            }
        }
        return true;
    }

    void InfDeviceInstaller::getHardwareIdInstallSectionInfName(wchar_t const* infDir)
    {
        try {
            boost::filesystem::wdirectory_iterator dirIter(infDir);
            boost::filesystem::wdirectory_iterator dirIterEnd;
            for (/* empty */; dirIter != dirIterEnd; ++dirIter) {
                if (boost::filesystem::is_directory((*dirIter).status())) {
                    getHardwareIdInstallSectionInfName((*dirIter).path().string().c_str());
                } else if (boost::filesystem::is_regular_file((*dirIter).status()) && L".inf" == (*dirIter).path().extension()) {
                    m_hInf = INVALID_HANDLE_VALUE;
                    int i = 0;
                    for (/* empty */; 0 != g_deviceClasses[i]; ++i) {
                        m_hInf = SetupOpenInfFileW((*dirIter).path().string().c_str(), g_deviceClasses[i], INF_STYLE_WIN4, NULL);
                        if (INVALID_HANDLE_VALUE != m_hInf) {
                            break;
                        }
                    }
                    if (INVALID_HANDLE_VALUE != m_hInf) {
                        processMfgSection(g_deviceClasses[i], (*dirIter).path().string().c_str());
                        SetupCloseInfFile(m_hInf);
                    }
                }
            }
        } catch (std::exception const& e) {
            m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER;
            m_errorResult.m_msg = e.what();
        } catch (...) {
            m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER;
            m_errorResult.m_msg = "unknown exception";
        }
    }

    void InfDeviceInstaller::processMfgSection(wchar_t const* deviceClass, wchar_t const* infName)
    {
        INFCONTEXT infContext;
        if (SetupFindFirstLineW(m_hInf, L"Manufacturer", NULL, &infContext)) {
            do {
                wbuffer_t buffer(512);
                DWORD size = (DWORD)buffer.size();
                DWORD idx = 1;
                DWORD count = SetupGetFieldCount(&infContext);
                // could be
                // mfg-name | %str%=mgf-name | %str%=mgf-name[,targetos1[, target os2[,...]]]
                // NOTE: if it is plain mfg-name, that will be field 1 not field 0 so always
                // ok to just start with field 1
                if (!getStringField(infContext, idx, buffer, size)) {
                    continue; // may should just return error
                }
                ++idx;
                std::wstring mfgName(&buffer[0]);
                processModelSection(deviceClass, infName, mfgName.c_str());
                for (/* empty */; idx <= count; ++idx) {
                    if (!getStringField(infContext, idx, buffer, size)) {
                        break;
                    }
                    processModelSection(deviceClass, infName, mfgName.c_str(), &buffer[0]);
                }
            } while (SetupFindNextLine(&infContext, &infContext));
        }
    }

    void InfDeviceInstaller::processModelSection(wchar_t const* deviceClass, wchar_t const* infName, wchar_t const* mfgSection, wchar_t const* archOs)
    {
        INFCONTEXT infContext;
        std::wstring modelSection(mfgSection);
        if (0 != archOs) {
            modelSection += L".";
            modelSection += archOs;
        }

        if (SetupFindFirstLineW(m_hInf, modelSection.c_str(), NULL, &infContext)) {
            do {
                wbuffer_t buffer(512);
                DWORD size = (DWORD)buffer.size();
                DWORD count = SetupGetFieldCount(&infContext);
                for (DWORD i = 2; i <= count; ++i) {
                    if (getStringField(infContext, i, buffer, size)) {
                        if (size > 1) {
                            if (findHardwareId(deviceClass, &buffer[0])) {
                                wbuffer_t installName(512);
                                if (getStringField(infContext, 1, installName, size)) {
                                    m_hardwareIdInstallSectionInfName.insert(std::make_pair(boost::algorithm::to_upper_copy(std::wstring(&buffer[0])),
                                                                                            HardwareIdInstallSectionInfName(&buffer[0], &installName[0], infName, archOs)));
                                }
                            }
                        }
                    }
                }
            } while (SetupFindNextLine(&infContext, &infContext));
        }
    }

    std::wstring InfDeviceInstaller::getDeviceInstanceId(HDEVINFO hDevInfo, SP_DEVINFO_DATA& deviceInfoData)
    {
        SP_DEVINFO_LIST_DETAIL_DATA_W devInfoListDetail;
        devInfoListDetail.cbSize = sizeof(devInfoListDetail);
        if (SetupDiGetDeviceInfoListDetailW(hDevInfo, &devInfoListDetail)) {
            wchar_t instanceId[MAX_DEVICE_ID_LEN];
            if (CR_SUCCESS == CM_Get_Device_ID_ExW(deviceInfoData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0, devInfoListDetail.RemoteMachineHandle)) {
                return instanceId;
            }
        }
        return std::wstring();
    }

    void InfDeviceInstaller::addHwids(deviceInsertResult_t& deviceInstertResult, wbuffer_t const& hwIds)
    {
        wchar_t const* buffer = &hwIds[0];
        while (0 != *buffer) {
            deviceInstertResult.first->second.push_back(boost::algorithm::to_upper_copy(std::wstring(buffer)));
            buffer += wcslen(buffer) + 1;
        }
    }

    bool InfDeviceInstaller::findHardwareId(wchar_t const* deviceClass, wchar_t const* id)
    {
        classDeviceHardwareIds_t::iterator findIter(m_classDeviceHardwareIds.find(deviceClass));
        if (m_classDeviceHardwareIds.end() != findIter) {
            deviceHardwareIds_t::iterator deviceIter((*findIter).second.begin());
            deviceHardwareIds_t::iterator deviceIterEnd((*findIter).second.end());
            for (/* empty */; deviceIter != deviceIterEnd; ++deviceIter) {
                hardwareIds_t::iterator idsIter((*deviceIter).second.begin());
                hardwareIds_t::iterator idsIterEnd((*deviceIter).second.end());
                for (/* empty*/; idsIter != idsIterEnd; ++idsIter) {
                    if (boost::algorithm::iequals((*idsIter), id)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void InfDeviceInstaller::archOsToMajMinArch(std::wstring const& archOs, int& maj, int& min, int& arch)
    {
        // archOs has the followoing form
        //
        //   NT[Architecture][.[OSMajorVersion][.[OSMinorVersion][.[ProductType][.SuiteMask]]]]
        //
        // we will only consider NT[Architecture][.[OSMajorVersion][.[OSMinorVersion] (case does not matter)
        //
        // [Architecture] will be x86, ia64, or amd64
        // [OSMajorVersion][.[OSMinorVersion] will be typical major minor (e.g. 6.2, 5.1, etc)
        arch = -1;
        maj = -1;
        min = -1;
        if (archOs.empty()) {
            return;
        }
        std::wstring tmpArchOs(archOs);
        wcharSep_t sep(L".");
        wtokenizer_t tokens(tmpArchOs, sep);
        wtokenizer_t::iterator iter(tokens.begin());
        wtokenizer_t::iterator iterEnd(tokens.end());
        if (iter == iterEnd) {
            // should not happen
            return;
        }
        if (boost::algorithm::iequals(*iter, L"ntamd64")) {
            arch = 64;
        } else if (boost::algorithm::iequals(*iter, L"ntx86")) {
            arch = 32;
        }
        ++iter;
        if (iter != iterEnd) {
            maj = boost::lexical_cast<int>(*iter);
            ++iter;
            if (iter != iterEnd) {
                min = boost::lexical_cast<int>(*iter);
            }
        }
    }

    InfDeviceInstaller::hardwareIdInstallSectionInfName_t::iterator InfDeviceInstaller::getBestHardwareIdMatch(std::wstring const& findId)
    {
        // looking for the entry for findId that has the closest archOs match (i.e. <= target arch os)
        //
        // the arch os stored with the hardware id is from the inf model section and will be of the form
        //
        // some examples of archOs in order of priority
        //
        // NTamd64.6.2
        // NT.6.2
        // NTamd64.6
        // NT.6
        // NTamd64
        // "" (no archOs specfied)

        int tgtMaj;
        int tgtMin;
        int tgtArch;

        osTypeToMajMinArch(m_osType, tgtMaj, tgtMin, tgtArch);

        archMajMin_t archMajMin;

        // order arch maj and min by prefered priority
        hardwareIdInstallSectionInfName_t::iterator findIter(m_hardwareIdInstallSectionInfName.lower_bound(findId));
        hardwareIdInstallSectionInfName_t::iterator findIterEnd(m_hardwareIdInstallSectionInfName.upper_bound(findId));
        for (/* empty */; findIter != findIterEnd; ++findIter) {
            int maj;
            int min;
            int arch;
            archOsToMajMinArch((*findIter).second.m_archOs, maj, min, arch);
            archMajMin.insert(ArchMajMin(arch, maj, min, findIter));
        }
        // now look for the best match, since ordered by
        // piroirty can stop as soon as a match is found
        archMajMin_t::iterator iter(archMajMin.begin());
        archMajMin_t::iterator iterEnd(archMajMin.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            int maj = (*iter).m_maj;
            int min = (*iter).m_min;
            int arch = (*iter).m_arch;
            if ((maj == tgtMaj && min == tgtMin && arch == tgtArch) // maj, min, and arch all match         : NTamd64.6.2
                || (maj == tgtMaj && min == tgtMin && -1 == arch)   // maj and min match (no arch specified): NT.6.2
                || (maj == tgtMaj && -1 == min && arch == tgtArch)  // maj and arch match (no min specified): NTamd64.6
                || (-1 == maj && -1 == min && arch == tgtArch)      // arch matches (no ver specified)      : NTamd64
                || (-1 == maj && -1 == min && -1 == tgtArch)        // no arch os specified                 : ""
                ) {
                break;
            }
        }
        if (iter != iterEnd) {
            return (*iter).m_iter;
        }
        // FIXME: maybe need to check for closest version < tgt version
        return findIterEnd;
    }

    void InfDeviceInstaller::getHardwareIdsToInstall(hardwareIdInstallSectionInfName_t& hwIdsToInstall)
    {
        classDeviceHardwareIds_t::iterator iter(m_classDeviceHardwareIds.begin());
        classDeviceHardwareIds_t::iterator iterEnd(m_classDeviceHardwareIds.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            deviceHardwareIds_t::iterator deviceIter((*iter).second.begin());
            deviceHardwareIds_t::iterator deviceIterEnd((*iter).second.end());
            for (/* empty */; deviceIter != deviceIterEnd; ++deviceIter) {
                hardwareIds_t::iterator idsIter((*deviceIter).second.begin());
                hardwareIds_t::iterator idsIterEnd((*deviceIter).second.end());
                for (/* empty*/; idsIter != idsIterEnd; ++idsIter) {
                    hardwareIdInstallSectionInfName_t::iterator findIter(getBestHardwareIdMatch(*idsIter));
                    if (m_hardwareIdInstallSectionInfName.end() != findIter) {
                        hwIdsToInstall.insert(*findIter);
                        break;
                    }
                }
            }
        }
    }

    void InfDeviceInstaller::getServicesToStartOnBoot(hardwareIdInstallSectionInfName_t& hwIdsToInstall)
    {
        hardwareIdInstallSectionInfName_t::iterator iter(hwIdsToInstall.begin());
        hardwareIdInstallSectionInfName_t::iterator iterEnd(hwIdsToInstall.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            m_hInf = SetupOpenInfFileW((*iter).second.m_infName.c_str(), NULL, INF_STYLE_WIN4, NULL);
            if (INVALID_HANDLE_VALUE == m_hInf) {
                continue;
            }
            INFCONTEXT infContext;
            std::wstring installServiceSection((*iter).second.m_installSection);
            installServiceSection += L".Services";
            if (SetupFindFirstLineW(m_hInf, installServiceSection.c_str(), NULL, &infContext)) {
                do {
                    wbuffer_t buffer(512);
                    DWORD size = (DWORD)buffer.size();
                    if (getStringField(infContext, 0, buffer, size)) {
                        if (boost::algorithm::iequals(&buffer[0], L"AddService")) {
                            if (getStringField(infContext, 1, buffer, size)) {
                                if (!boost::algorithm::icontains(&buffer[0], L"iscsiprt")) {
                                    m_idAndService.insert(std::make_pair((*iter).first, &buffer[0]));
                                }
                            }
                        }
                    }
                } while (SetupFindNextLine(&infContext, &infContext));
            }
            SetupCloseInfFile(m_hInf);
        }
    }

    bool InfDeviceInstaller::setDeviceServiceToStartOnBoot(hardwareIdInstallSectionInfName_t& hwIdsToInstall)
    {
        wbuffer_t name(MAX_PATH);
        DWORD nameLength = (DWORD)name.size();
        FILETIME lastWriteTime;
        DWORD idx = 0;
        DWORD rc;
        do {
            do {
                name.resize(nameLength);
                rc = OREnumKey(m_systemHiveKey, idx, &name[0], &nameLength, 0, 0, &lastWriteTime);
            } while (ERROR_MORE_DATA == rc);
            if (ERROR_SUCCESS == rc) {
                if (boost::algorithm::istarts_with(&name[0], L"ControlSet")) {
                    idAndService_t::iterator iter(m_idAndService.begin());
                    idAndService_t::iterator iterEnd(m_idAndService.end());
                    for (/* empty */; iter != iterEnd; ++iter) {
                        std::wstring keyName(&name[0]);
                        keyName += L"\\Services\\";
                        keyName += (*iter).second;
                        ORHKEY orKey;
                        rc = OROpenKey(m_systemHiveKey, keyName.c_str(), &orKey);
                        if (ERROR_SUCCESS == rc) {
                            DWORD type = REG_DWORD;
                            DWORD start = 0;
                            rc = ORSetValue(orKey, L"Start", type, (BYTE const*)&start, sizeof(DWORD));
                            ORCloseKey(orKey);
                        }
                    }
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == rc);
        if (ERROR_NO_MORE_ITEMS != rc) {
            m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER;
            m_errorResult.m_system = rc;
            return false;
        }
        return true;
    }

    bool InfDeviceInstaller::getHklmControlSetNames(hklmControlSetNames_t& hklmControlSetNames)
    {
        wbuffer_t name(MAX_PATH);
        DWORD nameLength = (DWORD)name.size();
        FILETIME lastWriteTime;
        DWORD idx = 0;
        DWORD rc;
        do {
            do {
                name.resize(nameLength);
                rc = OREnumKey(m_systemHiveKey, idx, &name[0], &nameLength, 0, 0, &lastWriteTime);
            } while (ERROR_MORE_DATA == rc);
            if (ERROR_SUCCESS == rc) {
                if (boost::algorithm::istarts_with(&name[0], L"ControlSet")) {
                    hklmControlSetNames.insert(&name[0]);
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == rc);
        if (ERROR_NO_MORE_ITEMS != rc) {
            m_errorResult.m_err = ERROR_INTERNAL_INF_DEVICE_INSTALLER;
            m_errorResult.m_system = rc;
            return false;
        }
        return true;
    }

    bool InfDeviceInstaller::fullDeviceInstall(hardwareIdInstallSectionInfName_t& hwIdsToInstall)
    {
        hklmControlSetNames_t hklmControlSetNames;
        if (!getHklmControlSetNames(hklmControlSetNames)) {
            // NOTE: use m_idAndService as that will have filtered out ids
            // we do not carea about (e.g. iscsi)
            idAndService_t::iterator iter(m_idAndService.begin());
            idAndService_t::iterator iterEnd(m_idAndService.end());
            for (/* empty */; iter != iterEnd; ++iter) {
                hardwareIdInstallSectionInfName_t::iterator findIter(hwIdsToInstall.find((*iter).first));
                if (hwIdsToInstall.end() != findIter) {
                    installSection((*findIter).second.m_installSection, hklmControlSetNames);
                }
            }
        }
        return (ERROR_OK == m_errorResult.m_err);
    }

    void InfDeviceInstaller::installSection(std::wstring const& section, hklmControlSetNames_t& hklmControlSetNames)
    {
        processSection(section, L"control", hklmControlSetNames);
        if (ERROR_OK == m_errorResult.m_err) {
            for (int i = 0; 0 != g_sectionNamesToProcess[i].m_section; ++i) {
                std::wstring sectionName(section);
                sectionName += L".";
                sectionName += g_sectionNamesToProcess[i].m_section;
                processSection(sectionName, g_sectionNamesToProcess[i].m_hkrReplacement, hklmControlSetNames);
            }
        }
    }

    void InfDeviceInstaller::processSection(std::wstring const& section, const wchar_t* hkrReplacement, hklmControlSetNames_t& hklmControlSetNames)
    {

        INFCONTEXT infContext;
        if (SetupFindFirstLineW(m_hInf, section.c_str(), NULL, &infContext)) {
            do {
                wbuffer_t buffer(512);
                DWORD size = (DWORD)buffer.size();
                if (getStringField(infContext, 0, buffer, size)) {
                    infDirectiveNameAndFunction_t::iterator iter(m_infDirectiveNameAndFunction.find(&buffer[0]));
                    if (m_infDirectiveNameAndFunction.end() != iter) {
                        (*iter).second(infContext, hkrReplacement);
                    }
                }
            } while (SetupFindNextLine(&infContext, &infContext));
        }
        SetupCloseInfFile(m_hInf);
    }

    void InfDeviceInstaller::addInterface(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::addPowerSetting(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::addProperty(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::addReg(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
        wbuffer_t buffer(512);
        DWORD size = (DWORD)buffer.size();
        DWORD count = SetupGetFieldCount(&infContext);
        for (DWORD i = 1; i <= count; ++i) {
            if (!getStringField(infContext, i, buffer, size)) {
                break;
            }
            addRegUpdateRegistry(&buffer[0], hkrReplacement);
        }
    }

    void InfDeviceInstaller::addService(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::bitReg(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::copyFiles(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::copyINF(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::delFiles(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::delProperty(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::delReg(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::delService(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::driverVer(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::featureScore(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::hardwareId(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::ini2Reg(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::logConfig(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::profileItems(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::registerDlls(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::renFiles(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::unregisterDlls(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::updateIniFields(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::updateInis(INFCONTEXT& infContext, wchar_t const* hkrReplacement)
    {
    }

    void InfDeviceInstaller::offregUpdate(ORHKEY key, wbuffer_t const& subKey, wbuffer_t const& valueName, DWORD flags, DWORD regType, wbuffer_t const& value, DWORD valueLen)
    {
        DWORD type = regType;
        wbuffer_t data(512);
        DWORD dataLen = (DWORD)data.size();
        DWORD rc = ERROR_SUCCESS;
        BYTE const* newValue = (BYTE const*)&value[0];

        if (IS_FLAG_SET(FLG_ADDREG_NOCLOBBER, flags)
            || IS_FLAG_SET(FLG_ADDREG_APPEND, flags)
            || IS_FLAG_SET(FLG_ADDREG_OVERWRITEONLY, flags)) {
            do {
                data.resize(dataLen);
                rc = ORGetValue(key, &subKey[0], &valueName[0], &type, (void*)&data, &dataLen);
            } while (ERROR_MORE_DATA == rc);
        }

        if (IS_FLAG_SET(FLG_ADDREG_KEYONLY, flags)) {
            ORHKEY newKey;
            DWORD disp;
            rc = ORCreateKey(key, &subKey[0], 0, 0, 0, &newKey, &disp);
            if (ERROR_SUCCESS == rc) {
                ORCloseKey(newKey);
            } else {
            }
            return;
        }

        if (IS_FLAG_SET(FLG_ADDREG_DELVAL, flags)) {
            if (L'\0' == value[0]) {
                ORDeleteKey(key, &subKey[0]);
            } else {
                ORHKEY newKey;
                rc = OROpenKey(key, &subKey[0], &newKey);
                if (ERROR_SUCCESS == rc) {
                    ORDeleteValue(newKey, &valueName[0]);
                    ORCloseKey(newKey);
                }
            }
            return;
        }

        if (IS_FLAG_SET(FLG_ADDREG_NOCLOBBER, flags) && ERROR_SUCCESS == rc) {
            return;
        }

        if (IS_FLAG_SET(FLG_ADDREG_OVERWRITEONLY, flags) && ERROR_SUCCESS != rc) {
            return;
        }

        if (IS_FLAG_SET(FLG_ADDREG_APPEND, flags) && ERROR_SUCCESS == rc) {
            if (boost::algorithm::icontains(&data[0], &value[0])) {
                return;
            }
            wbuffer_t tmp(data.size());
            tmp = data;
            data.resize(tmp.size() + value.size());
            data = tmp;
            data.insert(data.end(), value.begin(), value.end());
            newValue = (BYTE const*)&data[0];
            valueLen = (DWORD)data.size();
        }
        rc = ORSetValue(key, &subKey[0], regType, newValue, valueLen);
    }

    void InfDeviceInstaller::updateRegistry(wchar_t const* hkrReplacement, wbuffer_t const& rootKey, wbuffer_t const& subKey, wbuffer_t const& valueName, DWORD flags, DWORD regType, wbuffer_t const& value, DWORD len)
    {
        ORHKEY orRootKey;
        if (boost::algorithm::iequals(&rootKey[0], L"HKR")) {
            orRootKey = m_systemHiveKey; // for now we only support HKR for HKLM\system
        } else if (boost::algorithm::iequals(&rootKey[0], L"HKLM")) {
            // currently only support root HKLM
            if (boost::algorithm::istarts_with(&subKey[0], L"system")) {
                orRootKey = m_systemHiveKey; // for now we only support HKR for HKLM\system
            } else if (boost::algorithm::istarts_with(&subKey[0], L"software")) {
                orRootKey = m_softwareHiveKey; // for now we only support HKR for HKLM\system
            } else {
                return;
            }
        }
        offregUpdate(orRootKey, subKey, valueName, flags, regType, value, len);
    }

    void InfDeviceInstaller::addRegUpdateRegistry(wchar_t const* addRegSection, wchar_t const* hkrReplacement)
    {
        // each entry in and addreg will be
        // reg-root, [subkey],[value-entry-name],[flags],[value][,[value]]
        INFCONTEXT infContext;
        DWORD regType = REG_SZ;
        if (SetupFindFirstLineW(m_hInf, addRegSection, NULL, &infContext)) {
            do {
                wbuffer_t rootKey(512);
                DWORD size = (DWORD)rootKey.size();
                rootKey[0] = L'\0';
                DWORD idx = 1;
                DWORD count = SetupGetFieldCount(&infContext);
                // get reg-root
                if (!getStringField(infContext, idx, rootKey, size)) {
                    continue;
                }
                // get subkey
                ++idx;
                wbuffer_t subKey(512);
                size = (DWORD)subKey.size();
                subKey[0] = '\0';
                if (idx <= count) {
                    if (!getStringField(infContext, idx, subKey, size)) {
                        continue;
                    }

                }
                // get value-entry-name
                ++idx;
                wbuffer_t valueName(512);
                size = (DWORD)valueName.size();
                valueName[0] = '\0';
                if (idx <= count) {
                    if (!getStringField(infContext, idx, valueName, size)) {
                        continue;
                    }

                }
                // get flags
                ++idx;
                wbuffer_t buffer(512);
                size = (DWORD)buffer.size();
                buffer[0] = '\0';
                if (idx <= count) {
                    if (!getStringField(infContext, idx, buffer, size)) {
                        continue;
                    }

                }
                // docs do not say how to treat flags value if not present but values present
                // might be able to deduce type after reading values anc checking them all
                // for now treat as FLG_ADDREG_TYPE_SZ
                DWORD flags = FLG_ADDREG_TYPE_SZ;
                try {
                    if ('\0' != buffer[0]) {
                        flags = hexCast(&buffer[0]);
                    }
                } catch (...) {
                    continue;
                }
                // get value(s)
                ++idx;
                wbuffer_t value(512);
                size = (DWORD)value.size();
                value[0] = '\0';
                switch (FLG_ADDREG_TYPE_MASK & flags) {
                    case FLG_ADDREG_TYPE_SZ:
                        regType = REG_SZ;
                        getStringField(infContext, idx, value, size);
                        break;
                    case FLG_ADDREG_TYPE_MULTI_SZ:
                        regType = REG_MULTI_SZ;
                        getMultiStringField(infContext, idx, value, size);
                        break;
                    case FLG_ADDREG_TYPE_EXPAND_SZ:
                        regType = REG_EXPAND_SZ;
                        getStringField(infContext, idx, value, size);
                        break;
                    case FLG_ADDREG_TYPE_BINARY:
                        regType = REG_BINARY;
                        getBinaryField(infContext, idx, value, size);
                        break;
                    case FLG_ADDREG_TYPE_DWORD:
                        regType = REG_DWORD;
                        getStringField(infContext, idx, value, size); // will need to convert to DWARD
                        break;
                    case FLG_ADDREG_TYPE_NONE:
                        regType = REG_NONE;
                        getStringField(infContext, idx, value, size);
                        break;
                    default:
                        // probably custom reg type treat as binary
                        regType = REG_BINARY;
                        getBinaryField(infContext, idx, value, size);
                        break;
                }
                updateRegistry(hkrReplacement, rootKey, subKey, valueName, flags, regType, value, size);
            } while (SetupFindNextLine(&infContext, &infContext));
        }
    }

    bool InfDeviceInstaller::getStringField(INFCONTEXT& infContext, DWORD idx, wbuffer_t& buffer, DWORD& returnedSize)
    {
        if (!SetupGetStringFieldW(&infContext, idx, &buffer[0], (DWORD)buffer.size(), &returnedSize)) {
            DWORD error = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER != error) {
                return false;
            }
            buffer.resize(returnedSize);
            if (!SetupGetStringFieldW(&infContext, idx, &buffer[0], (DWORD)buffer.size(), &returnedSize)) {
                return false;
            }
        }
        return true;
    }

    bool InfDeviceInstaller::getMultiStringField(INFCONTEXT& infContext, DWORD idx, wbuffer_t& buffer, DWORD& returnedSize)
    {
        if (!SetupGetMultiSzFieldW(&infContext, idx, &buffer[0], (DWORD)buffer.size(), &returnedSize)) {
            DWORD error = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER != error) {
                return false;
            }
            buffer.resize(returnedSize);
            if (!SetupGetStringFieldW(&infContext, idx, &buffer[0], (DWORD)buffer.size(), &returnedSize)) {
                return false;
            }
        }
        return true;
    }

    bool InfDeviceInstaller::getBinaryField(INFCONTEXT& infContext, DWORD idx, wbuffer_t& buffer, DWORD& returnedSize)
    {
        if (!SetupGetBinaryField(&infContext, idx, (BYTE*)&buffer[0], (DWORD)buffer.size(), &returnedSize)) {
            DWORD error = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER != error) {
                return false;
            }
            buffer.resize(returnedSize);
            if (!SetupGetStringFieldW(&infContext, idx, &buffer[0], (DWORD)buffer.size(), &returnedSize)) {
                return false;
            }
        }
        return true;
    }

    DWORD InfDeviceInstaller::hexCast(wchar_t const* hexStr)
    {
        std::wstringstream ss;
        ss << hexStr;
        DWORD d;
        ss >> std::hex >> d;
        return d;
    }

} //namesapce HostToTarget
