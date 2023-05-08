//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdplock.h
//
// Description: 
//

#ifndef CDPLOCK__H
#define CDPLOCK__H

#include <ace/File_Lock.h>
#include <string>

#include "svtypes.h"
#include "boost/shared_ptr.hpp"

/**
* 1 TO N sync: Need to find appropriate place for this
*/
#define DP_LOCK_NAME_SEP "_"

class CDPLock
{
public:

    CDPLock(const std::string& lockName, bool convertflag = true, const std::string & extn = std::string());
    ~CDPLock();

    bool acquire();
    bool acquire_read();
	bool try_acquire();
	bool try_acquire_read();
    void release();

    typedef boost::shared_ptr<CDPLock> Ptr;
protected:

private:

    CDPLock(const CDPLock &);
    CDPLock& operator=(const CDPLock&);

    std::string m_lockName;
    std::string m_name;
    boost::shared_ptr<ACE_File_Lock> m_Lock;
};

#endif


