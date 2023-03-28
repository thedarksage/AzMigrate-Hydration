
///
/// \file win/targetupdaterimpmajor.cpp
///
/// \brief
///

#include <intrin.h>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>

#include "scopeguard.h"
#include "targetupdaterimpmajor.h"
#include "vmwareupdatermajor.h"
#include "hypervisorupdatermajor.h"
#include "fiopipe.h"
#include "extractcabfiles.h"
#include "utils.h"
#include "infdeviceinstaller.h"

namespace HostToTarget {

    wchar_t * g_wellKnownServicesManualStart[] =
    {
        0
    };

    wchar_t * g_involfltSettingsToClear[] =
    {
        L"DataPoolSize",
        L"VolumeDataSizeLimit",
        0
    };

    struct ORHKeySimpleGuard {
        ORHKeySimpleGuard()
            : m_key(0)
            {
            }

        ORHKeySimpleGuard(ORHKEY key)
            : m_key(key)
            {
            }

        ~ORHKeySimpleGuard()
            {
                if (0 != m_key) {
                    ORCloseKey(m_key);
                }
            }

        void set(ORHKEY key)
            {
                if (0 != m_key) {
                    ORCloseKey(m_key);
                    m_key = 0;
                }
                m_key = key;
            }

        void dismiss()
            {
                m_key = 0;
            }

    private:
        ORHKEY m_key;
    };

    bool TargetUpdaterImpMajor::validate()
    {
        switch (getHostToTargetInfo().m_osType) {
            case OS_TYPE_XP_32:
            case OS_TYPE_XP_64:
            case OS_TYPE_W2K3_32:
            case OS_TYPE_W2K3_64:
            case OS_TYPE_VISTA_32:
            case OS_TYPE_VISTA_64:
            case OS_TYPE_W2K8_32:
            case OS_TYPE_W2K8_64:
            case OS_TYPE_W2K8_R2:
            case OS_TYPE_WIN7:
            case OS_TYPE_W2K12:
            case OS_TYPE_WIN8:
                break;
            default:
                setError(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED);
                return false;
        }
        if (HOST_TO_VMWARE == getHostToTargetInfo().m_targetType) {
            VmwareUpdaterMajor vmwareUpdater(getHostToTargetInfo());
            ERROR_RESULT er = vmwareUpdater.validate();
            if (ERROR_OK != er.m_err) {
                setError(er.m_err, er.m_system, er.m_msg);
                return false;
            }
        }
        // FIXME: make sure any other required files are available
        return true;
    }

    bool TargetUpdaterImpMajor::update(int opts)
    {
        // opts |= UPDATE_OPTION_CLEAN_OLD_DRIVERS;

        if (!validate()) {
            return false;
        }
        backupHives();
        if (!openHives()) {
            return false;
        }
        if (0 == (UPDATE_OPTION_INSTALL_DRIVERS_ONLY & opts)) {
            if (0 != (UPDATE_OPTION_CLEAN_OLD_DRIVERS & opts)
                || okToCleanOldDriversForOsType()) {
                // MAYBE: for now ignore errors cleaning old drivers
                // for the most part it should work fine, just might
                // end up with stuff loaded that is not needed
                //cleanOldDrivers();
            }
            if (!updateHal()) {
                return false;
            }
            if (!addOutOfTheBoxDrivers()) {
                return false;
            }
            updateTcpIpSettings(false);
            if (0 != (opts & UPDATE_OPTION_CLEAR_CONFIG_SETTINGS)) {
                clearConfigSettings(false); // for now ignore any errors clearing the settings
            }
            if (0 != (opts & UPDATE_OTPION_MANUAL_START_SERVICES)) {
                updateWellKnownServices(false); // for now do not set errors
            }
#if 1
            if (0 == (opts & UPDATE_OPTION_VM_DO_NOT_USE_INF) && 0 == updateDeviceServiceStartSettingIfNeeded()) {
                // did system  registry updates, only need to do software registy updates
                opts |= UPDATE_OPTION_REGISTRY_UPDATE_SOFTWARE_HIVE_ONLY;
            }
#endif
        }
        installDriversIfNeeded(); // TODO: once InfDeviceInstaller supports full install can remove this call
        if (!saveHives()) {
            return false;
        }
        if (0 == (UPDATE_OPTION_INSTALL_DRIVERS_ONLY & opts)) {
            if (HOST_TO_VMWARE == getHostToTargetInfo().m_targetType) {
                return updateVmware(opts);
            } else if (HOST_TO_HYPERVISOR == getHostToTargetInfo().m_targetType) {
                return updateHyperv(opts);
            }
            return updatePhysical(opts);
        }
        return true;
    }

