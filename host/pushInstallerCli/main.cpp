#include <iostream>
#include <fstream>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include <ace/Init_ACE.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>


#include "errorexception.h"
#include "../common/version.h"
#include "HandlerCurl.h"
#include "inmsafecapis.h"
#include "localconfigurator.h"
#include "programfullpath.h"
#include "logger.h"
//#include "cdputil.h"
#include "defaultdirs.h"
#include "pushclient.h"
#include "terminateonheapcorruption.h"
#include "BiosIdMismatchException.h"
#include "FqdnMismatchException.h"

std::string g_stdoutpath, g_stderrpath, g_exitstatuspath, g_pidpath, g_jobid;
std::string g_passphrasePath, g_encryptionkey_path, g_encrypedpassword_path, g_certconfig_path;

using namespace PI;

class CopyrightNotice
{	
public:
	CopyrightNotice()
	{
		std::cout<<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
	}
};

CopyrightNotice displayCopyrightNotice;


int setupLogging(std::string const& argv0)
{
	std::string programName;
	if (!getProgramFullPath(argv0, programName)){
		return -1;
	}

	std::string::size_type idx = programName.find_last_of(securitylib::SECURITY_DIR_SEPARATOR);
	std::cout << programName << std::endl;
	if (idx != std::string::npos) {
		std::string parentDir = programName.substr(0, idx);

		g_stdoutpath = parentDir + securitylib::SECURITY_DIR_SEPARATOR + "stdout.txt";
		g_stderrpath = parentDir + securitylib::SECURITY_DIR_SEPARATOR + "stderr.txt";
		g_exitstatuspath = parentDir + securitylib::SECURITY_DIR_SEPARATOR + "exitstatus.txt";
		g_pidpath = parentDir + securitylib::SECURITY_DIR_SEPARATOR + "pid.txt";
	}

	boost::filesystem::path programPathName(programName);
	programPathName.replace_extension("log");
	return 0;
}

void cleanup()
{
	if (!g_passphrasePath.empty() && boost::filesystem::exists(g_passphrasePath))
		boost::filesystem::remove(g_passphrasePath);

	if (!g_certconfig_path.empty() && boost::filesystem::exists(g_certconfig_path))
		boost::filesystem::remove(g_certconfig_path);

	if (!g_encryptionkey_path.empty() && boost::filesystem::exists(g_encryptionkey_path))
		boost::filesystem::remove(g_encryptionkey_path);

	if (!g_encrypedpassword_path.empty() && boost::filesystem::exists(g_encrypedpassword_path))
		boost::filesystem::remove(g_encrypedpassword_path);

}

