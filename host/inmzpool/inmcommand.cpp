#include "inmcommand.h"

#include <sstream>

#ifdef SV_WINDOWS
#include <windows.h>
#include <Tlhelp32.h>
#endif


/*
* FUNCTION NAME : InmCommand
*
* DESCRIPTION : Constructor. Initializes the command name to be executed.
*                  
* INPUT PARAMETERS : command string
*
* OUTPUT PARAMETERS :None
*
* NOTES : 
* 
* return value : None
*
*/

InmCommand::InmCommand(const std::string& com)
:	m_command(com), m_pid(0), 
	m_status(not_started), m_exitstatus(-100), m_shouldWait(true),
	m_outputsize(1024 * 1024), m_errorsize(1024 * 1024)
{
	m_fdout[0] = m_fdout[1] = m_fderr[0] = m_fderr[1] = ACE_INVALID_HANDLE;
}


/*
* FUNCTION NAME : ~InmCommand
*
* DESCRIPTION : Destructor. closes the descriptors if they are open
*                  
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :None
*
* NOTES : 
* 
* return value : None
*
*/
InmCommand::~InmCommand()
{
	assert(m_status == not_started || m_status == completed || m_status == internal_error);
	assert(m_status != deleted);
	SAFE_CLOSEFILEHANDLE(m_fdout[0]);
	SAFE_CLOSEFILEHANDLE(m_fderr[0]);
	SAFE_CLOSEFILEHANDLE(m_fdout[1]);
	SAFE_CLOSEFILEHANDLE(m_fderr[1]);
	m_status = deleted;
}


/*
* FUNCTION NAME : PutEnv
*
* DESCRIPTION : Sets the environment variables for the command 
*                  
* INPUT PARAMETERS : environment variable string of the form PATH=/usr/local/bin
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : None
*
*/

void InmCommand::PutEnv(const std::string& env)
{
	m_opt.setenv(env.c_str());
}

/*
* FUNCTION NAME : SetOutputSize
*
* DESCRIPTION :  Sets the size of the output buffer
*                  
* INPUT PARAMETERS : size
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : None
*
*/


void InmCommand::SetOutputSize(size_t size)
{
	m_outputsize = size; 
}


/*
* FUNCTION NAME : SetErrorSize
*
* DESCRIPTION : Sets the size of the error buffer
*                  
* INPUT PARAMETERS : size
*
* OUTPUT PARAMETERS :None
*
* NOTES : 
* 
* return value : None
*
*/
void InmCommand::SetErrorSize(size_t size)
{
	m_errorsize = size;
}


/*
* FUNCTION NAME : Terminate
*
* DESCRIPTION : Terminates the command(child process) that was spawned.
*                  
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : None
*
* NOTES : It sets the status to InmCommand::internal_error
* 
* return value : None
*
*/
void InmCommand::Terminate(void)
{
#ifdef ACE_WIN32
	HANDLE command = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == command) {      
		return;
	}

	PROCESSENTRY32 processEntry;

	if (Process32Next(command, &processEntry)) {
		do {
			if (processEntry.th32ParentProcessID == m_pid) {
				HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processEntry.th32ProcessID);
				if (INVALID_HANDLE_VALUE == process) {
					break;
				}                                
				TerminateProcess(process, 0);
				break;
			}
		} while(Process32Next(command, &processEntry));
	}

	CloseHandle(command);
#endif
	m_proc.terminate();
}


/*
* FUNCTION NAME : Run
*
* DESCRIPTION : Executes the command and optionally waits for the termination status,
*				if m_shouldWait is true	
*			
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : statusType such as completed, internal_error, running
*
*/


InmCommand::statusType InmCommand::Run(void) 
{
	ACE_OS::last_error(0);

	if(m_status != not_started)
	{
		return internal_error;
	}

	if(( ACE_OS::pipe(m_fdout) == -1 ) || ( ACE_OS::pipe(m_fderr) == -1 ))
	{ 
		m_status = internal_error;
		std::ostringstream msg;
		m_exitstatus = ACE_OS::last_error();
		msg << "errno = " << m_exitstatus << "\nRun():internal_error: ACE_OS::pipe failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
		m_stderr = msg.str();

		return m_status;
	}

	ACE::set_flags(m_fdout[1], ACE_NONBLOCK);
	ACE::set_flags(m_fderr[1], ACE_NONBLOCK);

	m_opt.command_line(m_command.c_str());
	m_opt.set_handles(ACE_INVALID_HANDLE, m_fdout[1], m_fderr[1]);	

	ACE_OS::last_error(0);
	if((m_pid = m_proc.spawn(m_opt)) == -1)
	{
		m_status = internal_error;
		std::ostringstream msg;
		m_exitstatus = ACE_OS::last_error();
		msg << "errno = " << m_exitstatus << "\nRun():internal_error: spwan failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
		m_stderr = msg.str();
		return m_status;
	}
	m_status = running;

	ACE_OS::last_error(0);
	
	if (ACE_OS::close(m_fdout[1]) == -1)
	{
        m_status = internal_error;
		std::ostringstream msg;
		m_exitstatus = ACE_OS::last_error();
		msg << "errno = " << m_exitstatus << "\nRun():internal_error: close failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
		m_stderr = msg.str();
		ACE_OS::close(m_fderr[1]);
		m_fdout[1] = m_fderr[1] = ACE_INVALID_HANDLE;
		return m_status;
	}
	if(ACE_OS::close(m_fderr[1]) == -1)
	{
		m_status = internal_error;
		std::ostringstream msg;
		m_exitstatus = ACE_OS::last_error();
		msg << "errno = " << m_exitstatus << "\nRun():internal_error: close failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
		m_stderr = msg.str();
		m_fdout[1] = m_fderr[1] = ACE_INVALID_HANDLE;
		return m_status;

	}
	m_fdout[1] = m_fderr[1] = ACE_INVALID_HANDLE;

	if( m_shouldWait )
	{
		return Wait();
	}
	return m_status;
}

