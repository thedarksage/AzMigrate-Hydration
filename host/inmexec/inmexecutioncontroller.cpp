#include "inmexecutioncontroller.h"
#include "inmcmd.h"
#include "inmspecparser.h"
#include "inmlogger.h"
#include <boost/thread.hpp>
#include <iostream>
#include <ace/Process.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <exception>


int inm_execution_controller::parse_commands(const std::string & specfilename)
{
	int rv = 0;
	const std::string section_name[] = {"version","global","job"};
	const std::string job_mandotary_entries[] = {"command","uniqueid","exitstatusfile"};
	const std::string PRESCRIPT_ENTRY = "prescript";
	const std::string POSTSCRIPT_ENTRY = "postscript";
	const std::string STDIN_ENTRY = "stdin";
	const std::string STDOUT_ENTRY = "stdout";
	const std::string STDERR_ENTRY = "stderr";
	inm_spec_parser file(specfilename);

	try
	{
		if(file.parse())
		{
			rv = 1;
			return rv;
		}

		Data::ptr data;
		while( (data = file.get_next_data()) && !rv)
		{
			if(data->section_name.compare(section_name[0]) == 0)
			{

			}
			else if(data->section_name.compare(section_name[1]) == 0)
			{
				std::map<std::string,std::string>::iterator it = data->entries.begin();
				while(it != data->entries.end())
				{
					if(it->first.compare(PRESCRIPT_ENTRY) == 0)
					{
						m_prescript = it->second;
					}
					else if(it->first.compare(POSTSCRIPT_ENTRY) == 0)
					{
						m_postscript = it->second;
					}
					//Any new entry to the section should be added here.
					it++;
				}
			}
			else if(data->section_name.compare(section_name[2]) == 0)
			{
				std::map<std::string,std::string>::iterator it;
				//Mandatory fields..

				int count = sizeof(job_mandotary_entries)/sizeof(std::string);
				for(int i = 0; i < count; i++)
				{
					if((it = data->entries.find(job_mandotary_entries[i])) == data->entries.end())
					{
						rv = 1;
						std::stringstream message;
						message<<job_mandotary_entries[i]<<" is a mandatory entry. Please check the specfile";
						inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
						break;
					}
				}
				if(rv) break; //If any of the mandatory entries is missing.. exit.

				exit_status_writer::ptr writer = get_exit_status_writer(data->entries.find(job_mandotary_entries[2])->second);
				std::string command = data->entries.find(job_mandotary_entries[0])->second;
				std::string uniqueid = data->entries.find(job_mandotary_entries[1])->second;

				inm_cmd_options options(command,uniqueid,writer);

				//Optional entries.
				if((it = data->entries.find(PRESCRIPT_ENTRY)) != data->entries.end() )
				{
					options.prescript(it->second);
				}

				if((it = data->entries.find(POSTSCRIPT_ENTRY)) != data->entries.end() )
				{
					options.postscript(it->second);
				}

				if((it = data->entries.find(STDIN_ENTRY)) != data->entries.end())
				{
					create_diretctories(it->second);
					options.stdin_path(it->second);
				}

				if((it = data->entries.find(STDOUT_ENTRY)) != data->entries.end() )
				{
					create_diretctories(it->second);
					options.stdout_path(it->second);
				}

				if((it = data->entries.find(STDERR_ENTRY)) != data->entries.end())
				{
					create_diretctories(it->second);
					options.stderr_path(it->second);
				}

				//Command construction.
				if(m_inmcommand_map.find(uniqueid) == m_inmcommand_map.end() )
				{
					inm_cmd::ptr cmd(new inm_cmd(options));
					m_inmcommand_map.insert(std::pair<std::string,inm_cmd::ptr>(uniqueid,cmd));
				}
				else
				{
					rv = 1;
					std::stringstream message;
					message<<"Please provide a unique id, "<<uniqueid<<" is not unique";
					inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
					break;
				}
			}
		}
	}
	catch(std::exception & e)
	{
		rv = 1;
		std::stringstream message;
		message<<"Exception :"<<e.what()<<std::endl;
		inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
	}
	return rv;
}

