//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dataprotectionsync.cpp
// 
// Description: 
//


#include <time.h>
#include "dataprotectionsync.h"

#define HUNDREDS_OF_NANOSEC_IN_SECOND 10000000LL

bool DataProtectionSync::GenerateResyncTimeIfReq(ResyncTimeSettings &rts)
{
    struct timespec ts;
    if (0 != clock_gettime(CLOCK_REALTIME, &ts))
    {
        fprintf(stderr, "clock_gettime failed\n");
        return false;
    }
    unsigned long long time_in_100nsec = 0;
    time_in_100nsec += (ts.tv_sec*HUNDREDS_OF_NANOSEC_IN_SECOND);
    time_in_100nsec += (ts.tv_nsec/100);
    rts.time = time_in_100nsec;
    rts.seqNo = 0;
    return true;
}

