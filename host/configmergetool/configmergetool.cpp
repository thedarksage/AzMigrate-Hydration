// configmerge.cpp : Defines the entry point for the console application.
//

#include "configmerge.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

int main(int argc, char* argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	configmerge merge(argc,argv);
	if(!merge.run())
		return -1;
	return 0;
}

