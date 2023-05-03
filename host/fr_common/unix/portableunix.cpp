#include <stdio.h>
#include <string>
using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <new>
#include <algorithm>
#include <fcntl.h>
#include "svtypes.h"
#include "portable.h"
#include "svutils.h"
#include "../fr_config/svconfig.h"
#include "../fr_log/logger.h"
#include "portableunix.h"
#include "version.h"
#include "hostagenthelpers_ported.h"
#include "inmsafeint.h"
#include "inmageex.h"

static int ACTIVE_PROCESS = -99999;
static int EXIT_PROCESS_GROUP_LEADER_FAILED = -126;

char** SplitArguments( char const* pszCommandLine, int* pArgc = 0 );

SVERROR CProcess::Create( char const* pszCommandLine, CProcess** ppProcess,
		char const* pszStdoutLogfile, char const* pszDirName)
{
    if( NULL == pszCommandLine || NULL == ppProcess )
    {
        return( SVE_INVALIDARG );
    }

    SVERROR rc;
    *ppProcess = new (std::nothrow) CUnixProcess( 
        pszCommandLine, &rc, pszStdoutLogfile, pszDirName );
    if( NULL == *ppProcess )
    {
        return( SVE_OUTOFMEMORY );
    }

    if( rc.failed() )
    {
        delete *ppProcess;
    }

    return( rc );
}

CUnixProcess::CUnixProcess( const char* pszCommandLine,
		SVERROR* pErr, const char* pszStdoutLogfile,
		const char* pszDirName):m_alreadyExited(false)
{
    assert( NULL != pszCommandLine && NULL != pErr );
    DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::CUnixProcess()] pszCommandLine = %s\n",pszCommandLine);
    m_uExitCode = ACTIVE_PROCESS;
    *pErr = SVS_OK;
	*pErr = CreateSVProcess( pszCommandLine, pszStdoutLogfile, pszDirName);    
}

CUnixProcess::~CUnixProcess()
{
}

bool CUnixProcess::hasExited()
{
        if(m_alreadyExited) return true;
        waitForExit(5000); 
        DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::hasExited()] Exit Code = %d\n",m_uExitCode);
	return (ACTIVE_PROCESS == m_uExitCode) ? false : true;   
}

void CUnixProcess::waitForExit( SV_ULONG waitTime )
{
	int status = 0;
	pid_t  pid;
        int count = 0;
        if(m_alreadyExited) return ;
        int sec = waitTime/1000; /* Convert milliseconds to seconds */
        DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::waitForExit()] Going to wait for child process, %d\n",m_childPID);
        pid = waitpid(m_childPID,&status,WNOHANG);
        DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::waitForExit()] pid from waitpid()=  %d\n",pid);
        while(pid == 0 && count < sec)
	{
            sleep(1);
            count++;
            pid = waitpid(m_childPID,&status,WNOHANG);
            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::waitForExit()] pid from waitpid()=  %d\n",pid);
	}
        if(pid != 0) 
        {
            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::waitForExit()] Wait over for child process to finish,exit code = %d\n",status);
	    m_uExitCode = status;		
            m_alreadyExited = true;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"[CUnixProcess::waitForExit()] Wait over..Child process did not finish, pid = %d\n",m_childPID);
        }
}

SVERROR CUnixProcess::terminateWithForce()
{
    /* place holder function */
	return SVS_OK;
}

//
// Send a TERM signal to process to kill it. Because it is a process group,
// its children should receive a SIGHUP when it exits.  If it doesn't respond,
// we use SIGKILL.
//
SVERROR CUnixProcess::terminate()
{
    SVERROR rc;

    if( hasExited() )
    {
        rc = SVS_FALSE;
    }
    else
    {
        DebugPrintf( SV_LOG_DEBUG, "[CUnixProcess::terminate()] stopping pid %d\n",
            m_childPID );
        if( kill( -m_childPID, SIGTERM ) < 0 )
        {
            DebugPrintf( SV_LOG_WARNING, "[CUnixProcess::terminate()] not responding to SIGTERM: pid %d\n", m_childPID );
            rc = terminateWithForce();
        }
        else
        {
            DebugPrintf( SV_LOG_DEBUG, "[CUnixProcess::terminate()] stopped %d\n",
                m_childPID );
            m_alreadyExited = true;

            // discard zombie process
            int status = 0;
            (void) waitpid( m_childPID, &status, 0 );
        }
    }

    return( rc );
}

