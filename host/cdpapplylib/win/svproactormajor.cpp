//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :svproactormajor.cpp
//
// Description: 
//


#include <ace/WIN32_Proactor.h>

#include "svproactor.h"
#include "cdputil.h"


void SVProactorTask::create_proactor (void)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker (mutex_);

    if (threads_ == 0)
    {
        ACE_WIN32_Proactor *proactor = new ACE_WIN32_Proactor;
        proactor_ = new ACE_Proactor (proactor, 1);
        ACE_Proactor::instance(proactor_);
        event_.signal ();
    }

    threads_++;
}
