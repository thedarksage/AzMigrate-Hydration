///
/// \file pushclient.h
///
/// \brief
///

#include <string>
#include <boost/shared_ptr.hpp>
#include <ace/ACE.h>

namespace PI
{

	class PushClient 
	{

	public:

		PushClient(int argc, char ** argv):argc(argc), argv(argv) {}
		~PushClient() {}
		void parseCmd();
		void execute(ACE_exitcode &exitCode);
		void usage(const std::string & progname);
		void verifyBiosId(std::string & biosId);
		void verifyFqdn(std::string & fqdn);

	protected:

	private:

#ifdef WIN32
		void persistcredentials(const std::string & domain, const std::string & username, const std::string & encrypedpassword_path);
		bool VerifyScoutAgentServiceUserLogon(const std::string& userName, const std::string& domain, const std::string& password, HANDLE &agentLogonUserToken);
		bool GrantLogOnRightsToUser(const std::string &strDomainName, const std::string &strUserName);
#endif
		void verifysw(const std::string & installer);
		void executeCommand(const std::string & command, ACE_exitcode &exitCode);

		void SpawnProcessAndWaitForExit(const std::string &commandToExecute);
		void WaitForProcessExit(int processId);
		void PersistSecureInfo();
		void DoCleanup();

		void WriteErrorToLog(const std::string &errLogFilePath, const std::string &errMsg);
		void WriteExitStatusToLog(const std::string &statusLogFile, const int &jobId, const int &preScriptExitCode, const int &cmdExitCode, const int &postScriptExitCode);
		void Install();
		void UnInstall();
		void PatchInstall();
		void Upgrade();

		void reboot() {}

		int argc;
		char ** argv;
		std::string specFile;
	};
}