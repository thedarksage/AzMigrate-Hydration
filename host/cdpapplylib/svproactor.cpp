//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :svproactor.cpp
//
// Description: 
//



#ifdef SV_WINDOWS
#include "ace/WIN32_Proactor.h"
#else
#include "ace/POSIX_Proactor.h"
#endif


#include "svproactor.h"
#include "cdputil.h"

int SVProactorTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

int SVProactorTask::close(u_long flags)
{
    return 0;
}


void SVProactorTask::delete_proactor (void)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> locker (mutex_);
  if (--threads_ == 0)
    {
      ACE_Proactor::instance ((ACE_Proactor *) 0);
      delete proactor_;
      proactor_ = 0;
    }
}


int SVProactorTask::svc (void)
{

    create_proactor ();
    CDPUtil::disable_signal (ACE_SIGRTMIN, ACE_SIGRTMAX);

    while (ACE_Proactor::event_loop_done () == 0)
        ACE_Proactor::run_event_loop ();

    delete_proactor ();
    return 0;
}
