#include "inmexecutioncontroller.h"
#include "inmcmd.h"
#include "inmspecparser.h"
#include "inmlogger.h"
#include <vector>
#include <map>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <ace/Init_ACE.h>
#include <ace/ACE.h>
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"


/*************************************************************************
 * Parse input file to get list of commands to execute
 * Construnct the command list
 * Run any global pre-script
 * execute all the commands.
 * wait for commands/threads to complete.
 * Run any global post-script
 *************************************************************************/
 
void print_usage(std::string cmd_name)
{
	std::stringstream message;
	message<<"Usage : "<<cmd_name<<" --spec=<specication file name> --output=<inmexec command execution output file name>\n";
	inm_logger::instance()->log_message(inm_logger::INM_LOG_INFO,message.str());
}

std::vector<std::string> split(const std::string arg,
							   const std::string delim) 
{

	std::vector<std::string> splitList;
	std::string::size_type delimLength = delim.length();
	std::string::size_type argEndIndex   = arg.length();
	std::string::size_type front = 0;

	//
	// Find the location of the first delimeter.
	//
	std::string::size_type back = arg.find(delim, front);

	while( back != std::string::npos ) {
		//
		// Put the first (left most) field of the string into
		// the vector of strings.
		//
		// Example:
		//
		//      field1:field2:field3
		//      ^     ^
		//      front back
		//
		// The substr() call will take characters starting
		// at front extending to the length of (back - front).
		//
		// This puts 'field1' into the splitList.
		//
		splitList.push_back(arg.substr(front, (back - front)));

		//
		// Get the front of the second field in arg.  This is done
		// by adding the length of the delimeter to the back (ending
		// index) of the previous find() call.
		//
		// This makes front point to the first character after
		// the delimeter.
		//
		// Example:
		//
		//      'field1:field2:field3'
		//             ^^
		//         back front
		//
		//       delimLength = 1
		//
		front = back + delimLength;

		//
		// Find the location of the next delimeter.
		//
		back = arg.find(delim, front);
	}

	//
	// After we get through the entire string, there may be data at the
	// end of the arg that has not been stored.  The data will be
	// the trailing remnant (or the entire string if no delimeter was found).
	// So put the "remainder" into the splitList (this would add 'field3'
	// to the list.)
	//
	// Example:
	//
	//      'field1:field2:field3'
	//                     ^     ^
	//                 front     argEndIndex
	//
	// (argEndIndex - front) yeilds the number of elements to grab.
	//
	if( front < argEndIndex ) {
		splitList.push_back(arg.substr(front, (argEndIndex - front)));
	}

	return splitList;
}


int main(int argc, char * argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	int ret_val = 1;
	ACE::init();

	do
	{
		if(argc !=3)
		{
			print_usage(argv[0]);
			break;
		}

		std::string args;
		args += " ";
		for (int i = 1; i < argc ; ++i)
		{
			args += argv[i];
			args += " ";
		}

		std::vector<std::string> tokens = split(args," --");
		std::map<std::string, std::string> arg_pairs;
		for ( size_t i = 1; i < tokens.size(); ++i)
		{
			boost::algorithm::trim(tokens[i]);
			std::vector<std::string> currarg = split(tokens[i],"=");
			if (currarg.size() != 2)
			{
				//print_usage(argv[0]);
				break;
			}
			else
			{
				boost::algorithm::trim(currarg[0]);
				boost::algorithm::trim(currarg[1]);
				arg_pairs.insert(std::pair<std::string,std::string>(currarg[0],currarg[1]));
			}
		}

		std::map<std::string, std::string>::iterator args_it = arg_pairs.find("spec");
		if(args_it == arg_pairs.end())
		{
			print_usage(argv[0]);
			break;
		}
		std::string specfilename = args_it->second;

		args_it = arg_pairs.find("output");
		if (args_it == arg_pairs.end())
		{
			print_usage(argv[0]);
			break;
		}

		inm_execution_controller controller;

		controller.create_diretctories(args_it->second);

		inm_logger::instance()->m_outputfile = args_it->second;
		
		if(controller.parse_commands(specfilename))
		{
			break;
		}

		if(controller.execute_commands())
		{
			break;
		}

		ret_val = 0;
	}while(0);
	return ret_val;
}