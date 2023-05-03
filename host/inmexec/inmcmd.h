#ifndef INM_CMD_H_
#define INM_CMD_H_

#include <string>
#include <iostream>
#include <fstream>
#include "ace/OS.h"
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <ace/Process.h>
#include "errorexception.h"


const int INVALID_CMD = -1;
const int IO_ERROR = -2;

class inm_cmd;

class exit_status_writer
{
public:
	typedef boost::shared_ptr<exit_status_writer> ptr;
	exit_status_writer(std::string filename):m_filename(filename)
	{
		m_outputstream.open(m_filename.c_str(),std::ios_base::out|std::ios_base::trunc);
		if(!m_outputstream.good())
		{
			throw ERROR_EXCEPTION << "Could not open exit status file "<<m_filename;
		}
	}
	~exit_status_writer()
	{
		if(m_outputstream.good())
			m_outputstream.close();
	}

	bool write(const std::string & status) ;

protected:

private:
	friend class inm_command;
	std::string m_filename;
	std::ofstream m_outputstream;
	boost::mutex m_mutex;
};

class inm_cmd_options
{
public:
	inm_cmd_options(const std::string & command, const std::string & unique_id, exit_status_writer::ptr & w):
	  m_commandline(command),m_uniqueid(unique_id),m_exitstatus_writer(w){}

	inm_cmd_options& prescript(const std::string & prescript){m_prescript = prescript; return *this;}
	inm_cmd_options& postscript(const std::string & postscript){m_postscript = postscript; return *this;}
	inm_cmd_options& stdin_path(const std::string & stdin_path){m_stdin_path = stdin_path; return *this;}
	inm_cmd_options& stdout_path(const std::string & stdout_path){m_stdout_path = stdout_path; return *this;}
	inm_cmd_options& stderr_path(const std::string & stderr_path){m_stderr_path = stderr_path; return *this;}

protected:

private:

	friend class inm_cmd;
	std::string m_commandline;
	std::string m_uniqueid;

	std::string m_prescript;
	std::string m_postscript;
	std::string m_stdin_path;
	std::string m_stdout_path;
	std::string m_stderr_path;

	exit_status_writer::ptr m_exitstatus_writer;
};




class inm_cmd
{
public:
	typedef boost::shared_ptr<inm_cmd> ptr;
	inm_cmd(const inm_cmd_options & params):m_params(params){}
	int execute();
	int get_status(){return m_status;}

protected:

private:
	const inm_cmd_options m_params;
	int m_status;
	pid_t m_process_id;
};

#endif

