//                                       
// Copyright (c) 2008 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : main.cpp
// 
// Description: 
//


#include <ace/Init_ACE.h>

#include "cdpmgr.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"


int main(int argc, char ** argv)
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	// PR#10815: Long Path support
	ACE::init();

	//__asm int 3;
#ifdef SV_WINDOWS
	// PR #2094
	// SetErrorMode controls  whether the process will handle specified 
	// type of errors or whether the system will handle them
	// by creating a dialog box and notifying the user.
	// In our case, we want all errors to be sent to the program.
	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif

	CDPMgr cdpmgr(argc, argv);
	return cdpmgr.run();
}

