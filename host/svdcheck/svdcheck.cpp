#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sstream>
#include "portable.h"
#include "validatesvdfile.h"
#include "compatibility.h"
#include "logger.h"
#include "configurator2.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

Configurator* TheConfigurator = 0; // singleton

/* displays the usage of svdcheck */
void DispUsage(const std::string &progname);

/* validates and fills in the block size for statistics (IO freq and overlap) */
bool IsBlockSizeFilled(const char *bsize, stCmdLineAndPrintOpts_t *pclineandpropts);

// Main function
int main (int argc, char* argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    int result = 0;
    Checksums_t* checksums = NULL;
    stCmdLineAndPrintOpts_t clineandpropts; 
    bool bcanprocess = true;
    std::string progname = *argv;
    SetLogLevel(SV_LOG_DISABLE);

    clineandpropts.m_ullWrUnitLen = WRITELENGTH_UNIT;
    clineandpropts.m_ullOverlapLen = OVERLAP_LENGTH;
    clineandpropts.m_bShouldPrint = true;
    if (argc > 1)
    {
        char c;
        while ((bcanprocess) && --argc > 0 && (*++argv)[0] == '-')
        {
            bool breakout = false;
            while (!breakout && (c = *++argv[0]))
            {
                switch (c)
                {
                    case 'v':
                        clineandpropts.m_bvboseEnabled = true;
                        break;
                    case 's':
                        clineandpropts.m_bstatEnabled = true;
                        break;
                    case 'b':
                        ++argv;
                        --argc;
                        bcanprocess = IsBlockSizeFilled(*argv, &clineandpropts);
                        if (!bcanprocess)
                        {
                            /* actually should be stderr but no one redirect 2> */ 
                            fprintf(stdout, "%s: invalid value for block size specified\n", progname.c_str());
                        }
                        breakout = true;
                        break;
                    default:
                        bcanprocess = false;
                        breakout = true;
                        break;
                }
            }
        }

        if (bcanprocess && argc)
        {
            SV_ULONG ver = CurrentCompatibilityNum();
            fprintf(stdout, "%s version: %lu\n", progname.c_str(), ver);
            for (int fileno = 0; fileno < argc; fileno++)
            {
                fprintf (stdout, "\n\nValidating file:  %s \n", argv [fileno] );
                result = ValidateSVFile (argv [fileno], &clineandpropts, NULL, checksums);
                if (result != 1 )
                {
                    fprintf (stdout, "%s: File, %s, failed to validate.\n", progname.c_str(), argv [fileno] );
                }
            }
        }
        else
        {
            DispUsage(progname);
        }
    }
    else
    {
        DispUsage(progname);
    }
    
    return (!result); //making result 0 for success and 1 for failure for process exit condition
}


bool IsBlockSizeFilled(const char *bsize, stCmdLineAndPrintOpts_t *pclineandpropts)
{
    bool bisblksizefilled = false;

    if (bsize)
    {
        std::stringstream sstr(bsize);
        SV_ULONGLONG ull = 0;
        sstr >> ull;
        if (sstr && ull)
        {
            pclineandpropts->m_ullWrUnitLen = pclineandpropts->m_ullOverlapLen = ull;
            bisblksizefilled = true;
        }
    }

    return bisblksizefilled;
}


void DispUsage(const std::string &progname)
{
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "verbose: %s [-v] <filename.dat[.gz]> \n", progname.c_str());
    fprintf(stdout, "statistics: %s [-s] [-b] <blocksize> <filename.dat[.gz]> \n", progname.c_str());
}

