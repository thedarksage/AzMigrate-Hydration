
///
/// \file inmstatus.h
///
/// \brief inmage return type
/// 

#ifndef INMSTATUS__H
#define INMSTATUS__H

#include <string>
#include "inmtypes.h"

#define INM_SUCCESS  0
#define INM_FAILURE -1

struct inm_status_t
{
	
    inm_status_t(const inm_32_t & rv = INM_SUCCESS,
        const inm_32_t & errnum = 0,
        const std::string & err = "")
	{
        m_rv = rv;
        m_errno = errnum;
        m_error = err;
    }

    bool succeeded()
    {
        return (INM_SUCCESS == m_rv);
    }

    bool failed()
    {
        return !succeeded();
    }

    bool pfailed(inm_32_t & errnum, std::string & err)
    {
        errnum = m_errno;
        err = m_error;
        return !succeeded();
    }

    ///
    /// \brief return value from the function
    ///    on success rv is zero
    ///    on failure, rv is set to a negative value

    inm_32_t m_rv; 
	
    ///
    /// \brief error code as returned by os system call or library routine
    ///

    inm_32_t m_errno;
	
    ///
    /// \brief error string for logging
    ///

    std::string m_error;

};

#endif