    bool TargetUpdaterImpMajor::cleanOldDrivers()
    {
        std::vector<wchar_t> name(MAX_PATH);
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
                    std::wstring enumKey(&name[0]);
                    enumKey += L"\\Enum";
                    if (!deleteSubkey(m_systemHiveKey, enumKey.c_str())) {
                        return false;
                    }
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == rc);
        if (ERROR_NO_MORE_ITEMS != rc) {
            setError(ERROR_INTERNAL_REG_UPDATE, rc, "cleaning old drivers");
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::deleteSubkey(ORHKEY key, wchar_t const* subkey)
    {
        ORHKEY orKey;
        DWORD rc = OROpenKey(key, subkey, &orKey);
        if (ERROR_SUCCESS != rc) {
            if (ERROR_FILE_NOT_FOUND == rc) {
                return true; // does not exist so same as if it were deleted
            }
            setError(ERROR_INTERNAL_REG_OPEN_KEY, rc, subkey);
            return false;
        }
        std::vector<wchar_t> name(MAX_PATH);
        DWORD nameLength = (DWORD)name.size();;
        FILETIME lastWriteTime;
        do {
            do {
                name.resize(nameLength);
                rc = OREnumKey(orKey, 0, &name[0], &nameLength, 0, 0, &lastWriteTime);
            } while (ERROR_MORE_DATA == rc);
            if (ERROR_SUCCESS == rc) {
                bool delSubkeyRc = deleteSubkey(orKey, &name[0]);
                if (ERROR_SUCCESS != rc) {
                    ORCloseKey(orKey);
                    return delSubkeyRc;
                }
            }
        } while (ERROR_SUCCESS == rc);
        ORCloseKey(orKey);
        rc = ORDeleteKey(key, subkey);
        if (!(ERROR_SUCCESS == rc || ERROR_FILE_NOT_FOUND == rc)) {
            setError(ERROR_INTERNAL_REG_UPDATE, rc, subkey);
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::updateTcpIpSettings(bool reportErrors)
    {
        std::vector<wchar_t> name(MAX_PATH);
        DWORD nameLength = (DWORD)name.size();
        FILETIME lastWriteTime;
        DWORD idx = 0;
        DWORD rc;
        DWORD enumRc;
        do {
            do {
                name.resize(nameLength);
                enumRc = OREnumKey(m_systemHiveKey, idx, &name[0], &nameLength, 0, 0, &lastWriteTime);
            } while (ERROR_MORE_DATA == enumRc);
            if (ERROR_SUCCESS == enumRc) {
                if (boost::algorithm::istarts_with(&name[0], L"ControlSet")) {
                    std::wstring keyName(&name[0]);
                    keyName += L"\\services\\Tcpip\\Parameters\\Interfaces";
                    ORHKEY orKey;
                    rc = OROpenKey(m_systemHiveKey, keyName.c_str(), &orKey);
                    if (ERROR_SUCCESS == rc) {
                        setDhcpSettings(orKey, reportErrors);
                        ORCloseKey(orKey);
                    }
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == enumRc);
        if (ERROR_NO_MORE_ITEMS != enumRc) {
            if (reportErrors) {
                setError(ERROR_INTERNAL_REG_UPDATE, enumRc, "cleaning old drivers");
                return false;
            }
        }
        return true;
    }

    bool TargetUpdaterImpMajor::setDhcpSettings(ORHKEY orKey, bool reportErrors)
    {
        std::vector<wchar_t> name(MAX_PATH);
        DWORD nameLength = (DWORD)name.size();
        FILETIME lastWriteTime;
        DWORD idx = 0;
        DWORD enumRc;
        DWORD rc;
        do {
            do {
                name.resize(nameLength);
                enumRc = OREnumKey(orKey, idx, &name[0], &nameLength, 0, 0, &lastWriteTime);
            } while (ERROR_MORE_DATA == enumRc);
            if (ERROR_SUCCESS == enumRc) {
                std::wstring keyName(&name[0]);
                ORHKEY enumKey;
                rc = OROpenKey(orKey, keyName.c_str(), &enumKey);
                if (ERROR_SUCCESS == rc) {
                    DWORD type;
                    DWORD dhcp;
                    DWORD dataLength = sizeof(DWORD);
                    rc = ORGetValue(orKey, keyName.c_str(), L"EnableDHCP", &type, &dhcp, &dataLength);
                    if (ERROR_SUCCESS == rc) {
                        if (0 == dhcp) {
                            type = REG_DWORD;
                            dhcp = 1;
                            rc = ORSetValue(enumKey, L"EnableDHCP", type, (BYTE const*)&dhcp, sizeof(DWORD));
                        }
                        std::vector<wchar_t> data;
                        dataLength = MAX_PATH * sizeof(wchar_t);
                        do {
                            data.resize(dataLength);
                            rc = ORGetValue(orKey, keyName.c_str(), L"IpAddress", &type, &name[0], &dataLength);
                        } while (ERROR_MORE_DATA == rc);
                        if (ERROR_SUCCESS == rc) {
                            type = REG_MULTI_SZ;
                            rc = ORSetValue(enumKey, L"IpAddress", type, (BYTE const*)L"\0", 4);
                        }
                    }
                    ORCloseKey(enumKey);
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == enumRc);
        if (ERROR_NO_MORE_ITEMS != enumRc) {
            if (reportErrors) {
                setError(ERROR_INTERNAL_REG_UPDATE, enumRc, "cleaning old drivers");
                return false;
            }
        }
        return true;
    }

    void TargetUpdaterImpMajor::getApicAndAcpiSettings(bool& apic, bool& acpi)
    {
        int cpuInfo[4];
        int infoType = 1;
        __cpuid(cpuInfo, infoType); // see windows __cpuid documentation for details
        apic = ((cpuInfo[3] & 0x00000200) > 0); // bit 9
        // HACK: for now assume acpi is OK. As it seems that windows is using ACPI hal
        // even on systems where it is not set in cpuInfo[3]. If this proves to be an issue
        // then will have to firgure out  how windows is determing ACPI. Note a very old
        // knowledge base for w2k states that it uses 2 lists bad bios which will not get
        // ACPI hal, if bios is not on that list and its date is > 1/1/99 it will use ACPI hal.
        // If bios date < 1/1/99 it checks a good bios list and if on that list sets ACPI
        // otherwise does not. Pretty sure all users of this softwre will have bios dates > 1/1/99
        // so assume ACPI is OK. This will only be an issue of some of those bioses are on the bad list
        acpi = true; // ((cpuInfo[3] & 0x00400000) > 0); // bit 22
    }

    bool TargetUpdaterImpMajor::renameHalFiles(renameFiles_t const& renameFiles)
    {
        renameFiles_t::const_iterator iter(renameFiles.begin());
        renameFiles_t::const_iterator iterEnd(renameFiles.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            try {
                boost::filesystem::remove((*iter).second);
                boost::filesystem::rename((*iter).first, (*iter).second);
            } catch (...) {
            }
        }
        return true;
    }

    bool TargetUpdaterImpMajor::replaceHalFiles(std::string const& cab, bool x64)
    {
        renameFiles_t renameFiles;
        CabExtractInfo cabInfo;
        cabInfo.m_name = cab;
        if (!getCabFilePath(getHostToTargetInfo().m_systemVolume, cab, x64, cabInfo.m_path)) {
            return false;
        }
        cabInfo.m_destination = getHostToTargetInfo().m_systemVolume;
        cabInfo.m_destination += "tmpcabextract\\";
        boost::filesystem::remove_all(cabInfo.m_destination);
        boost::filesystem::create_directory(cabInfo.m_destination);

        std::string halDestination(getHostToTargetInfo().m_systemVolume);
        halDestination += "windows\\system32\\";

        std::string halFile;
        bool mp = boost::thread::hardware_concurrency() > 1;
        bool apic = false;
        bool acpi = false;
        getApicAndAcpiSettings(apic, acpi);
        if (mp) {
            if (acpi) {
                // "ACPI Multiprocessor PC," ACPI APIC MP HAL (Halmacpi.dll)
                cabInfo.m_extractFiles.insert("halmacpi.dll");
                renameFiles.insert(std::make_pair(cabInfo.m_destination + "halmacpi.dll", halDestination + "hal.dll"));
            } else {
                // "MPS Multiprocessor PC," Non-ACPI APIC MP HAL (Halmps.dll)
                cabInfo.m_extractFiles.insert("halmps.dll");
                renameFiles.insert(std::make_pair(cabInfo.m_destination + "halmps.dll", halDestination + "hal.dll"));
            }
        } else {
            // uniprocessor
            if (acpi) {
                if (apic) {
                    // "ACPI Uniprocessor PC," ACPI APIC UP HAL (Halaacpi.dll)
                    cabInfo.m_extractFiles.insert("halaacpi.dll");
                    renameFiles.insert(std::make_pair(cabInfo.m_destination + "halaacpi.dll", halDestination + "hal.dll"));
                } else {
                    // "Advanced Configuration and Power Interface (ACPI) PC," ACPI PIC HAL (Halacpi.dll)
                    cabInfo.m_extractFiles.insert("halacpi.dll");
                    renameFiles.insert(std::make_pair(cabInfo.m_destination + "halacpi.dll", halDestination + "hal.dll"));
                }
            } else {
                if (apic) {
                    // "MPS Uniprocessor PC," Non-ACPI APIC UP HA (halapic.dll)
                    cabInfo.m_extractFiles.insert("halapic.dll");
                    renameFiles.insert(std::make_pair(cabInfo.m_destination + "halapic.dll", halDestination + "hal.dll"));
                } else {
                    // Standard PC," Non-ACPI PIC HAL (Hal.dll)
                    cabInfo.m_extractFiles.insert("hal.dll");
                    renameFiles.insert(std::make_pair(cabInfo.m_destination + "hal.dll", halDestination + "hal.dll"));
                }
            }
        }

        if (mp) {
            // extract (expand.exe) ntkrpamp.exe and copy to targetVolume\windows\system32\ntkrnlpa.exe")
            cabInfo.m_extractFiles.insert("ntkrpamp.exe");
            renameFiles.insert(std::make_pair(cabInfo.m_destination + "ntkrpamp.exe", halDestination + "ntkrnlpa.exe"));
            // extract (expand.exe) ntkrnlmp.exe and copy to targetVolume\windows\wywtem32\ntoskrnl.exe")
            cabInfo.m_extractFiles.insert("ntkrnlmp.exe");
            renameFiles.insert(std::make_pair(cabInfo.m_destination + "ntkrnlmp.exe", halDestination + "ntoskrnl.exe"));
        } else {
            cabInfo.m_extractFiles.insert("ntkrnlpa.exe");
            renameFiles.insert(std::make_pair(cabInfo.m_destination + "ntkrnlpa.exe", halDestination + "ntkrnlpa.exe"));
            cabInfo.m_extractFiles.insert("ntoskrnl.exe");
            renameFiles.insert(std::make_pair(cabInfo.m_destination + "ntoskrnl.exe", halDestination + "ntoskrnl.exe"));
        }
        extractCabFiles(&cabInfo);
        renameHalFiles(renameFiles);
        boost::filesystem::remove_all(cabInfo.m_destination);
        return true;
    }

    bool TargetUpdaterImpMajor::updateHal()
    {
        bool x64 = false;
        switch (getHostToTargetInfo().m_osType) {
            case OS_TYPE_XP_32:
            case OS_TYPE_W2K3_32:
                break;
            case OS_TYPE_XP_64:
                x64 = true;
                // need to update hal
                break;
            case OS_TYPE_W2K3_64:
            case OS_TYPE_VISTA_32:
            case OS_TYPE_VISTA_64:
            case OS_TYPE_W2K8_32:
            case OS_TYPE_W2K8_64:
            case OS_TYPE_W2K8_R2:
            case OS_TYPE_WIN7:
            case OS_TYPE_W2K12:
            case OS_TYPE_WIN8:
                // should not need to update hal
                // if for some reason it is needed, then should only need
                // to copy the correct ntkrnlpa.exe and ntoskrnl.exe
                // but could validate that only hal.dll exists in the .cab file
                setError(ERROR_OK, ERROR_SUCCESS);
                return true;
            default:
                setError(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED, ERROR_SUCCESS, osTypeToStr(getHostToTargetInfo().m_osType));
                return false;
        }
        std::wstring csdVersion;
        if (!getCsdVersion(m_softwareHiveKey, csdVersion)) {
            return false;
        }
        if (boost::algorithm::icontains(csdVersion, L"service pack")) {
            int spNumber = getSpNumber(csdVersion);
            while (spNumber > 0) {
                std::string spFile("sp");
                spFile += boost::lexical_cast<std::string>(spNumber);
                spFile += ".cab";
                if (replaceHalFiles(spFile, x64)) {
                    setError(ERROR_OK, ERROR_SUCCESS);
                    return true;
                }
                --spNumber;
            }
        }
        if (replaceHalFiles("driver.cab", x64)) {
            setError(ERROR_OK, ERROR_SUCCESS);
            return true;
        }
        setError(ERROR_INTERNAL_UPDATE_HAL, ERROR_SUCCESS);
        return false;
    }

    bool TargetUpdaterImpMajor::addOutOfTheBoxDrivers()
    {
        try {
            if (getHostToTargetInfo().m_outOfTheBoxDrivers.empty()) {
                return true;
            }
            switch (getHostToTargetInfo().m_osType) {
                case OS_TYPE_XP_32:
                case OS_TYPE_W2K3_32:
                case OS_TYPE_XP_64:
                case OS_TYPE_W2K3_64:
                case OS_TYPE_W2K8_32:
                case OS_TYPE_W2K8_64:
                case OS_TYPE_W2K8_R2:
                case OS_TYPE_WIN7:
                case OS_TYPE_W2K12:
                case OS_TYPE_WIN8:
                    return addOutOfTheBoxDriversDevicePath();
                case OS_TYPE_VISTA_32:
                case OS_TYPE_VISTA_64:
                    return addOutOfTheBoxDriversDriverStore();
                default:
                    setError(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED, ERROR_SUCCESS, osTypeToStr(getHostToTargetInfo().m_osType));
            }
        } catch (std::exception const& e) {
            setError(ERROR_INTERNAL_OTB_COPY, ERROR_SUCCESS, e.what());
        }
        return false;
    }

    bool TargetUpdaterImpMajor::addOutOfTheBoxDriversDevicePath()
    {
        std::string driverDir(getHostToTargetInfo().m_systemVolume);
        driverDir += "tmpDrivers";
        if (!copyOutOfTheBoxDrivers(driverDir)) {
            return false;
        }
        return appendDevicePath(driverDir);
    }

    bool TargetUpdaterImpMajor::addOutOfTheBoxDriversDriverStore()
    {
        // just copy files to systemdir\windows\system32\DriverStore
        std::string driverDir(getHostToTargetInfo().m_systemVolume);
        driverDir += "windows\\system32\\DriverStore\\";
        return copyOutOfTheBoxDrivers(driverDir);
    }

    bool TargetUpdaterImpMajor::appendDevicePath(std::string const& driverPath)
    {
        ORHKEY orKey;
        DWORD rc = OROpenKey(m_softwareHiveKey,  L"Microsoft\\Windows\\CurrentVersion", &orKey);
        if (ERROR_SUCCESS != rc) {
            setError(ERROR_INTERNAL_REG_OPEN_KEY, rc, "Microsoft\\Windows\\CurrentVersion");
            return false;
        }
        ON_BLOCK_EXIT(&ORCloseKey, orKey);

        std::vector<wchar_t> data;
        DWORD dataLength = MAX_PATH * sizeof(wchar_t);
        DWORD type;
        do {
            data.resize(dataLength);
            rc = ORGetValue(m_softwareHiveKey, L"Microsoft\\Windows\\CurrentVersion", L"DevicePath", &type, &data[0], &dataLength);
        } while (ERROR_MORE_DATA == rc);
        if (ERROR_SUCCESS != rc) {
            if (ERROR_FILE_NOT_FOUND != rc) {
                setError(ERROR_INTERNAL_REG_GET_VALUE, rc, "Microsoft\\Windows\\CurrentVersion\\DevicePath");
                return false;
            }
            dataLength = 0;
        }
        std::wstring path;
        if (dataLength > 0) {
            // device path had data need to make sure to keep it
            path = &data[0];
            path += L';';
        }
        path.append(driverPath.begin(), driverPath.end());
        rc = ORSetValue(orKey, L"DevicePath", type, (BYTE const*)path.c_str(), ((DWORD)path.size() + 1) * sizeof(wchar_t));
        if (ERROR_SUCCESS != rc) {
            setError(ERROR_INTERNAL_OTB_APPEND_DEVICEPATH, rc, "Microsoft\\Windows\\CurrentVersion\\DevicePath");
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::copyOutOfTheBoxDriversRecursive(boost::filesystem::path const& sourceDir, boost::filesystem::path const& targetDir)
    {
        boost::filesystem::create_directory(targetDir);
        boost::filesystem::directory_iterator dirIterEnd;
        boost::filesystem::directory_iterator dirIter(sourceDir);
        for (/* empty */; dirIter != dirIterEnd; ++dirIter) {
            boost::filesystem::path tDir(targetDir);
            tDir /= (*dirIter).path().filename();
            if (boost::filesystem::is_directory((*dirIter).status())) {
                copyOutOfTheBoxDriversRecursive((*dirIter).path(), tDir);
            } else {
                boost::filesystem::copy_file((*dirIter).path(), tDir, boost::filesystem::copy_option::overwrite_if_exists);
            }
        }
        return true;
    }

    bool TargetUpdaterImpMajor::copyOutOfTheBoxDrivers(boost::filesystem::path const& targetDir)
    {
        bool rc = true;
        boost::char_separator<char> sep(";");
        boost::tokenizer<boost::char_separator<char> > tokens(getHostToTargetInfo().m_outOfTheBoxDrivers, sep);
        boost::tokenizer<boost::char_separator<char> >::iterator iter(tokens.begin());
        boost::tokenizer<boost::char_separator<char> >::iterator iterEnd(tokens.end());
        for (/* empty*/; iter != iterEnd; ++iter) {
            boost::filesystem::path sourceDir(*iter);
            // make sure source  and target are not the same and that target is not subdir of source
            if (STARTS_WITH(targetDir.file_string(), sourceDir.file_string()) || sourceDir == targetDir) {
                continue;
            }
            rc = (rc && copyOutOfTheBoxDriversRecursive(sourceDir, targetDir));
        }
        return true;
    }

    std::wstring TargetUpdaterImpMajor::getProductConfigDir(bool setErrors)
    {
        DWORD type;
        DWORD rc;
        std::vector<wchar_t> data(1024);
        DWORD dataLength = (DWORD)data.size();
        do {
            data.resize(dataLength);
            rc = ORGetValue(m_softwareHiveKey, L"SV Systems\\VxAgent", L"ConfigPathname", &type, &data[0], &dataLength);
        } while (ERROR_MORE_DATA == rc);
        if (ERROR_SUCCESS != rc) {
            do {
                data.resize(dataLength);
                rc = ORGetValue(m_softwareHiveKey, L"Wow6432Node\\SV Systems\\VxAgent", L"ConfigPathname", &type, &data[0], &dataLength);
            } while (ERROR_MORE_DATA == rc);
            if (ERROR_SUCCESS != rc) {
                setError(ERROR_INTERNAL_REG_GET_VALUE, rc, "SV Systems\\VxAgent\\ConfigPathname");
                return false;
            }
        }
        return std::wstring(&data[0]);
    }

    bool TargetUpdaterImpMajor::adjustConfigPath(boost::filesystem::wpath& configPath, bool setErrors)
    {
        // first check the origianl configPath hoping the origianl drive letter was assgined
        try {
            if (boost::filesystem::exists(configPath)) {
                return true;
            }
        } catch (...) {
        }

        // next replace original dirve letter with target drive letter
        HostToTargetInfo& hostInfo = getHostToTargetInfo();
        wchar_t systemDrive[4];
        systemDrive[0] = hostInfo.m_systemVolume[0];
        systemDrive[1] = ':';
        systemDrive[2] = '\\';
        systemDrive[3] = '\0';
        boost::filesystem::wpath path(systemDrive);
        path /= configPath.relative_path();
        try {
            if (boost::filesystem::exists(path)) {
                configPath = path;
                return true;
            }
        } catch (...) {
        }

        // not found, need to check all disks (except current system disk)
        wchar_t systemDir[MAX_PATH];
        if (!GetSystemDirectoryW(systemDir, sizeof(systemDir) / sizeof(wchar_t))) {
            if (setErrors) {
                setError(ERROR_INTERNAL_CONFIG_FILE_NOT_FOUND, GetLastError());
            }
            return false;
        }
        systemDrive[0] = systemDir[0];
        wchar_t winpeVolume[MAX_PATH];
        if (!GetVolumeNameForVolumeMountPointW(systemDrive, winpeVolume, sizeof(winpeVolume) / sizeof(wchar_t))) {
            if (setErrors) {
                setError(ERROR_INTERNAL_CONFIG_FILE_NOT_FOUND, GetLastError());
            }
            return false;
        }
        wchar_t name[MAX_PATH];
        HANDLE h = FindFirstVolumeW(name, sizeof(name) / sizeof(wchar_t));
        if (INVALID_HANDLE_VALUE == h) {
            if (setErrors) {
                setError(ERROR_INTERNAL_CONFIG_FILE_NOT_FOUND, GetLastError());
            }
            return false;
        }
        ON_BLOCK_EXIT(&FindVolumeClose, h);
        if (!boost::algorithm::iequals(name, winpeVolume)) {
            unsigned int driveType = GetDriveTypeW(name);
            if (DRIVE_FIXED == driveType) {
                path = name;
                path /= configPath.relative_path();
                try {
                    if (boost::filesystem::exists(path)) {
                        configPath = path;
                        return true;
                    }
                } catch (...) {
                }
            }
        }
        while (FindNextVolumeW(h, name, sizeof(name) / sizeof(wchar_t))) {
            if (!boost::algorithm::iequals(name, winpeVolume)) {
                unsigned int driveType = GetDriveTypeW(name);
                if (DRIVE_FIXED == driveType) {
                    path = name;
                    path /= configPath.relative_path();
                    try {
                        if (boost::filesystem::exists(path)) {
                            configPath = path;
                            return true;
                        }
                    } catch (...) {
                    }
                }
            }
        }
        if (setErrors) {
            setError(ERROR_INTERNAL_CONFIG_FILE_NOT_FOUND);
        }
        return false;
    }

    bool TargetUpdaterImpMajor::clearConfigSettings(bool setErrors)
    {
        boost::filesystem::wpath configPath(getProductConfigDir(setErrors));
        if (configPath.empty()) {
            return false;
        }
        if (!adjustConfigPath(configPath, setErrors)) {
            return false;
        }
        boost::filesystem::wpath bakConfigPath(configPath);
        try {
            std::wstringstream ext;
            ext << L"conf.bak" << time(0);
            bakConfigPath.replace_extension(ext.str());
            boost::filesystem::rename(configPath, bakConfigPath);
        } catch (...) {
            if (setErrors) {
                setError(ERROR_INTERNAL_CLEAR_CONFIG_SETTINGS);
            }
            return false;
        }

        std::ifstream bakConfigFile(bakConfigPath.file_string().c_str());
        std::ofstream configFile(configPath.file_string().c_str());
        if (!configFile.good() || !bakConfigFile.good()) {
            if (setErrors) {
                setError(ERROR_INTERNAL_CLEAR_CONFIG_SETTINGS);
            }
            return false;
        }
        bool foundConfigStoreLocation = false;
        bool foundHostId = false;
        std::string line;
        while (true) {
            std::getline(bakConfigFile, line);
            if (line.empty()) {
                if (bakConfigFile.eof() && configFile.good()) {
                    return true;
                } else {
                    if (setErrors) {
                        setError(ERROR_INTERNAL_CLEAR_CONFIG_SETTINGS);
                    }
                    return false;
                }
            }
            if (!foundConfigStoreLocation && boost::algorithm::istarts_with(line, "ConfigStoreLocation")) {
                configFile << "ConfigStoreLocation=\n";
                foundConfigStoreLocation = true;
            } else if (!foundHostId && boost::algorithm::istarts_with(line, "HostId")) {
                configFile << "HostId=\n";
                foundHostId = true;
            } else {
                configFile << line << '\n';
            }
        }
        return false; // should never get here
    }

    bool TargetUpdaterImpMajor::updateWellKnownServices(bool setErrors)
    {
        bool ok = false;
        std::vector<wchar_t> name(MAX_PATH);
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
                    ok &= setWellKnownServicesToManualStart(&name[0], setErrors);
                    ok &= clearInvolfltSettings(&name[0], setErrors);
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == rc);
        if (ERROR_NO_MORE_ITEMS != rc) {
            setError(ERROR_INTERNAL_REG_UPDATE, rc);
            return false;
        }
        return ok;
    }

    bool TargetUpdaterImpMajor::clearInvolfltSettings(wchar_t const* controlSetKeyName, bool setErrors)
    {
        ORHKeySimpleGuard regkeyGuard;
        ORHKEY key = 0;
        bool ok = true;
        std::wstring keyName(controlSetKeyName);
        keyName += L"\\services\\involflt\\Parameters";
        long result = OROpenKey(m_systemHiveKey, keyName.c_str(), &key);
        if (ERROR_SUCCESS != result) {
            if (ERROR_FILE_NOT_FOUND == result) {
                return true; // service not in this control set
            }
            if (setErrors) {
                setError(ERROR_INTERNAL_SERVICE_MANUAL_START, result);
            }
            return false;
        }
        regkeyGuard.set(key);
        for (int i = 0; 0 != g_involfltSettingsToClear[i]; ++i) {
            DWORD data = 0;
            DWORD rc = ORSetValue(key, g_involfltSettingsToClear[i], REG_DWORD,  (BYTE const*)&data, sizeof(DWORD));
            if (!(ERROR_SUCCESS == rc || ERROR_FILE_NOT_FOUND == rc)) {
                if (setErrors) {
                    setError(ERROR_INTERNAL_SERVICE_MANUAL_START, rc);
                }
                ok = false;
            }
        }
        return ok;
    }

    bool TargetUpdaterImpMajor::setWellKnownServicesToManualStart(wchar_t const* controlSetKeyName, bool setErrors)
    {
        bool ok = true;
        ORHKeySimpleGuard regkeyGuard;
        ORHKEY key = 0;
        for (int i = 0; 0 != g_wellKnownServicesManualStart[i]; ++i) {
            std::wstring keyName(controlSetKeyName);
            keyName += L"\\services\\";
            keyName += g_wellKnownServicesManualStart[i];
            long result = OROpenKey(m_systemHiveKey, keyName.c_str(), &key);
            if (ERROR_SUCCESS != result) {
                if (ERROR_FILE_NOT_FOUND == result) {
                    continue; // service not in this control move on to next service
                }
                if (setErrors) {
                    setError(ERROR_INTERNAL_SERVICE_MANUAL_START, result);
                }
                ok = false;
                continue;
            }
            regkeyGuard.set(key);
            DWORD data = 3;
            DWORD rc = ORSetValue(key, L"Start", REG_DWORD,  (BYTE const*)&data, sizeof(DWORD));
            if (!(ERROR_SUCCESS == rc || ERROR_FILE_NOT_FOUND == rc)) {
                if (setErrors) {
                    setError(ERROR_INTERNAL_SERVICE_MANUAL_START, rc);
                }
                ok = false;
            }
        }
        return ok;
    }

    bool TargetUpdaterImpMajor::openHives()
    {
        // system hive
        DWORD rc = OROpenHive(m_systemHiveName.c_str(), &m_systemHiveKey);
        if (ERROR_SUCCESS != rc) {
            setError(ERROR_INTERNAL_LOAD_HIVE, rc, m_systemHiveName);
            return false;
        }

        // software hive
        rc = OROpenHive(m_softwareHiveName.c_str(), &m_softwareHiveKey);
        if (ERROR_SUCCESS != rc) {
            setError(ERROR_INTERNAL_LOAD_HIVE, rc, m_softwareHiveName);
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::saveHives()
    {
        if (!saveHive(m_systemHiveKey, m_systemHiveName)) {
            return false;
        }
        return saveHive(m_softwareHiveKey, m_softwareHiveName);
    }

    bool TargetUpdaterImpMajor::saveHive(ORHKEY& orKey, std::wstring const& hiveName)
    {
        std::wstring tmpName(hiveName);
        tmpName += L".tmp";
        boost::filesystem::remove(tmpName);
        DWORD rc = ORSaveHive(orKey, tmpName.c_str(), 5, 2);
        if (ERROR_SUCCESS != rc) {
            setError(ERROR_INTERNAL_SAVE_HIVE, rc, tmpName);
            return false;
        }
        ORCloseHive(orKey);
        orKey = 0;
        return renameHive(tmpName, hiveName);
    }

    bool TargetUpdaterImpMajor::renameHive(std::wstring const& fromHive, std::wstring const& toHive)
    {
        std::string msg;
        try {
            if (boost::filesystem::remove(toHive)) {
                boost::filesystem::rename(fromHive, toHive);
                return true;
            }
            msg += "trying remove ";
            msg.append(toHive.begin(), toHive.end());
            msg += " for rename";
        } catch (std::exception const& e) {
            msg = e.what();
        } catch (...) {
            msg = "rename ";
            msg.append(fromHive.begin(), fromHive.end());
            msg += " to ";
            msg.append(toHive.begin(), toHive.end());
            msg += " unknown exception";
        }
        setError(ERROR_INTERNAL_RENAME_HIVE, ERROR_SUCCESS, msg);
        return false;
    }

    bool TargetUpdaterImpMajor::backupHive(std::wstring const& hive)
    {
        std::wstring hiveBackup(hive + L".backup");
        try {
            boost::filesystem::copy_file(boost::filesystem::wpath(hive), boost::filesystem::wpath(hiveBackup), boost::filesystem::copy_option::overwrite_if_exists);
        } catch (std::exception const& e) {
            setError(ERROR_INTERNAL_BACKUP_HIVE, GetLastError(), e.what());
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::backupHives()
    {
        if (!backupHive(m_systemHiveName)) {
            return false;
        }
        return backupHive(m_softwareHiveName);
    }

    bool TargetUpdaterImpMajor::installDriversIfNeeded()
    {
        extractFiles_t driverFilesToInstall;
        switch (getHostToTargetInfo().m_targetType) {
            case HOST_TO_VMWARE:
                driverFilesToInstall.insert("symmpi.sys");
                break;
            case HOST_TO_HYPERVISOR:
                driverFilesToInstall.insert("intelide.sys");
                driverFilesToInstall.insert("vmbus.sys");
                driverFilesToInstall.insert("strovsc.sys");
                break;
            default:
                return true; // target does not need drivers or we do not know what drivers it needes ;-)
        }
        bool x64 = false;
        switch (getHostToTargetInfo().m_osType) {
            case HostToTarget::OS_TYPE_XP_32:
            case HostToTarget::OS_TYPE_W2K3_32:
                x64 = false;
                break;
            case HostToTarget::OS_TYPE_XP_64:
            case HostToTarget::OS_TYPE_W2K3_64:
                x64 = true;
                break;
            default:
                return true; // os does not need drivers or we do not know what drivers it needs ;-)
        }
        extractFiles_t::iterator iter(driverFilesToInstall.begin());
        extractFiles_t::iterator iterEnd(driverFilesToInstall.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            ERROR_RESULT er = copyDriverFile(getHostToTargetInfo(), m_softwareHiveKey, *iter, x64);
            if (ERROR_OK != er.m_err) {
                setError(er.m_err, er.m_system, er.m_msg);
                return false;
            }
            return true;
        }
        return true;
    }

    int TargetUpdaterImpMajor::updateDeviceServiceStartSettingIfNeeded()
    {
        std::string infDir(getHostToTargetInfo().m_windowsDir);
        infDir += "inf";
        InfDeviceInstaller infDeviceInstaller;
        ERROR_RESULT er = infDeviceInstaller.install(m_systemHiveKey, m_softwareHiveKey, infDir.c_str(), getHostToTargetInfo().m_osType);
        if (ERROR_OK != er.m_err) {
            setError(er.m_err, er.m_system, er.m_msg);
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::updateVmware(int opts)
    {
        VmwareUpdaterMajor vmwareUpdater(getHostToTargetInfo());
        ERROR_RESULT er = vmwareUpdater.update(opts, m_softwareHiveKey);
        if (ERROR_OK != er.m_err) {
            setError(er.m_err, er.m_system, er.m_msg);
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::updateHyperv(int opts)
    {
        HypervisorUpdaterMajor hypervisorUpdater(getHostToTargetInfo());
        ERROR_RESULT er = hypervisorUpdater.update(opts, m_softwareHiveKey);
        if (ERROR_OK != er.m_err) {
            setError(er.m_err, er.m_system, er.m_msg);
            return false;
        }
        return true;
    }

    bool TargetUpdaterImpMajor::updatePhysical(int opts)
    {
        RegistryUpdater registryUpdater;
        ERROR_RESULT er = registryUpdater.update(getHostToTargetInfo().m_systemVolume,
                                                 getHostToTargetInfo().m_windowsDir,
                                                 HOST_TO_PHYSICAL,
                                                 getHostToTargetInfo().m_osType,
                                                 opts | UPDATE_OPTION_REGISTRY_UPDATE_SOFTWARE_HIVE_ONLY);
        if (ERROR_OK != er.m_err) {
            setError(er.m_err, er.m_system, er.m_msg);
            return false;
        }
        return true;
    }

} // namesapce HostToTarget
