//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpunix.cpp
//
// Description: 
// This file contains unix specific implementation for routines
// defined in cdpplatform.h
//

#include <unistd.h>
#include <fcntl.h>
#include <procfs.h>

#include "cdpplatform.h"
#include "cdpglobals.h"
#include "error.h"
#include "portable.h"

#include <sstream>
#include <sys/statvfs.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
  

#include <stdarg.h>
#include "drvstatus.h"
#include "VsnapUser.h"
#include "VsnapShared.h"
#include "localconfigurator.h"

#include "inmsafecapis.h"

#define PSNAMELEN 100 /* limit used by ps.c in code of ps utility */

bool GetServiceState(const std::string & ServiceName, SV_ULONG & state)
{
    const char *procdir = "/proc";	/* standard /proc directory */
    bool rv = true;
    state=0;
    psinfo_t info; /* process information structure from /proc */
    DIR *dirp;
    size_t pdlen;
    char psname[PSNAMELEN];
    struct dirent *dentp;

    do 
    {
        if ((dirp = opendir(procdir)) == NULL) 
        {
            printf("Failed Opening proc directory \n");
            rv = false;
            break;
        }

        (void) inm_strcpy_s(psname, ARRAYSIZE(psname), procdir);
        pdlen = strlen(psname);
        psname[pdlen++] = '/';

        /* for each active process --- */
        while (dentp = readdir(dirp))
        {
            int	psfd;	                        /* file descriptor for /proc/nnnnn/stat */

            if (dentp->d_name[0] == '.')		/* skip . and .. */
                continue;

            (void) inm_strcpy_s(psname + pdlen, (ARRAYSIZE(psname) - pdlen), dentp->d_name);
            (void) inm_strcat_s(psname, ARRAYSIZE(psname), "/psinfo");

            if ((psfd = open(psname, O_RDONLY)) == -1)
                continue;

            memset(&info, 0, sizeof info);
            if (read(psfd, &info, sizeof (info)) == sizeof (info))
            {
                if (0 == strcmp(ServiceName.c_str(), info.pr_fname))
                {
                    state=1;
                    rv=true;
                }
            }
            (void) close(psfd);
        }
        closedir(dirp);
    }while(0);

    return rv;
}


std::string getParentProccessName()
{
	std::string procName;
	psinfo_t info;
	memset(&info, 0, sizeof info);
	pid_t parent = ACE_OS::getppid();
	std::stringstream proccessfile;
	proccessfile << "/proc/" << parent << "/psinfo";
	int	psfd;	                        /* file descriptor for /proc/nnnnn/stat */
	do
	{

		if ((psfd = open(proccessfile.str().c_str(), O_RDONLY)) == -1)
			break;
		if (read(psfd, &info, sizeof (info)) == sizeof (info))
		{
			procName = info.pr_fname;
		}
		(void) close(psfd);
	}while(false);
	return procName;
}
