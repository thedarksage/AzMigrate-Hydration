
///
/// \file registryupdater.h
///
/// \brief
///

#ifndef REGISTRYUPDATER_H
#define REGISTRYUPDATER_H

#include <windows.h>
#include <string>
#include <vector>

#include "hosttotarget.h"
#include "hosttotargetmajor.h"

namespace HostToTarget {

    typedef std::vector<BYTE> binary_t;

    class RegistryUpdater {
    public:
        RegistryUpdater()
            : m_internalError(ERROR_OK),
              m_systemError(ERROR_OK),
              m_regDeleteKeyEx((RegDeleteKeyEx_t)0),
              m_regDeleteKey((RegDeleteKey_t)0),
              m_registrySystemControlSetsKeyValue(0)
            {
                loadAdvapiAddresses(); // FIXME: should report if this fails but OK for now as other checks prevent issues if this failed
            }

        /// \brief holds os specific registry values
        struct RegistryOsSpecific {
            int m_osType;   ///< set to all the OsTypes (they can be or'd together) that use this os specific info
            char* m_value;  ///< os specfic resgitsty value. Note this can be key name or data for a value
        };

        /// \brief holds registry key and value info needed for updating the registry
        struct RegistryKeyValue {
            int m_targetType;
            RegistryOsSpecific m_key[OS_TYPES_COUNT];
            char* m_name;
            DWORD m_type;
            RegistryOsSpecific m_data[OS_TYPES_COUNT];
        };

        /// \brief holds registry keys that need to be deleted
        struct RegistryKeyDelete {
            int m_targetType;
            RegistryOsSpecific m_key[OS_TYPES_COUNT];
        };

        /// \brief performs all requires registry updates
        bool update(std::string const& systemVolume, std::string const& windowsDir, TargetTypes targetType, OsTypes osType, int opts);

        /// \brief get internal error code
        ERR_TYPE getInternalError()
            {
                return m_internalError;
            }

        /// \brief get system error code
        ERR_TYPE getSystemError()
            {
                return m_systemError;
            }

        /// \brief get system error message
        std::string getInternalErrorMsg()
            {
                return m_errMsg;
            }

        /// \brief checks if there are any errors
        bool ok()
            {
                return (ERROR_OK == m_internalError && ERROR_OK == m_systemError);
            }

        /// \brief clears internal and system errors
        void clearErrors()
            {
                m_internalError = ERROR_OK;
                m_systemError = ERROR_OK;
                m_errMsg.clear();
            }

    protected:
        /// \brief sets current errors
        void setError(ERR_TYPE internalErr,                     ///< internal error
                      ERR_TYPE systemErr = ERROR_OK,            ///< optional system error
                      std::string const& errMsg = std::string() ///< optional additional error message
                      )
            {
                m_internalError = internalErr;
                m_systemError = systemErr;
                m_errMsg = errMsg;
            }

        /// \brief sets regitry key value for the system registry based on scsi controller found and/or os
        bool setRegistrySystemKeyValue(TargetTypes targetType, OsTypes osType);

        bool setLsiRegistrySystemKeyValue(OsTypes osType);

        /// \brief sets to the default scsi settings for the specific os
        bool setDefaultLsi(int osType);

        /// \brief updates the system hive
        bool updateSystemHive(std::string const& systemVolume, TargetTypes targetType, OsTypes osType, bool setServicesToManualStart);

        /// \brief updates all the control sets found in the system hive
        bool updateControlSets(TargetTypes targetType, OsTypes osType, bool setServicesToManualStart);

        /// \brief updates the specific control set found in the system hive
        bool updateControlSet(char const* controlSetName, TargetTypes targetType, OsTypes osType);

        /// \brief updates the software hive
        bool updateSoftwareHive(std::string const& systemVolume, std::string const& systemDir, TargetTypes targetType, OsTypes osType, int opts);

        /// \brief disables Shuttdown Event Tracker
        bool disableShutdownEventTracker(std::string const& systemVolume, bool setErrors);