/*
* FUNCTION NAME : TryWait
*
* DESCRIPTION : Performs a Non-blocking Wait. 
*				If the command finishes execution, collects the output, error, exitcode 
*					and returns InmCommand::completed.
*				Else returns InmCommand::running
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : statusType such as not_started, running, completed, internal_error 
*
*/
InmCommand::statusType InmCommand::TryWait(void)
{
	if(m_status != running)
	{
		return m_status;
	}

	ACE_exitcode exitcode;

	ACE_OS::last_error(0);
	pid_t rc = WaitHard(exitcode, WNOHANG);

	if(rc == m_pid)
	{
	#if !defined(ACE_WIN32)
	if(WIFEXITED(exitcode) || WEXITSTATUS(exitcode))
		{
			m_exitstatus = WEXITSTATUS(exitcode);
		}	
	else if(WIFSIGNALED(exitcode))
	{
		int signo = WTERMSIG(exitcode);
		std::ostringstream msg;
		msg << m_command << " was signaled by signal# " << WTERMSIG(exitcode);
		m_stderr = msg.str();
	}
		#else
			m_exitstatus = exitcode;
		#endif /*ACE_WIN32*/
	
		m_status = completed;
		m_opt.release_handles();
		ReadPipes();
		
			
		ACE_OS::last_error(0);

		if (ACE_OS::close(m_fdout[0]) == -1) 
		{
			m_status = internal_error;
			std::ostringstream msg;
			m_exitstatus = ACE_OS::last_error();
			msg << "errno = " << m_exitstatus << "\nTryWait():internal_error: close failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
			m_stderr = msg.str();
			ACE_OS::close(m_fderr[0]);
			m_fdout[0] = m_fderr[0] = ACE_INVALID_HANDLE;
			return m_status;
		}
		if(ACE_OS::close(m_fderr[0]) == -1)
		{
			m_status = internal_error;
			std::ostringstream msg;
			m_exitstatus = ACE_OS::last_error();
			msg << "errno = " << m_exitstatus << "\nTryWait():internal_error: close failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
			m_stderr = msg.str();
			m_fdout[0] = m_fderr[0] = ACE_INVALID_HANDLE;
			return m_status;
		}
		m_fdout[0] = m_fderr[0] = ACE_INVALID_HANDLE;
	}
	else if(rc == -1)
	{
		m_status = internal_error;
		std::ostringstream msg;
		m_exitstatus = ACE_OS::last_error();
		msg << "errno = " << m_exitstatus << "\nTryWait():internal_error: wait failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
		m_stderr = msg.str();
		return m_status;
	}
	return m_status;
}


/*
* FUNCTION NAME : WaitHard
*
* DESCRIPTION : Performs a blocking wait.
*				If wait returns because of EINTR, 
*				waits again.                   
*				
*				Used by InmCommand::Wait, InmCommand::TryWait
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : exitcode 
*
* NOTES : 
* 
* return value : pid of the process 
*
*/
pid_t InmCommand::WaitHard(ACE_exitcode& exitcode, int options)
{
	pid_t rv = -1;
	do
	{
		ACE_OS::last_error(0);
		if(options)
		{
			rv = m_proc.wait(&exitcode, options);
		}
		else
		{
			rv = m_proc.wait(&exitcode);
		}
	}while((rv != m_pid) && (ACE_OS::last_error() == EINTR));

	return rv;
} 

/*
* FUNCTION NAME : Wait
*
* DESCRIPTION : Performs a blocking wait. Returns when the command finishes execution
*				Collects the output, error, exitcode.                   
*
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : statusType such as not_started, commpleted, internal_error
*
*/

