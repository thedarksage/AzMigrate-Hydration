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

#include "cdpplatform.h"
#include "cdpglobals.h"
#include "error.h"
#include "portable.h"

#include <sstream>
#include <sys/statvfs.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
  

#include <stdarg.h>
#include "drvstatus.h"
#include "VsnapUser.h"
#include "VsnapShared.h"
#include "localconfigurator.h"

#include <ace/OS_NS_unistd.h>
#include <ace/Process_Manager.h>

#include "inmsafecapis.h"

using namespace std;

bool GetServiceState(const std::string & ServiceName, SV_ULONG & state)
{
    const char *procdir = "/proc";	/* standard /proc directory */
    bool rv = true;
    state=0;
    DIR *dirp;
    int pdlen;
    char psname[100];
    char buffer[1024];
    char filename[100];
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
            (void) inm_strcat_s(psname, ARRAYSIZE(psname), "/stat");

            if ((psfd = open(psname, O_RDONLY)) == -1)
                continue;

            int bytes = read(psfd,buffer,1024);
            if( bytes > 0)
            {	
                int i=0,j=0;
                while(buffer[i] != '(')
                    i++;

                i++;

                for(;buffer[i]!= ')'; i++,j++)
                {
                    filename[j]=buffer[i]; 
                }
                filename[j] = '\0'; 

				std::string check = filename;
                if(!strcmp(ServiceName.c_str(),check.c_str()))
                {
                    state=1;
                    rv=true;
                    break;
                }
            }
            close(psfd);
        }
        closedir(dirp);
    }while(0);

    return rv;
}


std::string getParentProccessName()
{
	std::string procName;
	pid_t parent = ACE_OS::getppid();
	std::stringstream proccessfile;
	proccessfile << "/proc/" << parent << "/cmdline";
	std::ifstream procfile(proccessfile.str().c_str());
	procfile >> procName;
	procfile.close();
	return procName;
}
