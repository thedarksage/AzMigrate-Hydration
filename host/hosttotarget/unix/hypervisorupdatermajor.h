
///
/// \file unix/hypervisorupdatermajor.h
///
/// \brief hypervisor updater implementation major declerations for windows
///

#ifndef HYPERVISORUPDATERMAJOR_H
#define HYPERVISORUPDATERMAJOR_H

#include <string>

#include "hypervisorupdater.h"
#include "errorexception.h"

namespace HostToTarget {

    /// \brief class for updating target target when performing host-to-target
    class HypervisorUpdaterMajor : public HypervisorUpdater {
    public:

        /// \ brief constructor
        ///
        /// \exception throws ERROR_EXCEPTION on failure to construct
        explicit HypervisorUpdaterMajor(HostToTargetInfo const& info)
            : HypervisorUpdater(info)
            {
            }

        /// \ desctructor
        virtual ~HypervisorUpdaterMajor()
            {
            }


        /// \brief validates that everything needed is available
        ///
        /// \return bool where
        /// \li true indicates passed validation
        /// \li false indicates did not pass validation. getError or getErrorAsString for specific failure
        virtual bool validate()
            {
                return true;
            }
        
        /// \brief performs the needed updates
        ///
        /// \return bool where
        /// \li true indicates success
        /// \li false indicates error. Use getError or getErrorAsString for specific failure
        virtual bool update(int opt);

    protected:

    private:

    };

} // namespace HostToTarget

#endif // HYPERVISORUPDATERMAJOR_H
