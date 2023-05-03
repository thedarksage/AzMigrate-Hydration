//#pragma once

#ifndef RUNNABLE__H
#define RUNNABLE__H

#include <list>

#include "ace/Thread.h"
//#include "ace/OS_NS_Thread.h"
//#include "ace/config-macros.h"


typedef ACE_THR_FUNC_RETURN THREAD_FUNC_RETURN;
typedef ACE_THR_FUNC THREAD_FUNC;

struct Runnable
{
public:
    virtual THREAD_FUNC_RETURN Run()      = 0;
    virtual THREAD_FUNC_RETURN Stop(const long int PARAM=0) = 0;
};

typedef std::list<Runnable*> RUNNABLE_LIST;

#endif

