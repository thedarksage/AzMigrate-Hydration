//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : utilfdsafeclose.cpp
// 
// Description:  so that guards can be setup with out worrying about trying to close
// an fd that has not been open

#include <unistd.h>

int fdSafeClose(int& fd)
{
    int rc = 0;
    if (-1 != fd) {
        rc = close(fd);
        fd = -1;
    }
    return rc;
}

