//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :svproactor.h
//
// Description: 
//

#ifndef SVPROACTOR__H
#define SVPROACTOR__H

#include <ace/Proactor.h>
#include <ace/Task.h>
#include <ace/Manual_Event.h>

class SVProactorTask : public ACE_Task <ACE_MT_SYNCH>
{

public:

    SVProactorTask (ACE_Thread_Manager* tm)
        : threads_ (0), proactor_ (0), ACE_Task<ACE_MT_SYNCH>(tm) {};

    virtual int open(void* args = 0);
    virtual int close(u_long flags =0);
    virtual int svc (void);
    void waitready (void) { event_.wait (); }

protected:

private:

    ACE_Recursive_Thread_Mutex mutex_;
    int threads_;
    ACE_Proactor *proactor_;
    ACE_Manual_Event event_;

    void create_proactor (void);
    void delete_proactor (void); 
};


#endif
