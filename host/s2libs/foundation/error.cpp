/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : error.cpp
 *
 * Description: 
 */

#include <iostream>
#include <sstream>
#include <locale>

#include "synchronize.h"

#include "boost/shared_ptr.hpp"
#include "boost/bind.hpp"

#include "ace/OS.h"

#include "error.h"

/**********************************************************************************
*** Note: Please write any platform specific code in ${platform}/error_port.cpp ***
**********************************************************************************/

Lockable Error::m_MsgLock;
Lockable Error::m_CodeLock;

/*
 * FUNCTION NAME : Msg
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : error string for current error.
 *
 */
std::string Error::Msg()
{ 	
    std::stringstream message;
	
    int errorCode = Code();

	return Msg(errorCode);
}

/*
 * FUNCTION NAME : Code 
 *
 * DESCRIPTION : Sets the error code.
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
const void Error::Code(int iCode)
{
    AutoLock CodeGuard(m_CodeLock);
    ACE_OS::last_error(UserToOS(iCode));
}

/*
 * FUNCTION NAME : TrimEnd
 *
 * DESCRIPTION : remove non printable chars from the end of the buffer 
 *
 * INPUT PARAMETERS : char buffer
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void Error::TrimEnd(char* buf)
{
    if (NULL == buf) {
        return;
    }

    char* tmp = buf + strlen(buf) - 1;

    while (tmp != buf) {
        if (std::isprint(*tmp, std::cout.getloc())) {
            return;
        }
        *tmp = '\0';        
        --tmp;
    }
}
