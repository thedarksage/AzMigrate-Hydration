//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdplock.cpp
//
// Description: 
//

#include "cdplock.h"
#include "cdputil.h"
#include "portable.h"
#include "error.h"
#include "localconfigurator.h"
#include "portablehelpersmajor.h"

using namespace std;

#define  REMOVE_ON_EXIT 0

CDPLock::CDPLock(const string & lockName, bool convertflag, const std::string & extn)
{
	LocalConfigurator localConfigurator;
	m_name = lockName;
	m_lockName = lockName;

	// If we are trying to get lock on volume
	// Convert the volume name to a standard format
	if(convertflag)
	{
		FormatVolumeName(m_lockName);

		// If we are trying to get lock on device
		// convert to standard device name if symbolic link was passed
		// as argument

		if (IsReportingRealNameToCx())
		{
			GetDeviceNameFromSymLink(m_lockName);
		}

		replace_nonsupported_chars(m_lockName);

		string agentPath;
		agentPath += localConfigurator.getInstallPath();
		agentPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
		agentPath += m_lockName;
		m_lockName = agentPath;
	}

    if(!extn.empty())
    {
        m_lockName +=  extn;
    }

	DebugPrintf(SV_LOG_DEBUG, "Lock name is %s\n", m_lockName.c_str());
	// PR#10815: Long Path support
	m_Lock.reset(new ACE_File_Lock(getLongPathName(m_lockName.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS ,REMOVE_ON_EXIT));
	if(!m_Lock)
	{
		DebugPrintf(SV_LOG_ERROR,"%s: Lock (%s) creation  failed. %s\n", 
			FUNCTION_NAME, m_lockName.c_str(), Error::Msg().c_str());
	}
}

CDPLock::~CDPLock()
{
	release();
}

bool CDPLock::acquire_read()
{
	//__asm int 3;
	if(!m_Lock)
		return false;

	DebugPrintf(SV_LOG_INFO, "Acquiring read lock on %s. This may take sometime.\n", 
		m_name.c_str());

	if(m_Lock -> acquire_read() != -1)
		return true;

	return false;
}

bool CDPLock::acquire()
{
	//__asm int 3;
	if(!m_Lock)
		return false;

	DebugPrintf(SV_LOG_INFO, "Acquiring write lock on %s. This may take sometime.\n", 
		m_name.c_str());

	if(m_Lock -> acquire_write() != -1)
		return true;

	return false;
}

bool CDPLock::try_acquire()
{
	//__asm int 3;
	if(!m_Lock)
		return false;

	DebugPrintf(SV_LOG_DEBUG, "Acquiring write lock on %s.\n", 
		m_name.c_str());

	if(m_Lock -> tryacquire_write() != -1)
		return true;

	return false;
}

bool CDPLock::try_acquire_read()
{
	//__asm int 3;
	if(!m_Lock)
		return false;

	DebugPrintf(SV_LOG_DEBUG, "Acquiring read lock on %s.\n", 
		m_name.c_str());

	if(m_Lock -> tryacquire_read() != -1)
		return true;

	return false;
}



void CDPLock::release()
{
	if(0 != m_Lock.get())
		m_Lock -> release();

	m_Lock.reset();
}
