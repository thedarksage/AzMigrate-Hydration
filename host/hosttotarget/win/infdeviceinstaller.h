
///
/// \file infdeviceinstaller.h
///
/// \brief
///

#ifndef INFDEVICEINSTALLER_H
#define INFDEVICEINSTALLER_H


#include <windows.h>
#include <setupapi.h>

#include <vector>
#include <string>
#include <map>
#include <set>
#include <offreg.h>

#include <boost/function.hpp>

#include "errorresult.h"
#include "hosttotarget.h"

namespace HostToTarget {

    class InfDeviceInstaller {
    public:
        InfDeviceInstaller();

        ERROR_RESULT install(ORHKEY systemHiveKey, ORHKEY softwareHiveKey, char const* infDir, OsTypes osType);

    protected:
        typedef std::vector<wchar_t> wbuffer_t; ///< generic buffer for holding data returned by various setup apis

        typedef std::vector<std::wstring> hardwareIds_t;  ///< hardware and compatible ids order best to least match
        typedef std::map<std::wstring, hardwareIds_t> deviceHardwareIds_t;  ///< first: device description, second: its ids
        typedef std::map<std::wstring, deviceHardwareIds_t> classDeviceHardwareIds_t; ///< first: device class, second: device with its ids
        typedef std::pair<deviceHardwareIds_t::iterator, bool> deviceInsertResult_t; ///< returned by std::map::insert when instering into deviceHardwareIds_t
        typedef std::pair<classDeviceHardwareIds_t::iterator, bool> classInsertResult_t; ///< returned by std::map::insert when instering into classDeviceHardwareIds_t
        typedef std::set<std::wstring> hklmControlSetNames_t; ///< holds "HKLM\system\ControlSetN" where n is number (e.g. 001, 002)

        struct HardwareIdInstallSectionInfName {
            HardwareIdInstallSectionInfName(wchar_t const* id, wchar_t const* installSection, wchar_t const* infName, wchar_t const* archOs)
                : m_id(id),
                  m_installSection(installSection),
                  m_infName(infName)
                {
                    if (0 != archOs) {
                        m_archOs = archOs;
                    }
                }
            std::wstring m_id;
            std::wstring m_installSection;
            std::wstring m_infName;
            std::wstring m_archOs;
        };

        typedef std::multimap<std::wstring, HardwareIdInstallSectionInfName> hardwareIdInstallSectionInfName_t; ///< first: hwid, second: HardwareIdInstallSectionInfName

        struct ArchMajMin {
            ArchMajMin(int arch, int maj, int min, hardwareIdInstallSectionInfName_t::iterator iter)
                : m_arch(arch),
                  m_maj(maj),
                  m_min(min),
                  m_iter(iter)
                {
                }
            int m_arch;
            int m_maj;
            int m_min;
            hardwareIdInstallSectionInfName_t::iterator m_iter;
        };
        struct ArchMajMinGreater {
            bool operator()(ArchMajMin const& lhs, ArchMajMin const& rhs) const
                {
                    if (lhs.m_maj > rhs.m_maj) {
                        return true;
                    }
                    if (lhs.m_maj == rhs.m_maj) {
                        if (lhs.m_min > rhs.m_min) {
                            return true;
                        }
                        if (lhs.m_min == rhs.m_min) {
                            return lhs.m_arch > rhs.m_arch;
                        }
                    }
                    return false;
                }
        };
        typedef std::set<ArchMajMin, ArchMajMinGreater> archMajMin_t;
        typedef std::map<std::wstring, std::wstring> idAndService_t; ///< first: hwid, second: service name

        typedef boost::function<void (INFCONTEXT& infContext, wchar_t const* hkrReplacement)> infDirectiveFunction_t;
        typedef std::map<std::wstring, infDirectiveFunction_t> infDirectiveNameAndFunction_t;

        bool getDeviceRegistryProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA& deviceInfoData, DWORD prop, wbuffer_t& deviceProperty);
        bool getHardwareIds();
        void getHardwareIdInstallSectionInfName(wchar_t const* infDir);
        void processMfgSection(wchar_t const* deviceClass, wchar_t const* infName);
        void processModelSection(wchar_t const* deviceClass, wchar_t const* infName, wchar_t const* modelSection, wchar_t const* archOs = 0);
        std::wstring getDeviceInstanceId(HDEVINFO hDevInfo, SP_DEVINFO_DATA& deviceInfoData);
        void addHwids(deviceInsertResult_t& deviceInsertResult, wbuffer_t const& hwIds);
        bool findHardwareId(wchar_t const* deviceClass, wchar_t const* id);
        void archOsToMajMinArch(std::wstring const& archOs, int& maj, int& min, int& arch);
        hardwareIdInstallSectionInfName_t::iterator getBestHardwareIdMatch(std::wstring const& findId);
        void getHardwareIdsToInstall(hardwareIdInstallSectionInfName_t& hwIdsToInstall);
        void getServicesToStartOnBoot(hardwareIdInstallSectionInfName_t& hwIdsToInstall);
        bool setDeviceServiceToStartOnBoot(hardwareIdInstallSectionInfName_t& hwIdsToInstall);
        bool getHklmControlSetNames(hklmControlSetNames_t& hklmControlSetNames);
        bool fullDeviceInstall(hardwareIdInstallSectionInfName_t& hwIdsToInstall);
        void installSection(std::wstring const& section, hklmControlSetNames_t& hklmControlSetNames);
        void processSection(std::wstring const& section, wchar_t const* hkrReplacement, hklmControlSetNames_t& hklmControlSetNames);
        void addInterface(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void addPowerSetting(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void addProperty(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void addReg(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void addService(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void bitReg(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void copyFiles(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void copyINF(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void delFiles(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void delProperty(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void delReg(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void delService(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void driverVer(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void featureScore(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void hardwareId(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void ini2Reg(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void logConfig(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void profileItems(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void registerDlls(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void renFiles(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void unregisterDlls(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void updateIniFields(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void updateInis(INFCONTEXT& infContext, wchar_t const* hkrReplacement);
        void offregUpdate(ORHKEY key, wbuffer_t const& subKey, wbuffer_t const& valueName, DWORD flags, DWORD regType, wbuffer_t const& value, DWORD valueLen);
        void updateRegistry(wchar_t const* hkrReplacement, wbuffer_t const& rootKey, wbuffer_t const& subKey, wbuffer_t const& valueName, DWORD flags, DWORD regType, wbuffer_t const& value, DWORD len);
        void addRegUpdateRegistry(wchar_t const* addRegSection, wchar_t const* hkrReplacement);
        bool getStringField(INFCONTEXT& infContext, DWORD idx, wbuffer_t& buffer, DWORD& returnedSize);
        bool getMultiStringField(INFCONTEXT& infContext, DWORD idx, wbuffer_t& buffer, DWORD& returnedSize);
        bool getBinaryField(INFCONTEXT& infContext, DWORD idx, wbuffer_t& buffer, DWORD& returnedSize);
        DWORD hexCast(wchar_t const* hexStr);

    private:
        ORHKEY m_systemHiveKey;
        ORHKEY m_softwareHiveKey;

        HINF m_hInf;

        OsTypes m_osType;

        classDeviceHardwareIds_t m_classDeviceHardwareIds;

        hardwareIdInstallSectionInfName_t m_hardwareIdInstallSectionInfName;

        infDirectiveNameAndFunction_t m_infDirectiveNameAndFunction;

        idAndService_t m_idAndService;

        ERROR_RESULT m_errorResult;
    };

} //namesapce HostToTarget

#endif // INFDEVICEINSTALLER_H
