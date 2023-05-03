#include <iostream>

#include "inmcmd.h"
#include "inmlogger.h"
#include "inmexecutioncontroller.h"
#include "ace/OS_main.h"
#include "ace/Process.h"
#include "ace/Process_Manager.h"
#include <fstream>
#include <boost/lexical_cast.hpp>

bool exit_status_writer::write(const std::string & status)
{
	bool rv = true;
	boost::unique_lock<boost::mutex> lock(m_mutex);
	if(m_outputstream.good())
	{
		m_outputstream<<status<<std::endl;
		m_outputstream.flush();
	}
	else
	{
		std::stringstream message;
		message<<"Failed to log status to file "<<m_filename<<" Status: " <<status;
		inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
		rv = false;
	}
	return rv;
}


int inm_cmd::execute()
{
	int ret_val = 0;
	ACE_HANDLE std_in,std_out,std_err;
	int mode = O_RDWR|O_CREAT|O_APPEND;
	int postscript_status = 0,command_status = 0, prescript_status = 0;

	try
	{

		do
		{
			if(!m_params.m_stdin_path.empty())
			{
				//Open file handle
				if((std_in = ACE_OS::open(m_params.m_stdin_path.c_str(),mode))==ACE_INVALID_HANDLE)
				{
					ret_val = IO_ERROR; //Failed to open file
					std::stringstream message;
					message << "Failed to open file "<<m_params.m_stdin_path<<" with error "<<ACE_OS::last_error();
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
					prescript_status = ret_val;
					break;
				}
			}
			else
			{
				std_in = ACE_INVALID_HANDLE;
			}

			if(!m_params.m_stdout_path.empty())
			{
				if((std_out = ACE_OS::open(m_params.m_stdout_path.c_str(),mode))==ACE_INVALID_HANDLE)
				{
					ret_val = IO_ERROR; //Failed to open file
					std::stringstream message;
					message << "Failed to open file "<<m_params.m_stdout_path<<" with error "<<ACE_OS::last_error();
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
					prescript_status = ret_val;
					break;
				}
			}
			else
			{

				std_out = ACE_INVALID_HANDLE;
			}

			if(!m_params.m_stderr_path.empty())
			{
				if((std_err = ACE_OS::open(m_params.m_stderr_path.c_str(),mode))==ACE_INVALID_HANDLE)
				{
					ret_val = IO_ERROR; //Failed to open file
					std::stringstream message;
					message << "Failed to open file "<<m_params.m_stdin_path<<" with error "<<ACE_OS::last_error();
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
					prescript_status = ret_val;
					break;
				}
			}
			else
			{
				std_err = ACE_INVALID_HANDLE;
			}


			//1. Run the pre-script
			if(!m_params.m_prescript.empty())
			{

				if((prescript_status = inm_execution_controller::spawn_process(m_params.m_prescript,std_in,std_out,std_err)))
				{
					ret_val = 1;
					std::stringstream message;
					message << "Prescript: [" << m_params.m_prescript << "] execution failed.\n";
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR, message.str());
					break;
				}

			}

			if((command_status = inm_execution_controller::spawn_process(m_params.m_commandline,std_in,std_out,std_err)))
			{
				ret_val = 1;
				std::stringstream message;
				message << "Command: [" << m_params.m_commandline << "] execution failed.\n";
				inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR, message.str());
				break;
			}

			//3. Run the post-script
			if(!m_params.m_postscript.empty())
			{
				if((postscript_status = inm_execution_controller::spawn_process(m_params.m_postscript,std_in,std_out,std_err)))
				{
					ret_val = 1;
					std::stringstream message;
					message << "Postscript: [" << m_params.m_postscript << "] execution failed.\n";
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR, message.str());
					break;
				}
			}

		}while(0);

	}
	catch(...)
	{
		std::stringstream message;
		message<<"Exception occurred for command set with unique id "<<m_params.m_uniqueid;
		inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
	}

	if(std_out!=ACE_INVALID_HANDLE)
	{
		ACE_OS::fsync(std_out);
		ACE_OS::close(std_out);
	}

	if(std_err!=ACE_INVALID_HANDLE)
	{
		ACE_OS::fsync(std_err);
		ACE_OS::close(std_err);
	}

	if(std_in!=ACE_INVALID_HANDLE)
	{
		ACE_OS::fsync(std_in);
		ACE_OS::close(std_in);
	}
	//4. Write the exit status to the exitstatus file provided.
	std::stringstream status_str;
	status_str<<m_params.m_uniqueid<<":"<<prescript_status<<":"<<command_status<<":"<<postscript_status;

	if(!m_params.m_exitstatus_writer->write(status_str.str()))
	{
		ret_val = IO_ERROR;
	}

	m_status = ret_val;
	
	return ret_val;
}