        /// \brief sets up a RunOnce entry to reset Shutdown Event Tracker to on
        bool resetShutdownEventTracker(std::string const& systemVolume, bool deleteEntry, bool setErrors);

        /// \brief disables windows error recovery and sets up to reset it if needed after boot
        bool disableWindowsErrorRecovery(std::string const& systemVolume, OsTypes osType, bool setErrors);

        // \brief returns the full name to bcd file if found else emtpy string
        std::string getBcdFileName(std::string const& systemVolume);

        /// \brief creates a bat file with the batScript
        bool createBatFile(std::string const& batName, std::string const& batScript, bool setErrors);

        /// \brief sets RunOnce registry setting to execute the given cmd
        bool setRunOnce(char const* name, std::string const& cmd, bool setErrors);

        /// \brief creates script and RunOnce setting to reset boot status policy
        bool setResetBootStatusPolicyRunOnce(std::string const& systemVolume, bool setErrors);

        /// \brief creates script and RunOnce setting to bring vhd disks online
        bool setVhdOnlineRunOnce(std::string const& systemVolume, TargetTypes targetType, OsTypes osType, bool setErrors);

        /// \brief holds RegDeletKeyExA proc address loaded at run time if OS supports it as 32-bit needs
        /// to delete from both 32-bit and 64-bit registry
        typedef LONG (WINAPI *RegDeleteKeyEx_t)(HKEY hKey, LPCTSTR lpSubKey, REGSAM samDesired, DWORD Reserved); // useed on 64-bit OS

        /// \brief holds RegDeletKeyA proc address loaded at run time if RegDeleteExA is not supported otherwise will be 0
        /// this is really only on 32 bit OS which does not need the Ex version
        typedef LONG (WINAPI *RegDeleteKey_t)(HKEY hKey, LPCTSTR lpSubKey);

        /// \brief loads the proc address for the given procName if found in the module.
        ///
        /// \return procAddress for the typename T
        /// \exception throws the error returned by GetLastError
        template <typename T>
        T getProcAddress(HMODULE module, LPCSTR procName)
            {
                T procAddress = reinterpret_cast<T>(GetProcAddress(module, procName));
                if (0 == procAddress) {
                    throw GetLastError();
                }
                return procAddress;
            }

        /// \brief loads the proc address needed from avdapi32.dll that depend on the running OS
        ///
        /// \returns
        /// \li ERROR_SUCCESS (0) on success
        /// \li error return by GetLastError() on failure
        DWORD loadAdvapiAddresses();

        /// \brief converts (hex) char to its binary value
        ///
        /// (e.g. '1' becomes 0x1, 'a' becomes 0xa.
        /// Chars not between '0' - '9' nor 'a' - 'f' (lower or upper case) are converted to 0x0
        BYTE convertHexNumberCharToBinary(char c);

        /// \brief converts hex number string to actual binary numbers string
        ///
        /// e.g. "1234567890abcdef" is converted to 0x1234567890abcdef
        /// Chars not between '0' - '9' nor 'a' - 'f' (lower or upper case) are converted to 0x0
        void convertHexNumberStringToBinary(std::string const& src, binary_t& binary);

    private:
        ERR_TYPE m_internalError;  ///< track internal error
        ERR_TYPE m_systemError;    ///< track system error (note there can be internal error but no system error)
        std::string m_errMsg;      ///< additional error info when available. can be empty even if there are errors.

        HMODULE m_advapi32Dll; ///< handle to the loaded advapi32 dll

        RegDeleteKeyEx_t m_regDeleteKeyEx; ///< holds proc address for RegDeleteExA function (0 if load fails)

        RegDeleteKey_t m_regDeleteKey; ///< holds proc address for RegDeleteA function (0 if load fails)

        RegistryKeyValue* m_registrySystemControlSetsKeyValue;
    };

} // namespace HostToTarget

#endif // REGISTRYUPDATER_H
