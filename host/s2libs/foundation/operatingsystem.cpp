/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : operatingsystem.cpp
 *
 * Description: 
 */
#include <string>
#include <ostream>
#include <istream>
#include <sstream>

#include <ace/OS_NS_errno.h>
#include <ace/os_include/os_limits.h>
#include <ace/OS_NS_sys_utsname.h>

#include "entity.h"
#include "synchronize.h"
#include "error.h"
#include "portable.h"

#include "operatingsystem.h"

Object OperatingSystem::m_OsInfo;
OS_VAL OperatingSystem::m_OsVal = OS_UNKNOWN;
ENDIANNESS OperatingSystem::m_Endianness = ENDIANNESS_UNKNOWN;

OperatingSystem* OperatingSystem::theOS = NULL;
Lockable OperatingSystem::m_CreateLock;

/*
 * FUNCTION NAME : OperatingSystem
 *
 * DESCRIPTION : constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES : No-op
 *
 * return value : 
 *
 */
OperatingSystem::OperatingSystem()
{

}

/*
 * FUNCTION NAME : OperatingSystem
 *
 * DESCRIPTION : copy constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES : No-op
 *
 * return value : 
 *
 */
OperatingSystem::OperatingSystem(const OperatingSystem &right)
{

}

/*
 * FUNCTION NAME : ~OperatingSystem
 *
 * DESCRIPTION : Destructor
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
OperatingSystem::~OperatingSystem()
{
}

/*
 * FUNCTION NAME : Reset
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
void OperatingSystem::Reset()
{
	m_OsInfo.Reset();
	m_OsVal = OS_UNKNOWN;
    m_Endianness = ENDIANNESS_UNKNOWN;
}

/*
 * FUNCTION NAME : GetInstance
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
OperatingSystem& OperatingSystem::GetInstance()
{
    /* commented entry and exit since getting printed 
     * many times */
	/* DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME); */

    if (NULL == theOS)
    {
        AutoLock CreateGuard(m_CreateLock);
        if (NULL == theOS)
        {
            theOS = new OperatingSystem;
            theOS->Init();
        }
    }
	/* DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME); */

	return *theOS;
	
}

const Object & OperatingSystem::GetOsInfo(void) const
{
    return m_OsInfo;
}


void OperatingSystem::SetEndianness(void)
{
    unsigned int i = 1;
    unsigned char *byte = (unsigned char *)&i;

    m_Endianness = ENDIANNESS_BIG;
    if (*byte == 1)
    {
        m_Endianness = ENDIANNESS_LITTLE;
    }
}


/*
 * FUNCTION NAME : Destroy
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
bool OperatingSystem::Destroy()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    AutoLock CreateGuard(m_CreateLock);
    if ( NULL != theOS)
    {
        delete theOS;
        theOS = NULL;
        Reset();
    }

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return true;
}


/*
 * FUNCTION NAME : Init
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
int OperatingSystem::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_bInitialized = true;

    std::string errmsg;
    int iStatus = SetOsInfo(errmsg);

	if (SV_SUCCESS != iStatus)
    {
        m_bInitialized = false;
        DebugPrintf(SV_LOG_ERROR, "Failed to acquire OS information with error %s\n", errmsg.c_str());
    }

    return iStatus;
}


int OperatingSystem::SetOsInfo(std::string &errmsg)
{
	int iStatus = SetOsSpecificInfo(errmsg);
	if (SV_SUCCESS == iStatus)
	{
		iStatus = SetOsCommonInfo(errmsg);
	}

	if (SV_SUCCESS == iStatus)
	{
        DebugPrintf(SV_LOG_DEBUG, "OS_VAL = %d, endianness = %d\n", m_OsVal, m_Endianness);
        DebugPrintf(SV_LOG_DEBUG, "osinfo:\n");
		m_OsInfo.Print();
	}

	return iStatus;
}


int OperatingSystem::SetOsCommonInfo(std::string &errmsg)
{
	int iStatus = SV_SUCCESS;

	SetEndianness();
    if (m_OsVal == OS_UNKNOWN)
	{
		ACE_utsname utsName;
		if (ACE_OS::uname(&utsName) >= 0)
		{
			std::string name = utsName.sysname;
			if (name == "HP-UX")
			{
				m_OsVal = SV_HPUX_OS;
			}
		}
		else
		{
			iStatus = SV_FAILURE;
			errmsg = "ACE_OS::uname failed with error ";
			errmsg += ACE_OS::strerror(ACE_OS::last_error());
		}
	}

	return iStatus;
}


/*
 * FUNCTION NAME : operator=
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
OperatingSystem & OperatingSystem::operator=(const OperatingSystem &right)
{
    return *this;
}


/*
 * FUNCTION NAME : operator==
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
int OperatingSystem::operator==(const OperatingSystem &right) const
{
    return 0;
}

/*
 * FUNCTION NAME : operator!=
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
int OperatingSystem::operator!=(const OperatingSystem &right) const
{
    return 0;
}


/*
 * FUNCTION NAME : operator<
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
int OperatingSystem::operator<(const OperatingSystem &right) const
{
    return 0;

}

/*
 * FUNCTION NAME : operator>
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
int OperatingSystem::operator>(const OperatingSystem &right) const
{
    return 0;

}

/*
 * FUNCTION NAME : operator<=
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
int OperatingSystem::operator<=(const OperatingSystem &right) const
{
    return 0;

}

/*
 * FUNCTION NAME : operator>=
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
int OperatingSystem::operator>=(const OperatingSystem &right) const
{
    return 0;
}


/*
 * FUNCTION NAME : operator<<
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
std::ostream & operator<<(std::ostream &stream,const OperatingSystem &right)
{
    return stream;
}

/*
 * FUNCTION NAME : operator>>
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
std::istream & operator>>(std::istream &stream,OperatingSystem &object)
{
    return stream;
}


const ENDIANNESS & OperatingSystem::GetEndianness(void) const
{
    return m_Endianness;
}

const OS_VAL & OperatingSystem::GetOsVal(void) const
{
	return m_OsVal;
}
