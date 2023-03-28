
///
/// \file errorresult.h
///
/// \brief
///

#ifndef ERRORRESULT_H
#define ERRORRESULT_H

#include <string>

#include "hosttotargetmajor.h"

namespace HostToTarget {

    struct ERROR_RESULT {
        ERROR_RESULT(ERR_TYPE err, ERR_TYPE system = ERROR_OK, std::string msg = std::string())
            : m_err(err),
              m_system(system),
              m_msg(msg)
            {
            }
            
        ERR_TYPE m_err;
        ERR_TYPE m_system;
        std::string m_msg;
    };

} // namespace HostToTarget

#endif // ERRORRESULT_H
