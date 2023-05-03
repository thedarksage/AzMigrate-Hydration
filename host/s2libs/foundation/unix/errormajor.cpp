/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : error_port.cpp
 *
 * Description: Linux specific port of error.cpp
 */

#include <iostream>
#include <sstream>
#include <locale>
#include <errno.h>

#include "synchronize.h"

#include "boost/shared_ptr.hpp"
#include "boost/bind.hpp"

#include "ace/OS.h"
#include "portablehelpers.h"
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
        case EINVAL:
            return ERR_INVALID_PARAMETER;
            break;
        case EBUSY:
            return ERR_BUSY;
            break;
        case EAGAIN:
            return ERR_AGAIN;
            break;
        case ENODEV:
            return ERR_NODEV;
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
		case ERR_INVALID_PARAMETER:
			return EINVAL;
			break;
		case ERR_BUSY:
			return EBUSY;
			break;
		case ERR_AGAIN:
			return EAGAIN;
			break;
		case ERR_NODEV:
			return ENODEV;
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
    AutoLock MsgGuard(m_MsgLock);
    std::stringstream message;
    char * msgBuf = NULL;
    msgBuf = ACE_OS::strerror(iErrorCode);
    Error::TrimEnd(msgBuf);
    message << "Error No: " << iErrorCode << ", Error Msg: " << msgBuf;   
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

    int iError =  errno;
    return iError;
}

