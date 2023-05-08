#ifndef INMCOMMAND_H
#define INMCOMMAND_H


#include <string>
#include <iostream>

#include <ace/ACE.h>
#include <ace/Log_Msg.h>
#include <ace/Process.h>
#include <ace/OS.h>
#include <ace/OS_NS_fcntl.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_errno.h>
#include <ace/Global_Macros.h>

inline void SAFE_CLOSEFILEHANDLE( ACE_HANDLE& h )
{
    if( ACE_INVALID_HANDLE != h )
    {
        ACE_OS::close( h );
    }
    h = ACE_INVALID_HANDLE;
}

class InmCommand
{
public:
	enum statusType
	{
		not_started,
		running,
		completed,
		internal_error,
		deleted
	};

	InmCommand(const std::string& com);
	virtual ~InmCommand();

	statusType TryWait(void);
	statusType Run(void);
	void Terminate(void);
	
	int ExitCode(void);
	std::string StdOut(void);
	std::string StdErr(void);

	void PutEnv(const std::string& str);
	
	void SetOutputSize(size_t size);
	void SetErrorSize(size_t size);

	void SetShouldWait(bool val) { m_shouldWait = val; }

	statusType Status(void) { return m_status; };
	bool GetShouldWait(void) { return m_shouldWait; }




private:

	std::string m_command;
	size_t m_outputsize;
	size_t m_errorsize;

	
	ACE_HANDLE m_fdout[2];
	ACE_HANDLE m_fderr[2];

	ACE_Process m_proc;

	ACE_Process_Options m_opt;

	pid_t m_pid;

	bool m_shouldWait;

	statusType m_status;

	int m_exitstatus;
	std::string m_stdout;
	std::string m_stderr;

	InmCommand();
	InmCommand(const InmCommand&);
	void operator=(const InmCommand&);

	void ReadPipes(void);
	statusType Wait(void);


	pid_t WaitHard(ACE_exitcode& exitcode, int options = 0);

};

#endif

