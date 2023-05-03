//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dumpmsg.cpp
// 
// Description: 
// 

#include <iostream>
#include "dumpmsg.h"
#include "logger.h"
#include "portable.h"

static bool g_DumpToStdout = false;

void DumpMsgInit(bool toStdout)
{
    g_DumpToStdout = toStdout;
}

void DumpMsg(std::string const & msg)
{
    if (!msg.empty()) {
        if (g_DumpToStdout) {
            std::cout << msg;
        } else {
            DebugPrintf(SV_LOG_INFO, "%s", msg.c_str());
        }
    }
}

