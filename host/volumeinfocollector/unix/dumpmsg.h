//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dumpmsg.h
// 
// Description: 
// 

#ifndef DUMPMSG_H
#define DUMPMSG_H

#include <string>

void DumpMsgInit(bool toStdout);

void DumpMsg(std::string const & msg);

#endif // ifndef DUMPMSG_H