InmCommand::statusType InmCommand::Wait(void)
{
	if(m_status != running)
	{
		return m_status;
	}
	
	ACE_OS::last_error(0);
	ACE_exitcode exitcode = 0;

	pid_t rv = WaitHard(exitcode);

	if (rv == m_pid)
	{
#if !defined(ACE_WIN32)
		if(WIFEXITED(exitcode) || WEXITSTATUS(exitcode))
		{
			m_exitstatus = WEXITSTATUS(exitcode);
		}		
		else if(WIFSIGNALED(exitcode))
		{
			int signo = WTERMSIG(exitcode);
			std::ostringstream msg;
			msg << m_command << " was signaled by signal# " << WTERMSIG(exitcode);
			m_stderr = msg.str();
		}
#else
		m_exitstatus = exitcode;
#endif /*ACE_WIN32*/
		m_status = completed;

		m_opt.release_handles();
		ReadPipes();
		if (ACE_OS::close(m_fdout[0]) == -1) 
		{
			m_status = internal_error;
			std::ostringstream msg;
			m_exitstatus = ACE_OS::last_error();
			msg << "errno = " << m_exitstatus << "\nWait():internal_error:close failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
			m_stderr = msg.str();
			ACE_OS::close(m_fderr[0]);
			m_fdout[0] = m_fderr[0] = ACE_INVALID_HANDLE;
			return m_status;
		}
		if (ACE_OS::close(m_fderr[0]) == -1)
		{
			m_status = internal_error;
			std::ostringstream msg;
			m_exitstatus = ACE_OS::last_error();
			msg << "errno = " << m_exitstatus << "\nWait():internal_error:close failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
			m_stderr = msg.str();
			m_fdout[0] = m_fderr[0] = ACE_INVALID_HANDLE;
			return m_status;
		}
		m_fdout[0] = m_fderr[0] = ACE_INVALID_HANDLE;

	}
	else
	{
		m_status = internal_error;
		std::ostringstream msg;
		m_exitstatus = ACE_OS::last_error();
		msg << "errno = " << m_exitstatus << "\nWait():internal_error:wait failed.\nerror = " << ACE_OS::strerror(m_exitstatus);
		m_stderr = msg.str();

		return m_status;
	}
	return m_status;
}
/*
* FUNCTION NAME : pollPipe
*
* DESCRIPTION : Reads any data on fd and appends it to buffer.  The flag
*				is cleared once the other end of the pipe is closed.
*                  
* INPUT PARAMETERS : i.	Handle from which data is to be read
*					 ii. flag to track when the other end of the pipe is closed
*					 iii.size limit of the data
*
* OUTPUT PARAMETERS : i. buffer to store the data
*		              ii. flag to track when the other end of the pipe is closed
*
* NOTES : 
* 
* return value : 1 if the buffer limit has been reached.
*				 0 if there is still room in the buffer.
*
*/

static int pollPipe(ACE_HANDLE fd, int *flag,
					std::string & buffer, size_t limit)
{
	if(!*flag)
	{
		return 0;
	}

	const size_t BUFFSIZE = 1024;
	char buf[BUFFSIZE + 1];
	ssize_t n = 0;
	ACE_OS::last_error(0);
	n = ACE_OS::read(fd, buf, BUFFSIZE);
	if (ACE_OS::last_error())
	{
		*flag = 0;
		return 0;
	}
	if(!n) //end of file on a pipe
	{
		*flag = 0;
		return 0;
	}

	if(n > 0)
	{
		buf[n] = '\0';
		buffer.append(buf);
		return (buffer.size() > limit);
	}

	return 0;
}

/*
* FUNCTION NAME : ReadPipes
*
* DESCRIPTION : Reads the output and error streams
*                  
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : None
*
*/
void InmCommand::ReadPipes(void)
{
	int outpipeIntact = 1;
	int errpipeIntact = 1;

	while ((m_status == completed ) &&
		(outpipeIntact || errpipeIntact))
	{
		
		if(pollPipe(m_fdout[0], &outpipeIntact, m_stdout, m_outputsize) ||
			pollPipe(m_fderr[0], &errpipeIntact, m_stderr, m_errorsize))
		{
			break;
		}
	}
}

/*
* FUNCTION NAME : ExitCode
*
* DESCRIPTION : Returns the termination status(exit code) of the command
*                  
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS : None
*
* NOTES : 
* 
* return value : Exit code of the command if the command is completed
*				 else -1 
*/
int InmCommand::ExitCode(void)
{
	//	If the status is completed or internal_error
	//	proper exit status(actual exit status from command)
	//	or error because of syscall failure is returned.

	// in all other cases -100 is returned.
	return m_exitstatus;
}

/*
* FUNCTION NAME : StdOut
*
* DESCRIPTION : Returns the data the command has written to standard output
*                  
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :None
*
* NOTES : 
* 
* return value : stdout of command if the command is completed
*				 else ""
*/
std::string InmCommand::StdOut(void)
{
	if(m_status == completed)
	{
		return m_stdout;
	}
	return ""; 
}

/*
* FUNCTION NAME : StdErr
*
* DESCRIPTION : Returns the data the command has written to standard error
*                  
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :None
*
* NOTES : 
* 
* return value : stderr of the command if the command is completed of failed
*				 else ""
*/
std::string InmCommand::StdErr(void)
{
	if(m_status == completed || m_status == internal_error)
	{
		return m_stderr;
	}
	return "Command is neither completed nor it encountered any internal error.";
}

