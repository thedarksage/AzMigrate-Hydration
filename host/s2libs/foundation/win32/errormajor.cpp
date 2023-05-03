/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : error_port.cpp
 *
 * Description: Windows specific port of error.cpp
 */

#include <iostream>
#include <sstream>
#include <locale>

#include "synchronize.h"

#include "boost/shared_ptr.hpp"
#include "boost/bind.hpp"

#include "ace/OS.h"

#include "error.h"

/*
 * FUNCTION NAME : OSToUser
 *
 * DESCRIPTION : Converts the os error code to user error code.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const int Error::OSToUser(const int liError)
{
    switch(liError)
    {
        case ERROR_SUCCESS:
            return SV_SUCCESS;
            break;
        case ERROR_IO_PENDING:
            return ERR_IO_PENDING;
            break;
        case ERROR_MORE_DATA:
            return ERR_MORE_DATA;
            break;
        case ERROR_HANDLE_EOF:
            return ERR_HANDLE_EOF;
            break;
        case ERROR_INVALID_FUNCTION:
            return ERR_INVALID_FUNCTION;
            break;
        case ERROR_INVALID_PARAMETER:
            return ERR_INVALID_PARAMETER;
            break;
        case ERROR_INVALID_HANDLE:
            return ERR_INVALID_HANDLE;
            break;
        case ERROR_NOT_READY:
            return ERR_NOT_READY;
            break;
        case ERROR_BUSY:
            return ERR_BUSY;
            break;
        case ERROR_RETRY:
            return ERR_AGAIN;
            break;
        default:
            return liError;
            break;
    }

    return liError;
}


/*
 * FUNCTION NAME : UserToOS
 *
 * DESCRIPTION : Converts user error code to system error code.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const int Error::UserToOS(const int liError)
{
    switch(liError)
    {
        case SV_SUCCESS:
            return ERROR_SUCCESS;
            break;
        case ERR_IO_PENDING:
            return ERROR_IO_PENDING;
            break;
        case ERR_MORE_DATA:
            return ERROR_MORE_DATA;
            break;
        case ERR_HANDLE_EOF:
            return ERROR_HANDLE_EOF;
            break;
        case ERR_INVALID_FUNCTION:
            return ERROR_INVALID_FUNCTION;
            break;
        case ERR_INVALID_PARAMETER:
            return ERROR_INVALID_PARAMETER;
            break;
        case ERR_INVALID_HANDLE:
            return ERROR_INVALID_HANDLE;
            break;
        case ERR_NOT_READY:
            return ERROR_NOT_READY;
            break;
        case ERR_BUSY:
            return ERROR_BUSY;
            break;
        case ERR_AGAIN:
            return ERROR_RETRY;
            break;
        default:
            return liError;
            break;
    }

    return liError;
}

/*
 * FUNCTION NAME : Msg
 *
 * DESCRIPTION : returns error message string for an error code.
 *
 * INPUT PARAMETERS : Error code.
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
std::string Error::Msg(const int iErrorCode)
{
    std::stringstream message;

	message << "Error No: " << iErrorCode << ", Error Msg: ";
    char * msgBuf = NULL;
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                iErrorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&msgBuf,
                0, 
                NULL )) {
		/* from msdn manual page of FormatMessage:
		 * Return value
		 * If the function succeeds, the return value is the number of TCHARs stored in the output buffer, excluding the terminating null character.
		 * If the function fails, the return value is zero. To get extended error information, call GetLastError. */
		// make sure to free msgBuf even if operator<< below throws an error
	    boost::shared_ptr<void> guard((void*)NULL, boost::bind<HLOCAL>(::LocalFree, msgBuf));
		Error::TrimEnd(msgBuf);
		message << msgBuf;
	} else
		message << "Failed to get error message with error " << GetLastError();

    return message.str();
}

/*
 * FUNCTION NAME : Code
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const int Error::Code()
{
    AutoLock CodeGuard(m_CodeLock);
    return GetLastError();
}