int inm_execution_controller::execute_commands()
{
	int ret_val = 0;

	do
	{
		if(!m_prescript.empty() && ((ret_val = run_prescript())!=0))
		{
			std::stringstream message;
			message<<"\nFailed to execute prescript "<<m_prescript<<". Exiting\n";
			inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
			break;
		}

		std::map < std::string, inm_cmd::ptr >::iterator it = m_inmcommand_map.begin();

		while( it != m_inmcommand_map.end() )
		{
			boost::thread * th  = new boost::thread(&inm_cmd::execute,it->second);
			m_thread_group.add_thread(th);
			it++;
		}
		m_thread_group.join_all();

		//get the status of each command executed. we return failure if any of the command execution fails.
		it = m_inmcommand_map.begin();
		while(it != m_inmcommand_map.end() ) 
		{
			if(it->second->get_status()) //If the status is non-zero 
			{
				ret_val = 1;
				std::stringstream message;
				message << "\nCommand execution has been failed with status : " << it->second->get_status() << "\n";
				inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR, message.str());
				break;
			}
			it++;
		}


		if(!ret_val && !m_postscript.empty() && ((ret_val = run_postscript())!=0))
		{
			std::stringstream message;
			message<<"\nFailed to execute prescript "<<m_postscript<<"\n";
			inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
			break;
		}
	}while(0);

	return ret_val;
}

int inm_execution_controller::run_prescript()
{
	return spawn_process(m_prescript);
}

int inm_execution_controller::run_postscript()
{
	return spawn_process(m_postscript);
}

int inm_execution_controller::spawn_process(const std::string & commandline, ACE_HANDLE std_in, ACE_HANDLE std_out, ACE_HANDLE std_err)
{
	int inherit_environment =1;
	int cmd_buf_len = commandline.size() +1;

	ACE_Process_Options options(inherit_environment, cmd_buf_len);

	options.command_line(commandline.c_str());
	options.set_handles(std_in,std_out,std_err);

	ACE_Process process;

	pid_t pid = process.spawn(options);

	if(pid == ACE_INVALID_PID)
	{
		std::stringstream message;
		message<<"Could not execute the command "<<commandline <<". Please check the command passed \n" ;
		inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
		return -1;
	}

	ACE_exitcode exitcode;
	int status;
	//Wait for the process to complete and collect the exit status.
	do 
	{
		if(process.wait(&exitcode)!=pid)
		{
			std::stringstream message;
			message<<"Error while waiting for command to complete "<<commandline << ". Error:"<<ACE_OS::last_error() ;
			inm_logger::instance()->log_message(inm_logger::INM_LOG_ERROR,message.str());
			status = ACE_OS::last_error();
			break;
		}

		//See ace/os_include/os_wait.h. If the macros are not defined, the WIFEXITED will always return true.
		if(WIFEXITED(exitcode))
		{
			status = WEXITSTATUS(exitcode);
		}else if(WIFSIGNALED(exitcode))
		{
			//128 is added to match the actual exit code returned by linux command line. The 16-8 bits consists the actual status.
			status = WTERMSIG(exitcode) + 128;
		}

	} while (!WIFEXITED(exitcode) && !WIFSIGNALED(exitcode));

	options.release_handles();

	return status;
}

exit_status_writer::ptr inm_execution_controller::get_exit_status_writer(const std::string & exitstatus_filename)
{
	create_diretctories(exitstatus_filename);

	std::map <std::string, exit_status_writer::ptr > ::iterator it = m_exit_status_writers.find(exitstatus_filename);
	if(it != m_exit_status_writers.end())
	{
		return it->second;
	}
	else
	{
		exit_status_writer::ptr wr(new exit_status_writer(exitstatus_filename)); 
		m_exit_status_writers.insert(std::pair<std::string,exit_status_writer::ptr>(exitstatus_filename,wr)); 
		return wr; 
	}
}

void inm_execution_controller::create_diretctories(std::string path)
{
	boost::filesystem::path file_path(path);

	boost::filesystem::path dir_path(file_path.parent_path());

	if (!boost::filesystem::exists(dir_path)) {
		boost::filesystem::create_directories(dir_path);
	}
}

