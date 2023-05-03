
///
/// \file win/hosttotargetmajor.h
///
/// \brief Hot to Target major delcerations for windows
///

#ifndef HOSTTOTARGETMAJOR_H
#define HOSTTOTARGETMAJOR_H

#include <windows.h>

#include "hosttotarget.h"

namespace HostToTarget {
    
    ERR_TYPE const ERROR_OK(ERROR_SUCCESS);                  ///< no errors

    /// \brief holds OS information needed for updating the target
    /// that is specific to a given OS
    struct SupportedOsTypeAndOptionalDriverFileInfo {
        int m_osType;                   ///< OS type
        char* m_storageDriverFile;      ///< storage driver file name that needs to be "installed". can be NULL
        char* m_storageDriverFileTgt;   ///< storage driver path to install driver file under. can be NULL
    };


    std::string errorToString(ERR_TYPE err);
    
    long setPrivilege(char const* privilege);
    
} //namesapce HostToTarget

#endif // HOSTTOTARGETMAJOR_H
