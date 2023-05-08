#include <iostream>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

#include <ace/Process_Manager.h>

#include "converterrortostringminor.h"
#include "errorexception.h"
#include "errorexceptionmajor.h"
#include "setpermissions.h"
#include "defaultdirs.h"
#include "pushjobspec.h"
#include "pushspecparser.h"
#include "pushclient.h"
#include "getpassphrase.h"
#include "errormessage.h"
#include "portablehelpersmajor.h"
#include "BiosIdMismatchException.h"
#include "FqdnMismatchException.h"
#include "host.h"

extern std::string g_stdoutpath, g_stderrpath, g_exitstatuspath, g_pidpath, g_jobid;
extern std::string g_passphrasePath, g_fingerprintPath, g_certPath, g_encryptionkey_path, g_encrypedpassword_path, g_certconfig_path;

namespace PI {
	
	std::string formatBiosId(std::string biosId)
	{
		// Check SwapByteOrder method at the file location
		// https://msazure.visualstudio.com/One/_git/Azure-SiteRecovery-RCM?path=%2Fsrc%2FCommon%2FRcmCommonUtils%2FRcmUtilities.cs&version=GBdevelop
		// for reference.

		// TODO (rovemula) - Check string to GUID implementation for windows and swapping bytes
		// using GUID.

		std::string formattedBiosId = biosId;
		char temp;

		temp = formattedBiosId[0];
		formattedBiosId[0] = formattedBiosId[6];
		formattedBiosId[6] = temp;
		temp = formattedBiosId[1];
		formattedBiosId[1] = formattedBiosId[7];
		formattedBiosId[7] = temp;

		temp = formattedBiosId[2];
		formattedBiosId[2] = formattedBiosId[4];
		formattedBiosId[4] = temp;
		temp = formattedBiosId[3];
		formattedBiosId[3] = formattedBiosId[5];
		formattedBiosId[5] = temp;

		temp = formattedBiosId[9];
		formattedBiosId[9] = formattedBiosId[11];
		formattedBiosId[11] = temp;
		temp = formattedBiosId[10];
		formattedBiosId[10] = formattedBiosId[12];
		formattedBiosId[12] = temp;

		temp = formattedBiosId[14];
		formattedBiosId[14] = formattedBiosId[16];
		formattedBiosId[16] = temp;
		temp = formattedBiosId[15];
		formattedBiosId[15] = formattedBiosId[17];
		formattedBiosId[17] = temp;

		return formattedBiosId;
	}

	void PushClient::usage(const std::string &progname)
	{
		std::cout << "usage: " << progname << " -c <spec file>" << std::endl;
		std::cout << "usage: " << progname << " --reboot" << std::endl;
	}

	void PushClient::parseCmd()
	{
		bool argsOk = true;
		bool printHelp = false;
		bool yOptionSet = false;

		if (1 == argc) {
			usage(argv[0]);
			throw ERROR_EXCEPTION << "incorrect invocation";
		} else if (2 == argc) {
			std::string option = std::string(argv[1]);
			boost::algorithm::trim(option);
			if (option == "--reboot") {
				reboot();
			}
			else {
				usage(argv[0]);
				throw ERROR_EXCEPTION << "incorrect invocation";
			}
		} else if (3 == argc) {
			std::string option = std::string(argv[1]);
			boost::algorithm::trim(option);

			if (std::string(argv[1]) == "-c"){
				specFile = std::string(argv[2]);
				boost::algorithm::trim(specFile);
			}
			else{
				usage(argv[0]);
				throw ERROR_EXCEPTION << "incorrect invocation";
			}
		}
		else {

			usage(argv[0]);
			throw ERROR_EXCEPTION << "incorrect invocation";
		}
	}

	void PushClient::verifyBiosId(std::string & biosId)
	{
		if (biosId.empty())
		{
			return;
		}

		std::string systemBiosId = GetSystemUUID();
		if (!boost::iequals(systemBiosId, biosId))
		{
			std::string formattedBiosId = formatBiosId(biosId);

			if (!boost::iequals(formattedBiosId, systemBiosId))
			{
				throw BiosIdMismatchException(biosId, systemBiosId);
			}
		}
	}

	void PushClient::verifyFqdn(std::string & fqdn)
	{
		if (fqdn == std::string())
		{
			// For CS stack, this value will not be passed. Hence skipping this check.
			return;
		}

		std::string sourceFqdn = Host::GetInstance().GetFQDN();
		std::string sourceHostname = Host::GetInstance().GetHostName();

		if (!boost::iequals(fqdn, sourceFqdn) && !boost::iequals(fqdn, sourceHostname))
		{
			throw FqdnMismatchException(fqdn, sourceFqdn);
		}
	}

