
///
/// \file win/targetupdaterimpmajor.h
///
/// \brief
///

#ifndef TARGETUPDATERIMPMAJOR_H
#define TARGETUPDATERIMPMAJOR_H

#include <windows.h>
#include <offreg.h>
#include <map>
#include <string>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "targetupdaterimp.h"
#include "infdeviceinstaller.h"

namespace HostToTarget {

    class TargetUpdaterImpMajor : public TargetUpdaterImp {
    public:
        explicit TargetUpdaterImpMajor(HostToTargetInfo const& info)
            :  TargetUpdaterImp(info),
               m_systemHiveKey(0),
               m_softwareHiveKey(0)
            {
                m_systemHiveName.assign(getHostToTargetInfo().m_windowsDir.begin(), getHostToTargetInfo().m_windowsDir.end());
                m_systemHiveName += L"system32\\config\\system";
                m_softwareHiveName.assign(getHostToTargetInfo().m_windowsDir.begin(), getHostToTargetInfo().m_windowsDir.end());
                m_softwareHiveName += L"system32\\config\\software";
            }

        ~TargetUpdaterImpMajor()
            {
                if (0 != m_systemHiveKey) {
                    ORCloseHive(m_systemHiveKey);
                }
                if (0 != m_softwareHiveKey) {
                    ORCloseHive(m_softwareHiveKey);
                }
            }

        virtual bool validate();

        virtual bool update(int opts);

    protected:
        typedef std::map<std::string, std::string> renameFiles_t;

        bool validateVmwareNeededFilesExist(std::string const& installPath, std::string& driverFiles);
        bool cleanOldDrivers();
        bool deleteSubkey(ORHKEY key, wchar_t const* subkey);
        bool updateTcpIpSettings(bool reportErrors);
        bool setDhcpSettings(ORHKEY orKey, bool resportErrors);
        bool renameHalFiles(renameFiles_t const& renameFiles);
        bool updateHal();
        bool replaceHalFiles(std::string const& cab, bool x64);
        void getApicAndAcpiSettings(bool& apic, bool& acpi);
        bool addOutOfTheBoxDrivers();
        bool addOutOfTheBoxDriversDevicePath();
        bool addOutOfTheBoxDriversDriverStore();
        bool appendDevicePath(std::string const& driverPath);
        bool copyOutOfTheBoxDriversRecursive(boost::filesystem::path const& sourceDir, boost::filesystem::path const& targetDir);
        bool copyOutOfTheBoxDrivers(boost::filesystem::path const& targetDir);
        std::wstring getProductConfigDir(bool setErrors);
        bool clearConfigSettings(bool setErrors);
        bool updateWellKnownServices(bool setErrors);
        bool clearInvolfltSettings(wchar_t const* controlSetKeyName, bool setErrors);
        bool setWellKnownServicesToManualStart(wchar_t const* controlSetKeyName, bool setErrors);
        bool openHives();
        bool saveHives();
        bool saveHive(ORHKEY& orKey, std::wstring const& hiveName);
        bool renameHive(std::wstring const& fromHive, std::wstring const& toHIve);
        bool backupHive(std::wstring const& hive);
        bool backupHives();
        bool adjustConfigPath(boost::filesystem::wpath& configPath, bool setErrors);
        bool updateVmware(int opts);
        bool updateHyperv(int opts);
        bool updatePhysical(int opts);
        bool installDriversIfNeeded();
        int updateDeviceServiceStartSettingIfNeeded();  // returns -1: error, 0: updated registry, 1: nothing done

        bool okToCleanOldDriversForOsType()
            {
                switch (getHostToTargetInfo().m_osType) {
                    case OS_TYPE_VISTA_32:
                    case OS_TYPE_VISTA_64:
                    case OS_TYPE_W2K8_32:
                    case OS_TYPE_W2K8_64:
                    case OS_TYPE_W2K8_R2:
                    case OS_TYPE_WIN7:
                    case OS_TYPE_W2K12:
                    case OS_TYPE_WIN8:
                        return true;
                    default:
                        return false;
                }
                return false;
            }

    private:
        std::wstring m_systemHiveName;
        std::wstring m_softwareHiveName;

        ORHKEY m_systemHiveKey;
        ORHKEY m_softwareHiveKey;

    };


} // namesapce HostToTarget

#endif // TARGETUPDATERIMPMAJOR_H