SVERROR CUnixProcess::getExitCode( SV_ULONG* pExitCode )
{
    *pExitCode = WEXITSTATUS(m_uExitCode);
    //*pExitCode = m_uExitCode;
    if(WIFEXITED(m_uExitCode)) 
    {
	//set status to normal termination
        DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::getExitCode()] Normal termination\n");
	return SVS_OK;
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING,"[CUnixProcess::getExitCode()] Abnormal termination\n");
        return SVS_FALSE;
    }	
    
}

SVERROR CUnixProcess::CreateSVProcess( const char* pszCommandLine,
		const char* pszStdoutLogfile,	const char* pszDirName)
{
    struct sigaction ignore,sigintr,sigquit;
    sigset_t chldmask, savemask;
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    ignore.sa_flags = 0;
    char** argv = NULL;
    int newStdout = 0;

    if(sigaction(SIGINT,&ignore,&sigintr) < 0)
    {
        return errno;
    }
    if(sigaction(SIGQUIT,&ignore,&sigquit) < 0)
    {
        return errno;
    }
    sigemptyset(&chldmask);
    sigaddset(&chldmask, SIGCHLD);
    if(sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0 )
    {
        return errno;
    }

    // Redirect stdout if required
    if( NULL != pszStdoutLogfile )
    {
        newStdout = open( pszStdoutLogfile, O_APPEND | O_WRONLY, 0666 );
        if( newStdout < 0 )
        {
            return errno;
        }
    }
	if(pszDirName != NULL) {
		chdir(pszDirName);
	}
    m_childPID = fork();
    
    switch(m_childPID)
    {
        case -1 :
	{
	    //set error message that fork failed
            DebugPrintf(SV_LOG_ERROR,"[CUnixProcess::CreateSVProcess()] Error in creating child process, errno = %d\n",errno);
  	    return errno;			
	}
	case 0:
	{
            //setting PR_SET_PDEATHSIG flag to send SIGTERM/SIGKILL signal to child process.so that when parent crashes/exit child will be exited 
            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::CreateSVProcess()] setting prctl option for child \n");
            prctl( PR_SET_PDEATHSIG, SIGKILL );
           	 
			sigaction(SIGINT,&sigintr,NULL);
      	    sigaction(SIGQUIT,&sigquit,NULL);
	    	sigprocmask(SIG_SETMASK, &savemask, NULL);

            pid_t group_id = getpid();
            if( setpgid( 0, getpid() ) < 0 ) {
                DebugPrintf( SV_LOG_ERROR, "[CUnixProcess::CreateSVProcess()] Couldn't become process group leader\n" );
                exit( EXIT_PROCESS_GROUP_LEADER_FAILED );
            }

            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::CreateSVProcess()] Inside child process, Going to execute command\n");
            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::CreateSVProcess()] %s\n",pszCommandLine);

	    //execlp(pszApplicationName,pszApplicationName,"-c", pszCommandLine,(char *)0);

            if( newStdout )
            {
                // Close stdout
                close( 1 );
                // Replace stdout with new handle (always takes lowest slot)
                dup( newStdout );
                // Free the handle's slot
                close( newStdout );
                //redirect stderr to stdout
                if (dup2(1,2) == -1)
	            DebugPrintf(SV_LOG_ERROR,"[CUnixProcess::CreateSVProcess()]attempt to redirect stderr to stdout failed\n");
            }
            argv = SplitArguments( pszCommandLine );
            if( NULL == argv )
            {
                DebugPrintf(SV_LOG_ERROR,"[CUnixProcess::CreateSVProcess()] Out of memory\n");
            }
            else
            {
                execvp( argv[ 0 ], argv );
            }
            exit(127);			
	}
        default:
	{
	    //this is parent process
	    //ignore any signal from childa
            if( newStdout )
            {
                close( newStdout );
            }
            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::CreateSVProcess()]parent pid is %d\n",getpid());
            DebugPrintf(SV_LOG_DEBUG,"[CUnixProcess::CreateSVProcess()]child pid is %d\n",m_childPID);
	}
    }
    return SVS_OK;
}




SVERROR CDispatchLoop::Create( CDispatchLoop** ppDispatcher )
{
    SVERROR rc;
    CDispatchLoop* pDispatcher = new (std::nothrow) CUnixDispatchLoop( &rc );
    if( NULL == pDispatcher )
    {
        return( SVE_OUTOFMEMORY );
    }
    if( rc.failed() )
    {
        delete pDispatcher ;
    }
    else
    {
        *ppDispatcher = pDispatcher;
    }

    return( rc );
}