	void PushClient::execute(ACE_exitcode &exitCode)
	{
		PushSpecParser specParser(specFile);
		specParser.parse();

		std::string csip, csport, biosid, fqdn, skip_install;

		g_stdoutpath = specParser.get(PI::SECTION_GLOBAL, PI::STD_OUT_FILEPATH);
		g_stderrpath = specParser.get(PI::SECTION_GLOBAL, PI::STD_ERR_FILEPATH);
		g_exitstatuspath = specParser.get(PI::SECTION_GLOBAL, PI::EXIT_STATUS_FILEPATH);
		g_pidpath = specParser.get(PI::SECTION_GLOBAL, PI::PID_FILEPATH);

		csip = specParser.get(PI::SECTION_GLOBAL, PI::CS_IP);
		csport = specParser.get(PI::SECTION_GLOBAL, PI::CS_PORT);
		biosid = specParser.get(PI::SECTION_JOBDEFINITION, PI::BIOS_ID);
		fqdn = specParser.get(PI::SECTION_JOBDEFINITION, PI::FQDN);
		skip_install = specParser.get(PI::SECTION_JOBDEFINITION, PI::SKIP_INSTALL);

		if (csip.empty()){
			throw ERROR_EXCEPTION << "internal error, configuration server ip is not available";
		}

		if (csport.empty()){
			throw ERROR_EXCEPTION << "internal error, configuration server port is not available";
		}

		std::string exitstatusTmp = g_exitstatuspath + ".tmp";

		securitylib::setPermissions(g_stdoutpath, securitylib::SET_PERMISSIONS_DEFAULT);
		securitylib::setPermissions(g_stderrpath, securitylib::SET_PERMISSIONS_DEFAULT);
		securitylib::setPermissions(exitstatusTmp, securitylib::SET_PERMISSIONS_DEFAULT);
		securitylib::setPermissions(g_pidpath, securitylib::SET_PERMISSIONS_DEFAULT);

		g_jobid = specParser.get(PI::SECTION_JOBDEFINITION, PI::JOB_ID);

		verifyFqdn(fqdn);
		verifyBiosId(biosid);

		int jobtype = boost::lexical_cast<int>(specParser.get(PI::SECTION_JOBDEFINITION, PI::JOB_TYPE));

		// fresh install, todo: use constants for job type check
		if (0 == jobtype || 5 == jobtype) {

			g_passphrasePath = specParser.get(PI::SECTION_JOBDEFINITION, PI::PASSPHRASE_FILEPATH);
			if (!g_passphrasePath.empty() && !boost::filesystem::exists(g_passphrasePath)) {
				throw ERROR_EXCEPTION << "internal error, connection passphrase file: " << g_passphrasePath << "is not available";
			}

			g_certconfig_path = specParser.get(PI::SECTION_JOBDEFINITION, PI::CERTCONFIG_FILEPATH);
			if (!g_certconfig_path.empty() && !boost::filesystem::exists(g_certconfig_path)) {
				throw ERROR_EXCEPTION << "internal error, certificate configuration file: " << g_certconfig_path << "is not available";
			}

			std::string installerFile = specParser.get(PI::SECTION_JOBDEFINITION, PI::INSTALLER_PATH);
			if (skip_install != "true" && installerFile.empty()){
				throw ERROR_EXCEPTION << "internal error, config file does not have installer path entry";
			}
			if (skip_install != "true" && !boost::filesystem::exists(installerFile)){
				exitCode = AGENT_INSTALLER_FILE_NOT_FOUND;
				throw ERROR_EXCEPTION << "internal error, installer file: " << installerFile << " is not available";
			}

			std::string username, domain;
			std::string encryptionkeyInstallPath = securitylib::getDomainEncryptionKeyFilePath();

#ifdef WIN32
			g_encryptionkey_path = specParser.get(PI::SECTION_JOBDEFINITION, PI::ENCRYPTION_KEY_FILE);
			g_encrypedpassword_path = specParser.get(PI::SECTION_JOBDEFINITION, PI::ENCRYPTED_PASSWORD_FILE);

			if (g_encryptionkey_path.empty() || !boost::filesystem::exists(g_encryptionkey_path)) {
				throw ERROR_EXCEPTION << "internal error, encryption key:" << g_encryptionkey_path << "is not available";
			}

			if (g_encrypedpassword_path.empty() || !boost::filesystem::exists(g_encrypedpassword_path)) {
				throw ERROR_EXCEPTION << "internal error, encryption file: " << g_encrypedpassword_path << "is not available";
			}

			username = specParser.get(PI::SECTION_JOBDEFINITION, PI::USER_NAME);
			domain = specParser.get(PI::SECTION_JOBDEFINITION, PI::DOMAIN_NAME);

			if (username.empty()){
				throw ERROR_EXCEPTION << "internal error, username is not available";
			}

			if (boost::filesystem::exists(encryptionkeyInstallPath)){
				//throw ERROR_EXCEPTION << " file " << encryptionkeyInstallPath << " exists from previous installation";
				boost::filesystem::remove(encryptionkeyInstallPath);
			}

			boost::algorithm::replace_all(encryptionkeyInstallPath, "/", securitylib::SECURITY_DIR_SEPARATOR);
			std::cout << encryptionkeyInstallPath << std::endl;
			std::string::size_type idx = encryptionkeyInstallPath.find_last_of(securitylib::SECURITY_DIR_SEPARATOR);
			if (idx != std::string::npos) {
				std::string parentdir = encryptionkeyInstallPath.substr(0, idx);
				std::cout << parentdir << std::endl;
				securitylib::setPermissions(parentdir, securitylib::SET_PERMISSIONS_NAME_IS_DIR);

			}
			boost::filesystem::rename(g_encryptionkey_path, encryptionkeyInstallPath);

			persistcredentials(domain, username, g_encrypedpassword_path);

#endif

			
			//securitylib::setPermissions(connectionPassPhraseInstallPath, securitylib::SET_PERMISSIONS_DEFAULT | securitylib::SET_PERMISSIONS_PARENT_DIR);

			//securitylib::setPermissions(certInstallPath, securitylib::SET_PERMISSIONS_DEFAULT | securitylib::SET_PERMISSIONS_PARENT_DIR);


			//securitylib::setPermissions(fingerprintInstallPath, securitylib::SET_PERMISSIONS_DEFAULT | securitylib::SET_PERMISSIONS_PARENT_DIR);
			//

		}

		// anything other than uninstall, todo: use constants for job type check
		if (3 != jobtype) {

			bool verifySignature = boost::lexical_cast<bool>(specParser.get(PI::SECTION_JOBDEFINITION, PI::VERIFY_SIGNATURE));
			if (skip_install != "true" && verifySignature) {
				std::string installer = specParser.get(PI::SECTION_JOBDEFINITION, PI::INSTALLER_PATH);
				if (installer.empty()){
					throw ERROR_EXCEPTION << "internal error, config file does not have installer path entry";
				}

				verifysw(installer);
			}
		}

		if (skip_install != "true")
		{
			std::string precommand = specParser.get(PI::SECTION_JOBDEFINITION, PI::JOB_PRE_COMMAND);
			if (!precommand.empty()) {
				executeCommand(precommand, exitCode);
				if (0 != exitCode)
					return;
			}
		}

		ACE_exitcode installationExitCode = 0;
		exitCode = AGENT_INSTALLATION_SUCCESS;
		if (skip_install != "true")
		{
			std::string command = specParser.get(PI::SECTION_JOBDEFINITION, PI::JOB_COMMAND);
			executeCommand(command, exitCode);
		}

		// if exit code is not reboot recommended or agent install successful, we will exit the agent installation and skip registration.
		// else we will store the exit code, complete registration also (since reboot recommended is not a blocking error) and return this error code if registration is successful.
		// if registration is not successful, we will return registration exit code.
		std::cout << "Mobility agent installation exit code: " << exitCode << std::endl;
		if (exitCode != AGENT_INSTALLATION_SUCCESS &&
			exitCode != AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT &&
			exitCode != AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT_LINUX &&
			exitCode != AGENT_SETUP_COMPLETED_WITH_WARNINGS) {
			std::cout << "Skipping post script command as command returned non-ignorable exit code: " << exitCode << std::endl;
			return;
		}
		else {
			installationExitCode = exitCode;
		}
		
		std::string postcommand = specParser.get(PI::SECTION_JOBDEFINITION, PI::JOB_POST_COMMAND);
		if (!postcommand.empty()){
			executeCommand(postcommand, exitCode);
			std::cout << "Mobility agent registration exit code: " << exitCode << std::endl;
		}

		if (exitCode == AGENT_INSTALLATION_SUCCESS) {
			exitCode = installationExitCode;
		}

		std::cout << "PushClient exit status to be written to file: " << exitCode << std::endl;
	}

	void PushClient::executeCommand(const std::string & command, ACE_exitcode &exitCode)
	{
		ACE_Process_Options options;
		options.handle_inheritance(false);
        options.command_line("%s", command.c_str());

		pid_t processId = ACE_Process_Manager::instance()->spawn(options);

		if (processId == ACE_INVALID_PID) {
			int lastError = ACE_OS::last_error();
			std::string err;
			convertErrorToString(lastKnownError(), err);

			throw ERROR_EXCEPTION << "internal error, failed to execute command: " << command << " with error: " << lastKnownError() << " ( "
				<< err << ")";
		}

		ACE_Process_Manager::instance()->wait(processId, &exitCode);

		std::cout << "Exit code of the executed command: " << exitCode << std::endl;

		if (WIFEXITED(exitCode)) {
			exitCode = WEXITSTATUS(exitCode);
		}
		else if (WIFSIGNALED(exitCode)) {
			exitCode = WTERMSIG(exitCode);
		}
		else if (WIFSTOPPED(exitCode)) {
			exitCode = WSTOPSIG(exitCode);
		}
	}
	
}
