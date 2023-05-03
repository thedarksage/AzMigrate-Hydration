
///
/// \file unix/hosttotargetmajor.cpp
///
/// \brief host to target major implemenation for unix
///

#include <sstream>
#include <string>

#include "hosttotargetmajor.h"

namespace HostToTarget {

    std::string errorToString(ERR_TYPE err)
    {
        char buffer[255];
#ifdef XOPEN_SOURCE
        if (-1 == ::strerror_r(err, buffer, sizeof(buffer))) {
            std::stringstream str;
            str << "failed to get message for error code: " << err;
            return str.str();
        }
        return std::string(buffer);
#else
        return std::string(::strerror_r(err, buffer, sizeof(buffer)));
#endif
    }

    OsTypes osNameToOsType(std::string const& osName)
    {
        return OS_TYPE_NOT_SUPPORTED;
    }
    
    OsTypes osMajMinArchToOsType(std::string const& osMajorVersion, std::string const& osMinorVersion, std::string const& arch, bool serverVesrion)
    {
        return OS_TYPE_NOT_SUPPORTED;
    }

    std::string toVolumeNameFormat(std::string const& systemVolume)
    {
        // FIXME:
        return systemVolume;
    }

    std::string getWindowsDir(std::string const& systemVolume, std::string const& windowsDir)
    {
        return std::string(); // FIXME: should not neeeed this for non-windows
    }

} // namespace HosttoTarget
