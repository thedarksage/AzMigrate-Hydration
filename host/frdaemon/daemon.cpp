
/* 
 * @file daemon.cpp 
 * 
 * Implements file replication deamon.that implements file
 * replication.
 *
 * Author				:
 * Revision Information	:
*/

/*******************************Include Headers*******************************/

#include <sys/types.h>
#include <sys/stat.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include "portable.h"
#include "version.h"
#include "../fr_log/logger.h"
#include "svtypes.h"
#include "portableunix.h"
#include "svtransport.h"
#include "../fragent/fragent.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include "inmsafecapis.h"
#include "curlwrapper.h"
/***************************** Global declarations****************************/

void RunCode();
void replace_file_handle( int fd, const char* filename );
char szLogFileName[1024];
char szConfigFileName[1024];
void UpdateJobStatus(int signal);

/*****************************Non-Member Function Definitions*****************/

/*
 * main
 *
 * The main deamon function.
 *
 * argc		argument count.
 * argv		pointer to argument list
 *
 * Return Value 0 or EXIT_FAILURE.
*/
int main(int argc, char** argv) 
{
	try
	{
		//calling init funtion of inmsafec library
		init_inm_safe_c_apis();
		
		if(argc > 1 &&(0==strcmp(argv[1],"-v") || 0==strcmp(argv[1],"--version"))) {
			puts( "Version " INMAGE_PRODUCT_VERSION_STR );
			return( 0 );
		}
		else {
		if( 3 != argc ) {
			printf( "Usage: %s [-v] <log_file_name> <config_file_name>\n", argv[0] );
			return( EXIT_FAILURE );
		}
		}
		
		memset(szLogFileName,0,1024);
		memset(szConfigFileName,0,1024);
		inm_strncpy_s(szLogFileName,ARRAYSIZE(szLogFileName),argv[1],1023);
		szLogFileName[1023] = '\0';
		inm_strncpy_s(szConfigFileName,ARRAYSIZE(szConfigFileName),argv[2],1023);
		szConfigFileName[1023] = '\0';

		/* if we are started by init (process 1) via /etc/inittab we needn't
		bother to detach from our process group context */
		#ifdef HAVE_SIGACTION
			struct sigaction sa_new;
			struct sigaction sa_new1;
			memset (&sa_new, 0, sizeof sa_new);
			memset (&sa_new1, 0, sizeof sa_new1);
			
			sigemptyset (&sa_new.sa_mask);
			sigemptyset (&sa_new1.sa_mask);
			sa_new.sa_handler = SIG_IGN;
			sa_new1.sa_handler = UpdateJobStatus;
			#ifdef SA_RESTART	/* SunOS 4.1 portability hack */
				sa_new.sa_flags = SA_RESTART;
				sa_new1.sa_flags = SA_RESTART;
			#endif
		#endif /* HAVE_SIGACTION */
		if (getppid() == 1) {
			//there is no need to create grandchild.... 
			//call fragents directly
		}

		pid_t pid; 

		pid = fork();

		if (pid < 0) {
			exit(1); 
		}
		if(pid != 0 ) exit(0) ;//parent


		/* make the process a group leader, session leader, and lose control tty */

		if (setsid() < 0) 
		{    
		return(errno);
		}
		/* change process group */
		setpgrp();

		/* close STDOUT, STDIN, STDERR */

		replace_file_handle(STDIN_FILENO, "/dev/null" );
		replace_file_handle(STDOUT_FILENO, "/dev/null" );
		replace_file_handle(STDERR_FILENO, "/dev/null" );

		/* close STDOUT, STDIN, STDERR */

		/* ignore SIGHUP that will be sent to a child of the process */

		#ifndef HAVE_SIGACTION
		signal(SIGHUP, SIG_IGN);
		signal(SIGTERM, UpdateJobStatus);
		#else
		sigaction (SIGHUP, &sa_new, NULL);
		sigaction (SIGTERM, &sa_new1, NULL);
		#endif /* HAVE_SIGACTION */


		
		#ifdef HAVE_GETCWD
		/* move to root directory, so we don't prevent filesystem unmounts */
		chdir("/");
		#endif

		pid = fork();

		if (pid < 0) {
			exit(1); /* fork() failed, no child process was created! */
		}

		if (pid != 0) {
		exit(0); /* this is the parent, hence should exit */
		}

		/* this is the child process of the child process of the actual
		calling process and can safely be called a grandchild of original process */

		/* ignore SIGPIPE, for reading, writing to non-opened pipes */
		signal(SIGPIPE, SIG_IGN);

		/* every program using pipes should ignore this signal for 
		/* being on the safe side */
		//CALL FRAgents function here......

		RunCode();
	}
	catch(ContextualException &ce)
	{
		printf("Caught Exception:%s \n",ce.what());
		UpdateJobStatus(1);
	}	
	catch(...)
	{
		printf( "Caught unknown exception\n");
		UpdateJobStatus(1);
	}
	exit(0);

}

void replace_file_handle( int fd, const char* filename )
{
	int newFd = open( filename, O_RDWR );

	if( newFd < 0 ) {
		DebugPrintf( SV_LOG_ERROR,"[replace_file_handle] Couldn't open %s\n",
			filename );
	}
	else {
		close( fd );
		if( dup( newFd ) != fd ) {
			DebugPrintf( SV_LOG_ERROR,"[replace_file_handle] fd %d couldn't be \
				replaced!\n", fd );
		}
		else
		{
			close( newFd );
		}
	}
}

void RunCode()
{
	/*
	const char* args = "ping -i 5 10.1.1.113";
	const char* cmd = "sh";
	SVERROR rc;
	CUnixProcess *proc = new CUnixProcess(cmd,args,&rc);
	*/
	SetLogFileName(szLogFileName);

	DebugPrintf( SV_LOG_DEBUG,"Log file name = %s\n",szLogFileName );
	DebugPrintf( SV_LOG_DEBUG,"Config file name = %s\n",szConfigFileName );
	CFileReplicationAgent m_agent;

	if( m_agent.Init(szConfigFileName).failed() ) {
		DebugPrintf( SV_LOG_SEVERE,"FAILED Couldn't init CFileReplicationAgent\n" );
	}
	else {
		DebugPrintf( SV_LOG_DEBUG,"SUCCESS init CFileReplicationAgent\n" );

	}
	m_agent.Run();
	sleep(100);
	CloseDebug();
}
void UpdateJobStatus(int signal)
{
	/* place holder function */
}

