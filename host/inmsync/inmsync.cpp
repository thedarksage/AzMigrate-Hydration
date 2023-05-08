
///
/// \file inmsync.cpp
///
/// \brief
///

#include <iostream>

#include "cxpssync.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"


/// \brief main entry to inmsync
int main(int argc, char* argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    CxpsSync cxpsSync;
    if (!cxpsSync.run(argc, argv)) {
        std::cout << "FAILED: " << cxpsSync.getErrors() << '\n';
        cxpsSync.usage();
    }
    return cxpsSync.resultCode();
}
