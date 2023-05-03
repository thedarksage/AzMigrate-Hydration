
///
/// \file win/vmwareupdatermajor.h
///
/// \brief vmware updater implementation major declerations for windows
///

#ifndef VMWAREUPDATERMAJOR_H
#define VMWAREUPDATERMAJOR_H

#include <windows.h>
#include <string>
#include <offreg.h>

#include "errorexception.h"
#include "registryupdater.h"
#include "errorresult.h"

namespace HostToTarget {

    /// \brief class for updating target target when performing host-to-target
    class VmwareUpdaterMajor {
    public:
        
        /// \ brief constructor
        ///
        /// \exception throws ERROR_EXCEPTION on failure to construct
        explicit VmwareUpdaterMajor(HostToTargetInfo const& info)
            : m_info(info)
            {
                // system volume required for all windows
                if (info.m_systemVolume.empty()) {
                    throw ERROR_EXCEPTION << "missing system volume target name";
                }
            }

        /// \ desctructor
        virtual ~VmwareUpdaterMajor()
            {
            }

        /// \brief validates that everything needed is available
        ///
        /// \return ERROR_RESULT
        /// \li ERROR_RESULT::m_err == ERROR_OK success
        /// \li ERROR_RESULT::m_err != ERROR_OK failed 
        ERROR_RESULT validate();

        /// \brief performs the needed updates
        ///
        /// \return ERROR_RESULT
        /// \li ERROR_RESULT::m_err == ERROR_OK success
        /// \li ERROR_RESULT::m_err != ERROR_OK failed 
        ERROR_RESULT update(int opts, ORHKEY hiveKey);

    protected:
        

    private:
        RegistryUpdater m_registryUpdater;  ///< used to update the registry

        HostToTargetInfo m_info; ///< HostToTargteInfo
    };

} // namespace HostToTarget

#endif // VMWAREUPDATERMAJOR_H
