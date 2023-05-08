
///
/// \file targetupdaterfactory.cpp
///
/// \brief target updater factory imp[lementation
///

#include "targetupdaterfactory.h"
#include "vmwareupdatermajor.h"
#include "hypervisorupdatermajor.h"

namespace HostToTarget {

    TargetUpdaterImp::ptr TargetUpdaterFactory(HostToTargetInfo const& info)
    {
        // host os name is required by all
        if (OS_TYPE_NOT_SUPPORTED == info.m_osType) {
            throw ERROR_EXCEPTION << "missing host os name";
        }

        switch (info.m_targetType) {
            case HOST_TO_VMWARE:
                return TargetUpdaterImp::ptr(new VmwareUpdaterMajor(info));

            case HOST_TO_XEN:
                throw ERROR_EXCEPTION << "HOST_TO_XEN not implemented";

            case HOST_TO_HYPERVISOR:
                return TargetUpdaterImp::ptr(new HypervisorUpdaterMajor(info));

            case HOST_TO_PHYSICAL:
                throw ERROR_EXCEPTION << "HOST_TO_PHYSCIAL not implemented";

            default:
                throw ERROR_EXCEPTION << "unknown target type " << info.m_targetType;
        }
    }

} // namespace HostToTarget
