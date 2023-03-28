
///
/// \file unix/vmwareupdatermajor.h
///
/// \brief vmware updater implementation major declerations for windows
///

#ifndef VMWAREUPDATERMAJOR_H
#define VMWAREUPDATERMAJOR_H

#include <string>

#include "vmwareupdater.h"
#include "errorexception.h"

namespace HostToTarget {

    /// \brief class for updating target target when performing host-to-target
    class VmwareUpdaterMajor : public VmwareUpdater {
    public:

        /// \ brief constructor
        ///
        /// \exception throws ERROR_EXCEPTION on failure to construct
        explicit VmwareUpdaterMajor(HostToTargetInfo const& info)
            : VmwareUpdater(info)
            {
            }

        /// \ desctructor
        virtual ~VmwareUpdaterMajor()
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

#endif // VMWAREUPDATERMAJOR_H
