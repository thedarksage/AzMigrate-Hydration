
///
/// \file unix/hypervisorupdatermajor.cpp
///
/// \brief targt updater major implementation for unix
///

#include <boost/algorithm/string.hpp>

#include "scopeguard.h"
#include "hypervisorupdatermajor.h"

namespace HostToTarget {

    bool HypervisorUpdaterMajor::update(int opt)
    {
        setError(ERROR_INTERNAL_NOT_IMPLEMENTED);
        return false;
    }

} // namesapce HostToTarget