CUnixDispatchLoop::CUnixDispatchLoop( SVERROR* pError )
{
    assert( NULL != pError );
    m_bQuitting = false;
    *pError = SVS_OK;
}

void CUnixDispatchLoop::initialize()
{
}

void CUnixDispatchLoop::dispatch( SV_ULONG delay ) /*delay in seconds*/
{
    DebugPrintf(SV_LOG_DEBUG,"[CUnixDispatchLoop::dispatch()] Going to sleep for %d seconds\n",delay/1000); 
    sleep(delay/1000);
}

const char* GetProductVersion()
{
    return( INMAGE_PRODUCT_VERSION_STR );
}

const char* GetDriverVersion()
{
    // to date, no driver is available on Unix
    return( "none" );
}

char* GetThreadsafeErrorStringBuffer()
{
    // BUGBUG: not actually threadsafe on Unix
    static char szBuffer[ THREAD_SAFE_ERROR_STRING_BUFFER_LENGTH ];
    return( szBuffer );
}

///
/// Returns an array of tokenized strings suitable for exec()
/// "one string" yeilds one entry. Quotes may not be escaped.
//  Tabs don't tokenize.  The returned array and strings are allocated 
/// from the heap.  Optionally returns the number of arguments.
///
char** SplitArguments( char const* pszCommandLineArg, int* pArgc ) 
{
    int rc = 0;
    int maxArraySize = count( pszCommandLineArg,
                              pszCommandLineArg + strlen( pszCommandLineArg ),
                              ' ' ) + 1;
    char** pArgv = NULL;
    int argc = 0;

    do
    {
        size_t argvlen;
        INM_SAFE_ARITHMETIC(argvlen = InmSafeInt<int>::Type(maxArraySize) + 2, INMAGE_EX(maxArraySize))
        pArgv = new char*[argvlen]; // one extra for NULL entry
        if( NULL == pArgv )
        {
            rc = -1;
            break;
        }

        int i = 0;
        char const* pszCommandLine = pszCommandLineArg;
        char const* pszStart = NULL;
        enum { WHITESPACE, UNQUOTED, QUOTED, END } state = WHITESPACE;
        while( END != state )
        {
            char ch = *pszCommandLine;
            switch( state )
            {
            case WHITESPACE:
                if( 0 == ch )   { state = END; break; }
                if( ' ' == ch ) { break; }
                if( '"' == ch )
                    { state = QUOTED; pszStart = pszCommandLine + 1; break; }
                pszStart = pszCommandLine;
                state = UNQUOTED;
                break;
            case QUOTED: // fallthrough
            case UNQUOTED:
                if( 0 == ch || ((QUOTED==state) ? '"' : ' ') == ch )
                {
                    int cch = pszCommandLine - pszStart;
                    size_t arglen;
                    INM_SAFE_ARITHMETIC(arglen = InmSafeInt<int>::Type(cch) + 1, INMAGE_EX(cch))
                    char* pszArgument = new char[arglen];
                    if( NULL == pszArgument )
                    {
                        rc = -1;
                        state = END;
                        break;
                    }
					inm_memcpy_s(pszArgument, arglen, pszStart, cch);
                    pszArgument[ cch ] = 0;
                    pArgv[ argc ] = pszArgument;
                    argc ++;
                    state = ch ? WHITESPACE : END;
                    break;
                }
                break;
            }
            pszCommandLine ++;
        }
    } while( false );

    //
    // Clean up on failure
    //
    if( rc < 0 )
    {
        if( NULL != pArgv )
        {
            for( int i = 0; i < argc; i++ )
            {
                if( pArgv[ i ] )
                {
                    delete [] pArgv[ i ];
                }
            }
            delete [] pArgv;
        }
        pArgv = NULL;
        argc = 0;
    }

    if( NULL != pArgc )
    {
        *pArgc = argc;
    }
    pArgv[ argc ] = NULL;

    return( pArgv );
}

#ifdef TEST_SPLIT_ARGUMENTS
// Small test driver for SplitArguments()
int main()
{
    int argc = 0;
    char const* pszCommandLine = "\"one and a half\"   two \"three\"   g";
    char** argv = SplitArguments( pszCommandLine, &argc );
    assert( NULL == argv[ argc ] );

    printf( "Parsing: %s\n", pszCommandLine );
    printf( "argc: %d\n", argc );
    for( int i = 0; i < argc; i++ )
    {
        printf( "arg[%d]: %s\n", i, argv[ i ] );
    }
    return( 0 );
}
#endif // TEST_SPLIT_ARGUMENTS