int main(int argc, char** argv)
{
	TerminateOnHeapCorruption();
	init_inm_safe_c_apis();
	int rc = -1;
	ACE_exitcode exitCode = 1000;

	try
	{
		init_inm_safe_c_apis();
		ACE::init();
		tal::GlobalInitialize();
		MaskRequiredSignals();
		setupLogging(argv[0]);

		boost::filesystem::remove(g_exitstatuspath);

		PushClient pc(argc, argv);
		
		pc.parseCmd();
		pc.execute(exitCode);
		
		std::string exitstatusTmp = g_exitstatuspath;
		exitstatusTmp += ".tmp";

		std::ofstream exitstream(exitstatusTmp.c_str());
		exitstream << g_jobid;
		exitstream << ":0:" << exitCode << ":0";
		exitstream.flush();
		exitstream.close();
		boost::filesystem::remove(g_exitstatuspath);
		boost::filesystem::rename(exitstatusTmp, g_exitstatuspath);

		rc = 0;

	}
	catch (ErrorException & ee) {

		if (!g_stderrpath.empty()) {
			std::ofstream errstream(g_stderrpath.c_str(), std::ios_base::app);
			errstream << ee.what();
			errstream << std::endl;
		}

		if (!g_exitstatuspath.empty()) {
			std::string exitstatusTmp = g_exitstatuspath;
			exitstatusTmp += ".tmp";

			std::ofstream exitstream(exitstatusTmp.c_str());

			if (g_jobid.empty()) {
				g_jobid = "-1";
			}
			exitstream << g_jobid;
			exitstream << ":0:" << exitCode << ":0";
			exitstream.flush();
			exitstream.close();
			boost::filesystem::remove(g_exitstatuspath);
			boost::filesystem::rename(exitstatusTmp, g_exitstatuspath);
		}
	}
	catch (BiosIdMismatchException & be)
	{
		if (!g_stderrpath.empty()) {
			std::ofstream errstream(g_stderrpath.c_str(), std::ios_base::app);
			errstream << be.what();
			errstream << std::endl;
		}

		if (!g_exitstatuspath.empty()) {
			std::string exitstatusTmp = g_exitstatuspath;
			exitstatusTmp += ".tmp";

			std::ofstream exitstream(exitstatusTmp.c_str());

			if (g_jobid.empty()) {
				g_jobid = "-1";
			}
			exitstream << g_jobid;
			// exit code for bios id mismatch is 1999
			exitstream << ":0:" << "1999" << ":0";
			exitstream.flush();
			exitstream.close();
			boost::filesystem::remove(g_exitstatuspath);
			boost::filesystem::rename(exitstatusTmp, g_exitstatuspath);
		}
	}
	catch (FqdnMismatchException & fe)
	{
		if (!g_stderrpath.empty()) {
			std::ofstream errstream(g_stderrpath.c_str(), std::ios_base::app);
			errstream << fe.what();
			errstream << std::endl;
		}

		if (!g_exitstatuspath.empty()) {
			std::string exitstatusTmp = g_exitstatuspath;
			exitstatusTmp += ".tmp";

			std::ofstream exitstream(exitstatusTmp.c_str());

			if (g_jobid.empty()) {
				g_jobid = "-1";
			}
			exitstream << g_jobid;
			// exit code for fqdn mismatch is 2005
			exitstream << ":0:" << "2005" << ":0";
			exitstream.flush();
			exitstream.close();
			boost::filesystem::remove(g_exitstatuspath);
			boost::filesystem::rename(exitstatusTmp, g_exitstatuspath);
		}
	}
	catch (std::exception & e) {

		if (!g_stderrpath.empty()) {
			std::ofstream errstream(g_stderrpath.c_str(), std::ios_base::app);
			errstream << e.what();
			errstream << std::endl;
		}

		if (!g_exitstatuspath.empty()) {
			std::string exitstatusTmp = g_exitstatuspath;
			exitstatusTmp += ".tmp";

			std::ofstream exitstream(exitstatusTmp.c_str());
			if (g_jobid.empty()) {
				g_jobid = "-1";
			}
			exitstream << g_jobid;
			exitstream << ":0:" << exitCode << ":0";
			exitstream.flush();
			exitstream.close();
			boost::filesystem::remove(g_exitstatuspath);
			boost::filesystem::rename(exitstatusTmp, g_exitstatuspath);
		}
	}
	catch (...){

		if (!g_stderrpath.empty()) {
			std::ofstream errstream(g_stderrpath.c_str(), std::ios_base::app);
			errstream << "unhandled exception";
			errstream << std::endl;
		}

		if (!g_exitstatuspath.empty()) {
			std::string exitstatusTmp = g_exitstatuspath;
			exitstatusTmp += ".tmp";

			std::ofstream exitstream(exitstatusTmp.c_str());
			if (g_jobid.empty()) {
				g_jobid = "-1";
			}
			exitstream << g_jobid;
			exitstream << ":0:" << exitCode << ":0";
			exitstream.flush();
			exitstream.close();
			boost::filesystem::remove(g_exitstatuspath);
			boost::filesystem::rename(exitstatusTmp, g_exitstatuspath);
		}
	}

	std::ofstream errorstream(g_stderrpath.c_str(), std::ios_base::app);
	errorstream << "Push client exiting with code " << rc << "\n";
	errorstream.flush();
	errorstream.close();

	cleanup();
	tal::GlobalShutdown();
	return rc;
}

