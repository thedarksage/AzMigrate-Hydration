///
/// \file pushjobdefinition.cpp
///
/// \brief
///

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>
#include "defaultdirs.h"
#include "errorexception.h"
#include "pushconfig.h"
#include "PushInstallSWMgrBase.h"
#include "pushjobdefinition.h"
#include "securityutils.h"
#include "getpassphrase.h"

namespace PI {

	void PushJobDefinition::DeleteTemporaryFile(boost::filesystem::path filePath)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"Deleting file %s.\n",
			filePath.string().c_str());

		try {
			boost::filesystem::remove(filePath);
		}
		catch (std::exception & e) {
			DebugPrintf(
				SV_LOG_DEBUG,
				"Deleting File %s failed. Exception : %s.\n",
				filePath.string().c_str(),
				e.what());
		}
	}

	void PushJobDefinition::DeleteTemporaryPushInstallFiles()
	{
		std::string tempLogFilepath =
			this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			this->PIVerboseLogFileName();

		std::vector<boost::filesystem::path> filesToBeDeleted;
		std::vector<std::string> filesToBeUploadedToTelemetry;
		filesToBeUploadedToTelemetry.push_back(this->PILocalMetadataPath());
		filesToBeUploadedToTelemetry.push_back(this->PITemporaryVmWareSummaryLogFilePath());
		filesToBeUploadedToTelemetry.push_back(this->PITemporaryWMISSHSummaryLogFilePath());
		filesToBeUploadedToTelemetry.push_back(tempLogFilepath);

		boost::filesystem::path tempPushInstallFolder(this->PITemporaryFolder());
		boost::filesystem::directory_iterator it(tempPushInstallFolder), eod;

		while (it != boost::filesystem::directory_iterator())
		{
			if (std::find(filesToBeUploadedToTelemetry.begin(), filesToBeUploadedToTelemetry.end(), it->path()) == filesToBeUploadedToTelemetry.end())
			{
				filesToBeDeleted.push_back(it->path());
			}

			*it++;
		}

		for (boost::filesystem::path filePath : filesToBeDeleted)
		{
			this->DeleteTemporaryFile(filePath);
		}
	}

	void PushJobDefinition::RenameFileWithRetries(std::string &srcPath, std::string &destPath, int retries)
	{
		bool done = false;
		int attempts = 0;
		boost::system::error_code ec;

		while (!done && (attempts < retries)) 
		{
			boost::filesystem::rename(srcPath.c_str(), destPath.c_str(), ec);
			if (ec.value() && ec.value() != boost::system::errc::no_such_file_or_directory)
			{
				DebugPrintf(
					SV_LOG_WARNING,
					"Failed to rename push install telemetry file %s with error %d (%s).\n",
					srcPath.c_str(),
					ec.value(),
					ec.message().c_str());
				attempts++;
				boost::this_thread::sleep((boost::posix_time::seconds(30)));
			}
			else {
				done = true;
			}
		}

		if (!done)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Failed to rename push install telemetry file %s with error %d (%s) after %d retries.\n",
				srcPath.c_str(),
				ec.value(),
				ec.message().c_str(),
				retries);
		}
	}

	void PushJobDefinition::MoveTelemetryFilesToTemporaryTelemetryUploadFolder()
	{
		int retries = 3;

		this->RenameFileWithRetries(this->PILocalMetadataPath(), this->PILocalMetadataPathInTelemetryUploadFolder(), retries);
		this->RenameFileWithRetries(this->PITemporaryVmWareSummaryLogFilePath(), this->PITemporaryVmWareSummaryLogFilePathInTelemetryUploadFolder(), retries);
		this->RenameFileWithRetries(this->PITemporaryWMISSHSummaryLogFilePath(), this->PITemporaryWMISSHSummaryLogFilePathInTelemetryUploadFolder(), retries);
		this->RenameFileWithRetries(this->PITemporaryVerboseLogFilePath(), this->PITemporaryVerboseLogFilePathInTelemetryUploadFolder(), retries);
	}

	void PushJobDefinition::verifyMandatoryFields()
	{

	}

	bool PushJobDefinition::isRemoteMachineWindows() const
	{
		remoteApiLib::os_idx osId = (enum remoteApiLib::os_idx)this->os_id;
		return osId == remoteApiLib::windows_idx;
	}

	bool PushJobDefinition::isRemoteMachineUnix() const
	{
		remoteApiLib::os_idx osId = (enum remoteApiLib::os_idx)this->os_id;
		return osId == remoteApiLib::unix_idx;
	}

	std::string PushJobDefinition::osname() const
	{
		std::string val = getOptionalParam(PI_OSNAME_KEY);
		if (val.empty()){
			throw ERROR_EXCEPTION << "internal error, " << PI_OSNAME_KEY << " is not set";
		}

		return val;
	}

	std::string PushJobDefinition::UAOrPatchLocalPath() const
	{
		std::string val = getOptionalParam(PI_UAORPATCHLOCALPATH_KEY);
		if (val.empty()){
			throw ERROR_EXCEPTION << "internal error, " << PI_UAORPATCHLOCALPATH_KEY << " is not set";
		}

		return val;
	}

	std::string PushJobDefinition::pushclientLocalPath() const
	{
		std::string val = getOptionalParam(PI_PUSHCLIENTLOCALPATH_KEY);
		if (val.empty()){
			throw ERROR_EXCEPTION << "internal error, " << PI_PUSHCLIENTLOCALPATH_KEY << " is not set";
		}

		return val;
	}


	std::string PushJobDefinition::userName() const
	{

		std::string domainWoSpaces = domain;
		std::string userWoSpaces = username;
		boost::algorithm::trim(domainWoSpaces);
		boost::algorithm::trim(userWoSpaces);

		std::string val = userWoSpaces;

		if (os_id == remoteApiLib::windows_idx) {

			if (userWoSpaces.find_first_of("\\@") == std::string::npos) {
				
				if (domainWoSpaces.empty() ||
					(domainWoSpaces == ".") ||
					(domainWoSpaces == "\\") ||
					(domainWoSpaces == ".\\")){
					val = std::string("WORKGROUP\\") + userWoSpaces;
				}
				else {
					val = domainWoSpaces + std::string("\\") + userWoSpaces;
				}
			}
		}

		return val;
	}

	std::string PushJobDefinition::domainName() const
	{

		std::string domainWoSpaces = domain;
		std::string userWoSpaces = username;
		boost::algorithm::trim(domainWoSpaces);
		boost::algorithm::trim(userWoSpaces);

		std::string val = domainWoSpaces;

		if (os_id == remoteApiLib::windows_idx) {

			if (domainWoSpaces.empty() ||
				(domainWoSpaces == ".") ||
				(domainWoSpaces == "\\") ||
				(domainWoSpaces == ".\\")) {

				val = "WORKGROUP";
			}

			if (userWoSpaces.find_first_of("\\") != std::string::npos) {
				val = userWoSpaces.substr(0, userWoSpaces.find_first_of("\\"));
			}
			else if (userWoSpaces.find_first_of("@") != std::string::npos) {
				val = userWoSpaces.substr(userWoSpaces.find_first_of("@") + 1);
			}
		}

		return val;
	}

	std::string PushJobDefinition::remoteFolder() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = m_config->remoteRootFolder(id) 
			+ remoteApiLib::pathSeperator(id) 
			+ GetInstallMode()
			+ remoteApiLib::pathSeperator(id)
			+ jobid;
		return val;
	}

	std::string PushJobDefinition::LocalInstallerLogPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "stdout.txt";
	}

	std::string PushJobDefinition::remoteLogPath() const 
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "stdout.txt";
		return val;
	}

	std::string PushJobDefinition::LocalInstallerJsonPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "installerjson.json";
	}

	std::string PushJobDefinition::remoteInstallerJsonPath()  const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "installerjson.json";
		return val;
	}

	std::string PushJobDefinition::LocalInstallerExtendedErrorsJsonPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "installerextendederrors.json";
	}

	std::string PushJobDefinition::remoteInstallerExtendedErrorsJsonPath()  const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "installerextendederrors.json";
		return val;
	}

	std::string PushJobDefinition::LocalConfiguratorJsonPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "configuratorjson.json";
	}

	std::string PushJobDefinition::remoteConfiguratorJsonPath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "configuratorjson.json";
		return val;
	}

	std::string PushJobDefinition::remotePidPath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "pid.txt";
		return val;
	}

	std::string PushJobDefinition::LocalInstallerErrorLogPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "stderr.txt";
	}

	std::string PushJobDefinition::remoteErrorLogPath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "stderr.txt";
		return val;
	}

	std::string PushJobDefinition::LocalInstallerExitStatusPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "exitstatus.txt";
	}

	std::string PushJobDefinition::remoteExitStatusPath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id) + "exitstatus.txt";
		return val;
	}

	std::string PushJobDefinition::remoteUaPath() const
	{
		std::string val;
		std::string localUApath = UAOrPatchLocalPath();
		std::string::size_type idx = localUApath.find_last_of(remoteApiLib::pathSeperator());

		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += localUApath.substr(idx + 1);
		return val;
	}

	std::string PushJobDefinition::remoteUaPathWithoutGz() const
	{
		std::string val = remoteUaPath();
		std::string::size_type idx = val.rfind(".gz");
		if (std::string::npos != idx && (val.length() - idx) == 3)
			val.erase(idx, val.length());
		return val;
	}

	std::string PushJobDefinition::remotePushClientPath() const
	{
		std::string val;
		std::string localPushclientPath = pushclientLocalPath();
		std::string::size_type idx = localPushclientPath.find_last_of(remoteApiLib::pathSeperator());

		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += localPushclientPath.substr(idx + 1);
		return val;
	}

	std::string PushJobDefinition::remotePushClientPathWithoutGz() const
	{
		std::string val = remotePushClientPath();
		std::string::size_type idx = val.rfind(".gz");
		if (std::string::npos != idx && (val.length() - idx) == 3)
			val.erase(idx, val.length());
		return val;
	}

	std::string PushJobDefinition::remotePushClientExecutableAfterExtraction() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);

		std::string os = osname();

		if (os.find("Solaris") != std::string::npos ||
			os.find("HP-UX") != std::string::npos ||
			os.find("AIX") != std::string::npos) {

			val += os;
			val += remoteApiLib::pathSeperator(id);
			val += "push";
			val += remoteApiLib::pathSeperator(id);
		}

		val += "pushinstallclient";
		return val;
	}

	std::string PushJobDefinition::remotePushClientInvocation() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);

		std::string os = osname();

		if (os == "Win") {
			val = "\"" + val;

			if (m_config->stackType() == StackType::CS)
			{
				val += "PushClient.exe\"";
			}
			else
			{
				std::string pushClientLocalPath = this->pushclientLocalPath();
				std::string::size_type idx = 
					pushClientLocalPath.find_last_of(remoteApiLib::pathSeperator());

				if (idx != std::string::npos)
				{
					std::string pushClientFileName = pushClientLocalPath.substr(idx + 1);
					val += pushClientFileName + "\"";
				}
			}

			val += " -c ";
			val += "\"" + remoteSpecFile() + "\"" ;

		} else if (os.find("Solaris") != std::string::npos ||
			os.find("HP-UX") != std::string::npos ||
			os.find("AIX") != std::string::npos) {

			val += os;
			val += remoteApiLib::pathSeperator(id);
			val += "push";
			val += remoteApiLib::pathSeperator(id);
			val += "pushinstallclient";
			val += " -c ";
			val += remoteSpecFile();
		}
		else{
			val += "pushinstallclient";
			val += " -c ";
			val += remoteSpecFile();
		}

		return val;
	}

	std::string PushJobDefinition::remoteOsScriptInvocation() const
	{
		std::string val;
		std::string localScriptPath = m_config->osScript();
		std::string::size_type idx = localScriptPath.find_last_of(remoteApiLib::pathSeperator());

		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += localScriptPath.substr(idx + 1);
		val += " 1";
		return val;
	}

	std::string PushJobDefinition::remoteInstallScriptPath() const
	{
		if (this -> isRemoteMachineWindows())
		{
			return this->remotePushClientInvocation();
		}
		else
		{
			return this->remoteInstallerScriptPathOnUnix();
		}
	}

	std::string PushJobDefinition::LocalInstallerScriptPathForUnix() const
	{
		return this -> PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"piwrapper.sh";
	}

	std::string PushJobDefinition::remoteInstallerScriptPathOnUnix() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += "piwrapper.sh";

		return val;
	}

	std::string PushJobDefinition::remoteInstallationPath() const
	{
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		std::string installPath = installation_path;
		boost::replace_all(installPath, remoteApiLib::pathSeperatorToReplace(id), remoteApiLib::pathSeperator(id));
		if (boost::ends_with(installPath, remoteApiLib::pathSeperator(id)))
		{
			installPath.pop_back();
		}
		return installPath;
	}

	std::string PushJobDefinition::remoteMobilityAgentConfigurationFilePath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += "config.json";
		return val;
	}

	std::string PushJobDefinition::remoteConnectionPassPhrasePath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += "connection.passphrase";
		return val;
	}


	std::string PushJobDefinition::remoteCertPath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += m_proxy->csip();
		val += "_";
		val += boost::lexical_cast<std::string>(m_proxy->csport());
		val += ".crt";
		return val;
	}


	std::string PushJobDefinition::remoteFingerPrintPath() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += m_proxy->csip();
		val += "_";
		val += boost::lexical_cast<std::string>(m_proxy->csport());
		val += ".fingerprint";
		return val;
	}

	std::string PushJobDefinition::LocalEncryptedPasswordFile() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"encrypted_password.dat";
	}

	std::string PushJobDefinition::remoteEncryptionPasswordFile() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += "encrypted_password.dat";
		return val;
	}


	std::string PushJobDefinition::LocalEncryptionKeyFile() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"encryption_key.dat";
	}

	std::string PushJobDefinition::remoteEncryptionKeyFile() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += "encryption_key.dat";
		return val;
	}

	std::string PushJobDefinition::LocalPushclientSpecFile() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"pushclient_spec.txt";
	}

	std::string PushJobDefinition::remoteSpecFile() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		val = remoteFolder() + remoteApiLib::pathSeperator(id);
		val += "pushclient_spec.txt";
		return val;
	}

	std::string PushJobDefinition::remoteCommandToExtract() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		if (IsInstallationRequest() && (id == remoteApiLib::windows_idx)){


			std::string commandLineOptions = "/q /x:";
			commandLineOptions += "\"" + remoteFolder() + "\"";
			val = "\"" + remoteUaPath() + "\"";
			val += " " + commandLineOptions;
			val += " >>";
			val += "\"" + remoteLogPath() + "\"";
			val += " 2>>";
			val += "\"" + remoteErrorLogPath() + "\"" ;
		}

		return val;
	}

	std::string PushJobDefinition::remoteCommandToConfigure() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		if (IsInstallationRequest() && (id == remoteApiLib::windows_idx)){

			val = "\"" + remoteInstallationPath() + "\\agent\\UnifiedAgentConfigurator.exe" + "\"";

			if (m_clientAuthenticationType == ClientAuthenticationType::Certificate)
			{
				val += " /SourceConfigFilePath \"" + remoteMobilityAgentConfigurationFilePath() + "\"";
			}
			else
			{
				val += " /CSEndPoint \"" + m_proxy->csip() + "\"";
				val += " /PassphraseFilePath \"" + remoteConnectionPassPhrasePath() + "\"";
			}

			if (this->isVmTypePhysical())
			{
				val += " /ExternalIP \"" + remoteip + "\"";
			}

			val += " /LogFilePath \"" + remoteLogPath() + "\"";
			std::string installationMode = GetInstallMode();
			if (installationMode == "VmWare")
			{
				val += " /Invoker \"vmtools\"";
			}
			else
			{
				val += " /Invoker \"pushinstall\"";
			}
			val += " /ValidationsOutputJsonFilePath \"" + remoteInstallerExtendedErrorsJsonPath() + "\"";
			val += " /SummaryFilePath \"" + remoteConfiguratorJsonPath() + "\"";
			if (m_config->stackType() == StackType::RCM)
			{
				val += " /CSType CSPrime /CSPort " + boost::lexical_cast<std::string>(m_proxy->csport());
				val += " /ClientRequestId \"" + this->clientRequestId + "\"";
			}
			else
			{
				val += " /CSType CSLegacy /CSPort " + boost::lexical_cast<std::string>(m_proxy->csport());
			}
		}
		else {
			val = "\"" + remoteInstallationPath() + "/Vx/bin/UnifiedAgentConfigurator.sh" + "\"";

			if (m_clientAuthenticationType == ClientAuthenticationType::Certificate)
			{
				val += " -q -S \"" + remoteMobilityAgentConfigurationFilePath() + "\"";
			}
			else
			{
				val += " -i \"" + m_proxy->csip() + "\"";
				val += " -P \"" + remoteConnectionPassPhrasePath() + "\"";
			}

			if (this->isVmTypePhysical())
			{
				val += " -e \"" + remoteip + "\"";
			}
			val += " -l \"" + remoteLogPath() + "\"";
			std::string installationMode = GetInstallMode();
			if (installationMode == "VmWare")
			{
				val += " -m \"vmtools\"";
			}
			else
			{
				val += " -m \"PushInstall\"";
			}
			val += " -f \"" + remoteInstallerExtendedErrorsJsonPath() + "\"";
			val += " -j \"" + remoteConfiguratorJsonPath() + "\"";
			if (m_config->stackType() == StackType::RCM)
			{
				val += " -c CSPrime -p " + boost::lexical_cast<std::string>(m_proxy->csport());
				val += " -r \"" + this->clientRequestId + "\"";
			}
			else
			{
				val += " -c CSLegacy -p " + boost::lexical_cast<std::string>(m_proxy->csport());
			}
		}

		return val;
	}

	std::string PushJobDefinition::remoteCommandToRun() const
	{
		std::string val;
		remoteApiLib::os_idx id = (enum remoteApiLib::os_idx)os_id;
		if (IsInstallationRequest()){
			if (id == remoteApiLib::windows_idx){

				if (jobtype == pushPatchInstall) {
					val = "\"" + remoteFolder() + remoteApiLib::pathSeperator(id);
					val += "UnifiedAgentpatch.exe";
					val += "\"";
					val += " ";
					val += " /Install ";
				}
				else {
					val = "\"" + remoteFolder() + remoteApiLib::pathSeperator(id);
					val += "UnifiedAgent.exe";
					val += "\"";
					val += " ";
					val += m_commandLineOptions;

					// setup accept LogFilePath option for fresh install and upgrade
					if (jobtype == pushFreshInstall || jobtype == pushUpgrade){
						val += " /LogFilePath ";
						val += "\"" + remoteLogPath() + "\"";
					}
				}

				std::string installationMode = GetInstallMode();
				if (installationMode == "VmWare")
				{
					val += " /Invoker \"vmtools\"";
				}
				else
				{
					val += " /Invoker \"pushinstall\"";
				}
				val += " /ValidationsOutputJsonFilePath \"" + remoteInstallerExtendedErrorsJsonPath() + "\"";
				val += " /SummaryFilePath \"" + remoteInstallerJsonPath() + "\"";
			}
			else {

				// note: cs is not sending the log name, so adding it
				val = "\"" + remoteFolder() + remoteApiLib::pathSeperator(id);
				val += "install";
				val += "\"";
				val += " ";
				val += m_commandLineOptions;
				val += " -l \"" + remoteLogPath() + "\"";
				std::string installationMode = GetInstallMode();
				if (installationMode == "VmWare")
				{
					val += " -m \"vmtools\"";
				}
				else
				{
					val += " -m \"PushInstall\"";
				}
				val += " -e \"" + remoteInstallerExtendedErrorsJsonPath() + "\"";
				val += " -j \"" + remoteInstallerJsonPath() + "\"";
			}

		}
		else if (IsUninstallRequest()){
			if (id == remoteApiLib::windows_idx){
				val = "msiExec.exe /qn /x {275197FC-14FD-4560-A5EB-38217F80CBD1} /L+*V C:\\ProgramData\\ASRSetupLogs\\UAuninstall.log";
				//val += "\"" + remoteLogPath() + "\"";
			}
			else {

				// note: unix uninstaller does not provide option 
				// for specifying log path
				std::string os = osname();
				val = "\"";
				val += remoteInstallationPath();
				val += remoteApiLib::pathSeperator(id);
				if (os.find("HP-UX") != std::string::npos)	{
					val += "Fx/uninstall";
					val += "\"";
					val += " -Y";
				} else	{
					val += "uninstall.sh";
					val += "\"";
					val += " -Y -L " + remoteLogPath();
				}
			}
		}
		return val;
	}

	std::string PushJobDefinition::isrebootRemoteMachine() const
	{
		if (rebootRemoteMachine)
			return "1";
		return "0";
	}

	std::string PushJobDefinition::verifySignatureFlag() const
	{
		if (m_config->SignVerificationEnabled())
			return "1";
		return "0";
	}

	std::string PushJobDefinition::SignVerificationEnabledOnRemoteMachine() const
	{
		if (m_config->SignVerificationEnabledOnRemoteMachine())
			return "1";
		return "0";
	}


	std::string PushJobDefinition::jobType() const
	{
		int val = jobtype;
		return boost::lexical_cast<std::string>(val);
	}

	/// <summary>
	/// Gets the folder path for sending push install telemetry data.
	/// </summary>
	/// <returns>The folder path for sending push install telemetry data.
	///</returns>
	std::string PushJobDefinition::PITelemetryFolder() const
	{
		return m_config->PushInstallTelemetryLogsPath() + "\\" + pushInstallIdFolderName;
	}

	/// <summary>
	/// Gets the file name  for push install telemetry data.
	/// </summary>
	/// <returns>The file name for push install telemetry data.</returns>
	std::string PushJobDefinition::PIVerboseLogFileName() const
	{
		return "PIVerbose_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".log";
	}

	std::string PushJobDefinition::PITemporaryVerboseLogFilePath() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			this->PIVerboseLogFileName();
	}

	/// <summary>
	/// Gets the file path for push install summary log.
	/// </summary>
	/// <returns>The file path for push install summary.</returns>
	std::string PushJobDefinition::PITemporarySummaryLogFilePath() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			this->PISummaryLogFileName();
	}

	std::string PushJobDefinition::PITemporaryVmWareSummaryLogFilePath() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"PISummary" +
			"VmWare" +
			"_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".json";
	}

	std::string PushJobDefinition::PITemporaryWMISSHSummaryLogFilePath() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"PISummary" +
			"WMISSH" +
			"_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".json";
	}

	std::string PushJobDefinition::PITemporaryVmWareSummaryLogFilePathInTelemetryUploadFolder() const
	{
		return this->PITemporaryUploadFolder() +
			remoteApiLib::pathSeperator() +
			"PISummary" +
			"VmWare" +
			"_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".json";
	}

	std::string PushJobDefinition::PITemporaryWMISSHSummaryLogFilePathInTelemetryUploadFolder() const
	{
		return this->PITemporaryUploadFolder() +
			remoteApiLib::pathSeperator() +
			"PISummary" +
			"WMISSH" +
			"_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".json";
	}

	std::string PushJobDefinition::PITemporaryVerboseLogFilePathInTelemetryUploadFolder() const
	{
		return this->PITemporaryUploadFolder() +
			remoteApiLib::pathSeperator() +
			this->PIVerboseLogFileName();
	}

	/// <summary>
	/// Gets the file path for push install summary log.
	/// </summary>
	/// <returns>The file path for push install summary.</returns>
	std::string PushJobDefinition::PISummaryLogFileName() const
	{
		return "PISummary" +
			GetInstallMode() +
			"_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".json";
	}

	/// <summary>
	/// Gets the folder path for sending agent installation telemetry data.
	/// </summary>
	/// <returns>The folder path for sending agent installation telemetry data.
	///</returns>
	std::string PushJobDefinition::AgentTelemetryFolder() const
	{
		return m_config->AgentTelemetryLogsPath() + "\\" + pushInstallIdFolderName;
	}

	/// <summary>
	/// Gets the file path for agent metadata.
	/// </summary>
	/// <returns>The file path for agent metadata.</returns>
	std::string PushJobDefinition::AgentTemporaryMetadataFilePath() const
	{
		return this->AgentTemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"metadata.json";
	}

	/// <summary>
	/// Gets the file path for agent install summary telemetry data.
	/// </summary>
	/// <returns>The file path for agent install summary telemetry data.</returns>
	std::string PushJobDefinition::AgentTemporaryInstallSummaryFilePath() const
	{
		return this -> AgentTemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"AgentInstallSummary_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime + 
			".json";
	}

	/// <summary>
	/// Gets the file path for agent install verbose telemetry data.
	/// </summary>
	/// <returns>The file path for agent install verbose telemetry data.</returns>
	std::string PushJobDefinition::AgentTemporaryVerboseLogFilePath() const
	{
		return this->AgentTemporaryFolder() +
			remoteApiLib::pathSeperator() + 
			"AgentInstallVerbose_" + 
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime + 
			".log";
	}

	/// <summary>
	/// Gets the file path for agent install verbose telemetry data.
	/// </summary>
	/// <returns>The file path for agent install verbose telemetry data.</returns>
	std::string PushJobDefinition::AgentTemporaryErrorLogFilePath() const
	{
		return this->AgentTemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"AgentInstallErrors_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".log";
	}

	std::string PushJobDefinition::AgentTemporaryExitStatusFilePath() const
	{
		return this->AgentTemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"AgentInstallExitStatus_" +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime +
			".log";
	}


	/// <summary>
	/// Gets the file path for agent configuration summary telemetry data.
	/// </summary>
	/// <returns>The file path for agent configuration summary telemetry data.</returns>
	std::string PushJobDefinition::AgentTemporaryConfigurationSummaryFilePath() const
	{
		return this->AgentTemporaryFolder() +
			remoteApiLib::pathSeperator() + 
			"AgentConfigSummary_" + 
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime + 
			".json";
	}

	/// <summary>
	/// Gets the folder path for creating temporary files.
	/// </summary>
	/// <returns>Folder path for creating temporary file.</returns>
	std::string PushJobDefinition::TemporaryFolder() const
	{
		return m_config->tmpFolder() +
			remoteApiLib::pathSeperator() +
			pushInstallIdFileNamePrefix +
			this->m_jobStartTime;
	}

	/// <summary>
	/// Gets the folder path for creating temporary files.
	/// </summary>
	/// <returns>Folder path for creating temporary file.</returns>
	std::string PushJobDefinition::PITemporaryFolder() const
	{
		return TemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"PushInstall";
	}

	std::string PushJobDefinition::PITemporaryUploadFolder() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"TelemetryUpload";
	}

	/// <summary>
	/// Gets the folder path for creating temporary files.
	/// </summary>
	/// <returns>Folder path for creating temporary file.</returns>
	std::string PushJobDefinition::AgentTemporaryFolder() const
	{
		return TemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"Agent";
	}

	/// <summary>
	/// Gets the name for creating temporary file.
	/// </summary>
	/// <returns>A name for creating temporary file.</returns>
	std::string PushJobDefinition::PITemporaryFilePath(const std::string & prefix) const
	{
		return this-> PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			prefix + ".dat";
	}

	std::string PushJobDefinition::PILocalMetadataPath() const
	{
		return this->PITemporaryFolder() + remoteApiLib::pathSeperator() + "metadata.json";
	}

	std::string PushJobDefinition::PILocalMetadataPathInTelemetryUploadFolder() const
	{
		return this->PITemporaryUploadFolder() + remoteApiLib::pathSeperator() + "metadata.json";
	}

	std::string PushJobDefinition::AgentLocalMetadataPath() const
	{
		return this->AgentTemporaryFolder() + remoteApiLib::pathSeperator() + "metadata.json";
	}

	/// <summary>
	/// Constructs the command line for fetching operating system details using vmware wrapper cmd.
	/// </summary>
	VmWareWrapperCmdInput PushJobDefinition::VmWareApiOsCmdInput() const
	{
		VmWareWrapperCmdInput cmdInput;

		std::string outputFilepath = this->PITemporaryFilePath("vmwareapioscmdoutput");
		std::string logFilepath = this->PITemporaryFilePath("vmwareapioscmdlog");
		std::string summaryLogFilepath = this->PITemporaryFilePath("vmwareapioscmdsummarylog");
		remoteApiLib::os_idx osId = (enum remoteApiLib::os_idx)os_id;

		std::string cmd = m_config->vmWareWrapperCmd();
				
		cmd += " --operation fetchosname ";

		switch (m_config->stackType())
		{
		case StackType::RCM:
			cmd += " --stacktype \"RCM\" ";

			cmd += " --credentialstorefilepath ";
			cmd += "\"";
			cmd += m_config->credStoreCredsFilePath();
			cmd += "\"";
			break;
		default:
			break;
		}
		
		cmd += " --csip ";
		cmd += "\"";
		cmd += m_proxy->csip();
		cmd += "\"";

		cmd += " --csport ";
		cmd += "\"";
		cmd += boost::lexical_cast<std::string>(m_proxy->csport());
		cmd += "\"";

		cmd += " --usehttps ";
		cmd += "\"";
		cmd += m_config->secure()? "true":"false";
		cmd += "\"";

		cmd += " --pshostid ";
		cmd += "\"";
		cmd += m_config->hostid();
		cmd += "\"";

		cmd += " --vcenterip ";
		cmd += "\"";
		cmd += this->vCenterIP;
		cmd += "\"";

		cmd += " --vcenteraccount ";
		cmd += "\"";
		cmd += this->vcenterAccountId;
		cmd += "\"";

		cmd += " --vmipaddresses ";
		cmd += "\"";
		cmd += this->remoteip;
		cmd += "\"";

		cmd += " --vmname ";
		cmd += "\"";
		cmd += this->vmName;
		cmd += "\"";

		cmd += " --vmaccount ";
		cmd += "\"";
		cmd += this->vmAccountId;
		cmd += "\"";

		cmd += " --sourcemachinebiosid ";
		cmd += "\"";
		cmd += this->sourceMachineBiosId;
		cmd += "\"";

		cmd += " --outputfilepath ";
		cmd += "\"";
		cmd += outputFilepath;
		cmd += "\"";

		cmd += " --logfilepath ";
		cmd += "\"";
		cmd += logFilepath;
		cmd += "\"";

		cmd += " --summarylogfilepath ";
		cmd += "\"";
		cmd += summaryLogFilepath;
		cmd += "\"";

		cmd += " --filestocopy ";
		cmd += "\"";
		cmd += m_config->osScript();
		cmd += "\"";
		cmd += " ";

		cmd += " --folderonremotemachine ";
		cmd += "\"";
		cmd += this ->remoteFolder();
		//cmd += remoteApiLib::pathSeperator(osId);
		cmd += "\"";

		cmd += " --cmdtorun ";
		cmd += "\"";
		cmd += this->remoteOsScriptInvocation();
		cmd += "\"";
		cmd += " ";

		cmdInput.cmd = cmd;
		cmdInput.outputFilepath = outputFilepath;
		cmdInput.summaryLogFilepath = summaryLogFilepath;
		cmdInput.logFilepath = logFilepath;
				
		return cmdInput;
	}

	/// <summary>
	/// Constructs the command line for installing mobility agent using vmware wrapper cmd.
	/// </summary>
	VmWareWrapperCmdInput PushJobDefinition::InstallMobilityAgentCmdInput() const
	{
		VmWareWrapperCmdInput cmdInput;

		std::string outputFilepath = this->PITemporaryFilePath("installcmdoutput");
		std::string logFilepath = this->PITemporaryFilePath("installcmdlog");
		std::string summaryLogFilepath = this->PITemporaryFilePath("installcmdsummarylog");
		remoteApiLib::os_idx osId = (enum remoteApiLib::os_idx)os_id;

		std::string cmd = m_config->vmWareWrapperCmd();

		cmd += " --operation installmobilityagent ";

		switch (m_config->stackType())
		{
		case StackType::RCM:
			cmd += " --stacktype \"RCM\" ";

			cmd += " --credentialstorefilepath ";
			cmd += "\"";
			cmd += m_config->credStoreCredsFilePath();
			cmd += "\"";
			break;
		default:
			break;
		}

		cmd += " --csip ";
		cmd += "\"";
		cmd += m_proxy->csip();
		cmd += "\"";

		cmd += " --csport ";
		cmd += "\"";
		cmd += boost::lexical_cast<std::string>(m_proxy->csport());
		cmd += "\"";

		cmd += " --usehttps ";
		cmd += "\"";
		cmd += m_config->secure() ? "true" : "false";
		cmd += "\"";

		cmd += " --pshostid ";
		cmd += "\"";
		cmd += m_config->hostid();
		cmd += "\"";

		cmd += " --vcenterip ";
		cmd += "\"";
		cmd += this->vCenterIP;
		cmd += "\"";

		cmd += " --vcenteraccount ";
		cmd += "\"";
		cmd += this->vcenterAccountId;
		cmd += "\"";

		cmd += " --vmipaddresses ";
		cmd += "\"";
		cmd += this->remoteip;
		cmd += "\"";

		cmd += " --vmname ";
		cmd += "\"";
		cmd += this->vmName;
		cmd += "\"";

		cmd += " --vmaccount ";
		cmd += "\"";
		cmd += this->vmAccountId;
		cmd += "\"";

		cmd += " --outputfilepath ";
		cmd += "\"";
		cmd += outputFilepath;
		cmd += "\"";

		cmd += " --logfilepath ";
		cmd += "\"";
		cmd += logFilepath;
		cmd += "\"";

		cmd += " --summarylogfilepath ";
		cmd += "\"";
		cmd += summaryLogFilepath;
		cmd += "\"";

		std::string passphraseFilePath = m_proxy->GetPassphraseFilePath();
		boost::replace_all(
			passphraseFilePath,
			remoteApiLib::pathSeperatorToReplace(),
			remoteApiLib::pathSeperator());

		cmd += " --filestocopy ";
		cmd += "\"";
		cmd += this->pushclientLocalPath();
		cmd += ";";
		if (m_clientAuthenticationType == ClientAuthenticationType::Passphrase)
		{
			cmd += passphraseFilePath;
			cmd += ";";
		}
		else
		{
			cmd += mobilityAgentConfigurationFilePath;
			cmd += ";";
		}
		cmd += this->LocalPushclientSpecFile();

		if (this->jobtype != PushJobType::pushConfigure)
		{
			if (this->IsInstallationRequest())
			{
				cmd += ";";
				cmd += this->UAOrPatchLocalPath();
			}
		}
		if (this->isRemoteMachineUnix())
		{
			cmd += ";";
			cmd += this->LocalInstallerScriptPathForUnix();
		}
		if (this->isRemoteMachineWindows())
		{
			cmd += ";";
			cmd += this->LocalEncryptedPasswordFile();
			cmd += ";";
			cmd += this->LocalEncryptionKeyFile();
		}
		cmd += "\"";
		cmd += " ";

		cmd += " --folderonremotemachine ";
		cmd += "\"";
		cmd += this->remoteFolder();
		//cmd += remoteApiLib::pathSeperator(osId);
		cmd += "\"";

		cmd += " --filestofetch ";
		cmd += "\"";
		cmd += this->remoteLogPath();
		cmd += ";";
		cmd += this->remoteErrorLogPath();
		cmd += ";";
		cmd += this->remoteExitStatusPath();
		cmd += ";";
		cmd += this->remoteInstallerJsonPath();
		cmd += ";";
		cmd += this->remoteConfiguratorJsonPath();
		cmd += ";";
		cmd += this->remoteInstallerExtendedErrorsJsonPath();
		cmd += "\"";
		cmd += " ";

		cmd += " --localfoldertodownload ";
		cmd += "\"";
		cmd += this->AgentTemporaryFolder();
		//cmd += remoteApiLib::pathSeperator();
		cmd += "\"";
		cmd += " ";

		cmd += " --cmdtorun ";
		cmd += "\"";
		std::string installScript = this->remoteInstallScriptPath();
		boost::replace_all(installScript, "\"", "\\\"");
		cmd += installScript;
		cmd += "\"";
		cmd += " ";

		cmdInput.cmd = cmd;
		cmdInput.outputFilepath = outputFilepath;
		cmdInput.summaryLogFilepath = summaryLogFilepath;
		cmdInput.logFilepath = logFilepath;

		return cmdInput;
	}

	/// <summary>
	/// Constructs the command line for getting source machine bios id.
	/// </summary>
	std::string PushJobDefinition::remoteCommandToGetBiosId() const
	{
		// TODO (rovemula) - Update the bios id command and add this to push install client script
		remoteApiLib::os_idx source_os_id = (enum remoteApiLib::os_idx)this->os_id;
		std::string command = "";

		if (source_os_id == remoteApiLib::os_idx::windows_idx)
		{
			command += "wmic bios get serialnumber";
		}
		else
		{
			command += "dmidecode -s system-serial-number";
		}

		return command;
	}

	std::string PushJobDefinition::GetPassphraseTemporaryFilePath() const
	{
		return this->PITemporaryFolder() +
			remoteApiLib::pathSeperator() +
			"connection.passphrase";
	}

}
