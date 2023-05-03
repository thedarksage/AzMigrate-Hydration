//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : sendquit.cpp
// 
// Description: used to send quit events to agent processes that are waiting on
//              ACE named process events for quit request
// 

#include <string>

#include <ace/OS_main.h>                        
#include <ace/Manual_Event.h>

#include "event.h"

std::string const AgentQuitEventPrefix(AGENT_QUITEVENT_PREFIX);

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
    if (argc != 2) {
        return -1;
    }
            
    std::string eventName(AgentQuitEventPrefix + argv[1]);
    
    ACE_Manual_Event quitEvent(0, USYNC_PROCESS, eventName.c_str());
    
    quitEvent.signal();
        
    return 0;		
}


