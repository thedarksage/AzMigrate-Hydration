#ifndef INM_EXECUTION_CONTROLLER_H_
#define INM_EXECUTION_CONTROLLER_H_

#include <map>
#include "inmcmd.h"
#include <boost/thread.hpp>


//Parses the input ini file provided, and executes the commands

class inm_execution_controller
{
public:

	int parse_commands(const std::string & filename);
	int execute_commands();
	int run_prescript();
	int run_postscript();
	static int spawn_process(const std::string & commandline, ACE_HANDLE std_in = ACE_INVALID_HANDLE, 
		ACE_HANDLE std_out  = ACE_INVALID_HANDLE, ACE_HANDLE std_err = ACE_INVALID_HANDLE);
	void create_diretctories(std::string path);

private:
	exit_status_writer::ptr  get_exit_status_writer(const std::string & exitstatus_filename);

	//vector/list maintaining the commands
	std::map < std::string, inm_cmd::ptr > m_inmcommand_map;

	//map for exit status writers. 
	std::map <std::string, exit_status_writer::ptr> m_exit_status_writers;
	std::string m_prescript;
	std::string m_postscript;
	boost::thread_group m_thread_group;
};

#endif 
