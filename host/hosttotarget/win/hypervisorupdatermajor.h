
///
/// \file win/hypervisorupdatermajor.h
///
/// \brief
///

#ifndef HYPERVISORUPDATERMAJOR_H
#define HYPERVISORUPDATERMAJOR_H


#include <windows.h>
#include <string>
#include <offreg.h>

#include "hypervisorupdater.h"
#include "errorexception.h"
#include "registryupdater.h"
#include "errorresult.h"

namespace HostToTarget {

    /// \brief class for updating target target when performing host-to-target
    class HypervisorUpdaterMajor {
    public:

        /// \ brief constructor
        ///
        /// \exception throws ERROR_EXCEPTION on failure to construct
        explicit HypervisorUpdaterMajor(HostToTargetInfo const& info)
            : m_info(info)
            {
                // system volume required for all windows
                if (info.m_systemVolume.empty()) {
                    throw ERROR_EXCEPTION << "missing system volume target name";
                }
            }

        /// \ desctructor
        ~HypervisorUpdaterMajor()
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
        HostToTargetInfo m_info; ///< HostToTargteInfo

        RegistryUpdater m_registryUpdater;

    };

} // namespace HostToTarget


#endif // HYPERVISORUPDATERMAJOR_H
