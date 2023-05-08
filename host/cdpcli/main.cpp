//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : main.cpp
//
// Description: 
//

#include <ace/Init_ACE.h>

#include "cdpcli.h"
#include "configurator2.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

Configurator* TheConfigurator = 0; // singleton

int main(int argc, char ** argv)
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis(); 
	// PR#10815: Long Path support
	ACE::init();

#ifdef SV_WINDOWS
	// PR #2094
	// SetErrorMode controls  whether the process will handle specified 
	// type of errors or whether the system will handle them
	// by creating a dialog box and notifying the user.
	// In our case, we want all errors to be sent to the program.
	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif

	CDPCli cli(argc, argv);

    // PR #6888
    // cdpcli should return 0 on success and 1 on failure
	if(cli.run())
    {
        return 0;
    } else
    {
        return 1;
    }
}





