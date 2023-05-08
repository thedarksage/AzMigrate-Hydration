///
/// \file pushjob.h
///
/// \brief
///


#ifndef INMAGE_PI_JOB_DEFINITION_H
#define INMAGE_PI_JOB_DEFINITION_H

#include <map>
#include <string>

#include "pushInstallationSettings.h"
#include "PushInstallCommunicationBase.h"
#include "pushconfig.h"
#include "defs.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/filesystem.hpp>

namespace PI {

	const std::string PI_OSNAME_KEY = "osname";
	const std::string PI_UAORPATCHLOCALPATH_KEY = "uaOrPatchLocalPath";
	const std::string PI_PUSHCLIENTLOCALPATH_KEY = "pushclientLocalPath";

	const int WINDOWS_OS = 1;
	const int LINUX_OS = 2;

	enum InstallationMode { VmWare, WMISSH };
	enum ClientAuthenticationType { Certificate, Passphrase };
	enum PushJobType { pushFreshInstall = 0, pushUpgrade, pushPatchInstall, pushUninstall, fetchDistroDetails, pushConfigure };

	struct VmWareWrapperCmdInput
	{
		std::string cmd;            // command to execute.
		std::string outputFilepath; // temporary file for sending output from command execution.
		std::string logFilepath;    // temporary file for verbose logging.
		std::string summaryLogFilepath; // temporary file for summary logging.
	};

	class PushJobDefinition
	{
	public:
		std::string jobid;              // job identifier
		PushJobType jobtype;            // fresh install, upgrade , patch installation or uninstall
		int  os_id;                     //  linux/unix:0, windows:1
		std::string remoteip;           // ip where to install/uninstall
		VM_TYPE vmType;					// vmware discovery type
		std::string vCenterIP;			// vCenter IP for vmware discovery
		std::string vcenterAccountId;   // vCenter account ID for vmware based install.
		std::string vCenterUserName;	// vCenter username for vmware based install.
		std::string vCenterPassword;	// vCenter password for vmware based install.
		std::string vmName;				// VM Display Name
		std::string vmAccountId;        // VM account ID for vmware based install.
		std::string domain;             // remote machine domain (not applicable for unix, unix: empty)
		std::string username;           // remote machine username
		std::string password;           // remote machine password
		std::string clientRequestId;    // Client Request ID.
		std::string activityId;         // activity id
		std::string serviceActivityId;  // Service Activity ID.
		std::string installation_path;  // installation path on remote machine
		bool        rebootRemoteMachine; // reboot remote machine after job completion

		std::string UAdownloadLocation;    // For Uninstallaton: this field is empty 
		// for  upgrade/patch installation
		//   always filled in by CS
		// for fresh installation, 
		//  windows : always filled up by CS
		//  unix: determined after finding the remote machine os type (by push service)


		std::string pushclientDownloadLocation;    	 // for  upgrade/patch installation
		//   always filled in by CS
		// for fresh installation, 
		//  windows : always filled up by CS
		//  unix: determined after finding the remote machine os type (by push service)


		std::string m_commandLineOptions; // silent installation/uninstallation command line arguments
		PUSH_JOB_STATUS jobStatus;

		std::map<std::string, std::string> optionalParameters; // currently none

		std::string sourceMachineBiosId;
		std::string sourceMachineFqdn;

		std::string m_jobStartTime;
		std::string m_jobStartTimeISO;
		InstallationMode installationMode;

		std::string pushInstallIdFolderName;
		std::string pushInstallIdFileNamePrefix;

		PushInstallCommunicationBasePtr m_proxy;
		PushConfigPtr m_config;

		ClientAuthenticationType m_clientAuthenticationType;
		std::string mobilityAgentConfigurationFilePath;

		PushJobDefinition(
			const std::string & id,
			int os_id,
			const std::string & remoteip,
			VM_TYPE vmType,
			const std::string & vCenterIP,
			const std::string & vcenterAccountId,
			const std::string & vCenterUserName,
			const std::string & vCenterPassword,
			const std::string & vmName,
			const std::string & vmAccountId,
			const std::string & domain,
			const std::string & username,
			const std::string & password,
			const std::string & clientRequestId,
			const std::string & activityId,
			const std::string & serviceActivityId,
			const std::string & installation_path,
			bool rebootRemoteMachine,
			PUSH_JOB_STATUS jobStatus,
			InstallationMode installationMode,
			PushInstallCommunicationBasePtr & proxyImpl,
			PushConfigPtr & configImpl,
			const std::string & sourceMachineBiosId,
			const std::string & sourceMachineFqdn,
			const std::string & pushInstallIdFolderName,
			const std::string & pushInstallIdFileNamePrefix,
			const std::string & clientAuthenticationType,
			const std::string & mobilityAgentConfigurationFilePath,
			const std::string & jobType):
			jobid(id), os_id(os_id),
			remoteip(remoteip),
			vmType(vmType),
			vCenterIP(vCenterIP),
			vcenterAccountId(vcenterAccountId),
			vCenterUserName(vCenterUserName),
			vCenterPassword(vCenterPassword),
			vmName(vmName),
			vmAccountId(vmAccountId),
			domain(domain),
			username(username),
			password(password),
			clientRequestId(clientRequestId),
			activityId(activityId),
			serviceActivityId(serviceActivityId),
			installation_path(installation_path),
			rebootRemoteMachine(rebootRemoteMachine),
			jobStatus(jobStatus),
			installationMode(installationMode),
			sourceMachineBiosId(sourceMachineBiosId),
			sourceMachineFqdn(sourceMachineFqdn),
			pushInstallIdFolderName(pushInstallIdFolderName),
			pushInstallIdFileNamePrefix(pushInstallIdFileNamePrefix),
			mobilityAgentConfigurationFilePath(mobilityAgentConfigurationFilePath)
		{
			m_config = configImpl;
			m_proxy = proxyImpl;

			if (this->IsDistroFetchRequest(jobType))
			{
				jobtype = PushJobType::fetchDistroDetails;
			}
			else if (this->IsConfigurationRequest(jobType))
			{
				jobtype = PushJobType::pushConfigure;
			}
			else if (this->IsInstallationRequest())
			{
				jobtype = PushJobType::pushFreshInstall;
			}
			else
			{
				jobtype = PushJobType::pushUninstall;
			}

			if (clientAuthenticationType == "Certificate")
			{
				m_clientAuthenticationType = ClientAuthenticationType::Certificate;
			}
			else
			{
				m_clientAuthenticationType = ClientAuthenticationType::Passphrase;
			}

			boost::posix_time::ptime currTime = boost::posix_time::microsec_clock::local_time();
			m_jobStartTimeISO = to_iso_extended_string(currTime) + "Z";
			std::stringstream ss;
			ss << currTime.date().year() 
				<< "_" << static_cast<int>(currTime.date().month()) 
				<< "_" << currTime.date().day() 
				<< "_"
				<< currTime.time_of_day().hours() 
				<< "_" << currTime.time_of_day().minutes() 
				<< "_" << currTime.time_of_day().seconds();

			m_jobStartTime = ss.str();
		}

		PushJobDefinition(const PushJobDefinition & rhs)
		{
			jobid = rhs.jobid;
			jobtype = rhs.jobtype;
			os_id = rhs.os_id;
			remoteip = rhs.remoteip;
			vmType = rhs.vmType;
			vCenterIP = rhs.vCenterIP;
			vcenterAccountId = rhs.vcenterAccountId;
			vCenterUserName = rhs.vCenterUserName;
			vCenterPassword = rhs.vCenterPassword;
			vmName = rhs.vmName;
			vmAccountId = rhs.vmAccountId;
			domain = rhs.domain;
			username = rhs.username;
			password = rhs.password;
			clientRequestId = rhs.clientRequestId;
			activityId = rhs.activityId;
			serviceActivityId = rhs.serviceActivityId;
			installation_path = rhs.installation_path;
			rebootRemoteMachine = rhs.rebootRemoteMachine;
			UAdownloadLocation = rhs.UAdownloadLocation;
			pushclientDownloadLocation = rhs.pushclientDownloadLocation;
			m_commandLineOptions = rhs.m_commandLineOptions;
			jobStatus = rhs.jobStatus;
			optionalParameters = rhs.optionalParameters;
			m_jobStartTime = rhs.m_jobStartTime;
			m_jobStartTimeISO = rhs.m_jobStartTimeISO;
			installationMode = rhs.installationMode;
			m_proxy = rhs.m_proxy;
			m_config = rhs.m_config;
			sourceMachineBiosId = rhs.sourceMachineBiosId;
			sourceMachineFqdn = rhs.sourceMachineFqdn;
			pushInstallIdFolderName = rhs.pushInstallIdFolderName;
			pushInstallIdFileNamePrefix = rhs.pushInstallIdFileNamePrefix;
			m_clientAuthenticationType = rhs.m_clientAuthenticationType;
			mobilityAgentConfigurationFilePath = rhs.mobilityAgentConfigurationFilePath;
		}

		PushJobDefinition &operator=(const PushJobDefinition & rhs)
		{
			if (this == &rhs) {
				return *this;
			}

			jobid = rhs.jobid;
			jobtype = rhs.jobtype;
			os_id = rhs.os_id;
			remoteip = rhs.remoteip;
			vmType = rhs.vmType;
			vCenterIP = rhs.vCenterIP;
			vcenterAccountId = rhs.vcenterAccountId;
			vCenterUserName = rhs.vCenterUserName;
			vCenterPassword = rhs.vCenterPassword;
			vmName = rhs.vmName;
			vmAccountId = rhs.vmAccountId;
			domain = rhs.domain;
			username = rhs.username;
			password = rhs.password;
			clientRequestId = rhs.clientRequestId;
			activityId = rhs.activityId;
			serviceActivityId = rhs.serviceActivityId;
			installation_path = rhs.installation_path;
			rebootRemoteMachine = rhs.rebootRemoteMachine;
			UAdownloadLocation = rhs.UAdownloadLocation;
			pushclientDownloadLocation = rhs.pushclientDownloadLocation;
			m_commandLineOptions = rhs.m_commandLineOptions;
			jobStatus = rhs.jobStatus;
			optionalParameters = rhs.optionalParameters;
			m_jobStartTime = rhs.m_jobStartTime;
			m_jobStartTimeISO = rhs.m_jobStartTimeISO;
			installationMode = rhs.installationMode;
			m_proxy = rhs.m_proxy;
			m_config = rhs.m_config;
			sourceMachineBiosId = rhs.sourceMachineBiosId;
			sourceMachineFqdn = rhs.sourceMachineFqdn;
			pushInstallIdFolderName = rhs.pushInstallIdFolderName;
			pushInstallIdFileNamePrefix = rhs.pushInstallIdFileNamePrefix;
			m_clientAuthenticationType = rhs.m_clientAuthenticationType;
			mobilityAgentConfigurationFilePath = rhs.mobilityAgentConfigurationFilePath;
		}

		~PushJobDefinition()
		{
			
		}

		bool IsDistroFetchRequest(const std::string jobType) const
		{
			return ((jobType == "FetchDistroDetailsOfVMwareVm") ||
				(jobType == "FetchDistroDetailsOfPhysicalServer"));
		}

		bool IsConfigurationRequest(const std::string jobType) const
		{
			return ((jobType == "ConfigureMobilityServiceOnVMwareVm") ||
				(jobType == "ConfigureMobilityServiceOnPhysicalServer"));
		}

		bool IsInstallationRequest() const 
		{
			return ((jobStatus == INSTALL_JOB_PENDING) ||
				(jobStatus == INSTALL_JOB_PROGRESS) ||
				(jobStatus == INSTALL_JOB_COMPLETED) ||
				(jobStatus == INSTALL_JOB_FAILED) ||
				(jobStatus == INSTALL_JOB_COMPLETED_WITH_WARNINGS));
		}

		bool IsUninstallRequest()  const 
		{
			return ((jobStatus == UNINSTALL_JOB_PENDING) ||
				(jobStatus == UNINSTALL_JOB_PROGRESS) ||
				(jobStatus == UNINSTALL_JOB_COMPLETED) ||
				(jobStatus == UNINSTALL_JOB_FAILED));
		}

		bool IsJobSucceeded() const
		{
			return (jobStatus == INSTALL_JOB_COMPLETED ||
				jobStatus == UNINSTALL_JOB_COMPLETED);
		}

		bool IsJobFailed() const
		{
			return (jobStatus == INSTALL_JOB_FAILED ||
				jobStatus == UNINSTALL_JOB_FAILED ||
				jobStatus == INSTALL_JOB_COMPLETED_WITH_WARNINGS);
		}

		void setOptionalParam(const std::string & key, const std::string & value)
		{
			optionalParameters[key] = value;
		}


		std::string getOptionalParam(const std::string & key) const
		{
			std::map<std::string, std::string>::const_iterator iter = optionalParameters.find(key);
			if (iter == optionalParameters.end()){
				return std::string();
			}
			else{
				return iter->second;
			}
		}

		bool isRemoteMachineWindows() const;
		bool isRemoteMachineUnix() const ;
		bool isVmTypeVmWare() const { return vmType == VM_TYPE::VMWARE; }
		bool isVmTypePhysical() const { return vmType == VM_TYPE::PHYSICAL; }

		bool UseVmwareBasedInstall() const
		{
			return m_config->isVmWarePushInstallationEnabled() &&
				isVmTypeVmWare();
		}

		bool UseWMISSHBasedInstall() const
		{
			return true;
		}

		void SetInstallMode(InstallationMode installMode) 
		{
			installationMode = installMode; 
		}

		std::string GetInstallMode() const
		{
			if (this ->installationMode == InstallationMode::VmWare)
			{
				return "VmWare";
			}
			else if (this -> installationMode == InstallationMode::WMISSH)
			{
				return "WMISSH";
			}
			else
			{
				throw std::runtime_error("Unknown install mode.");
			}
		}

		std::string osname() const;
		std::string UAOrPatchLocalPath() const;
		std::string pushclientLocalPath() const;
		std::string jobId() const { return jobid; }
		std::string userName() const;
		std::string domainName() const;
		std::string remoteFolder() const;
		std::string remotePidPath() const;
		std::string remoteLogPath() const;
		std::string remoteInstallerJsonPath() const;
		std::string remoteInstallerExtendedErrorsJsonPath() const;
		std::string remoteConfiguratorJsonPath() const;
		std::string remoteErrorLogPath() const;
		std::string remoteExitStatusPath() const;
		std::string remoteUaPath() const;
		std::string remoteUaPathWithoutGz() const;
		std::string remotePushClientPath() const;
		std::string remotePushClientPathWithoutGz() const;
		std::string remotePushClientExecutableAfterExtraction() const;
		std::string remotePushClientInvocation() const;
		std::string remoteOsScriptInvocation() const;
		std::string remoteInstallScriptPath() const;
		std::string remoteInstallerScriptPathOnUnix() const;
		std::string remoteInstallationPath() const;
		std::string remoteConnectionPassPhrasePath() const;
		std::string remoteMobilityAgentConfigurationFilePath() const;
		std::string remoteCertPath() const;
		std::string remoteFingerPrintPath() const;
		std::string remoteEncryptionPasswordFile() const;
		std::string remoteEncryptionKeyFile() const;
		std::string remoteSpecFile() const;
		std::string remoteCommandToExtract() const;
		std::string remoteCommandToRun() const;
		std::string remoteCommandToConfigure() const;
		std::string isrebootRemoteMachine() const;
		std::string remoteCommandToGetBiosId() const;
		std::string verifySignatureFlag() const;
		std::string SignVerificationEnabledOnRemoteMachine() const;
		std::string prescript()  const { return std::string(); }
		std::string postscript()  const{ return std::string(); }
		std::string jobType()  const;

		std::string PITelemetryFolder() const;
		std::string PIVerboseLogFileName() const;
		std::string PITemporaryVerboseLogFilePath() const;
		std::string PITemporarySummaryLogFilePath() const;
		std::string PITemporaryVmWareSummaryLogFilePath() const;
		std::string PITemporaryWMISSHSummaryLogFilePath() const;
		std::string PITemporaryVmWareSummaryLogFilePathInTelemetryUploadFolder() const;
		std::string PITemporaryWMISSHSummaryLogFilePathInTelemetryUploadFolder() const;
		std::string PITemporaryVerboseLogFilePathInTelemetryUploadFolder() const;
		std::string PISummaryLogFileName() const;
		std::string AgentTelemetryFolder() const;
		std::string AgentTemporaryMetadataFilePath() const;
		std::string AgentTemporaryInstallSummaryFilePath() const;
		std::string AgentTemporaryVerboseLogFilePath() const;
		std::string AgentTemporaryErrorLogFilePath() const;
		std::string AgentTemporaryExitStatusFilePath() const;
		std::string AgentTemporaryConfigurationSummaryFilePath() const;
		std::string TemporaryFolder() const;
		std::string PITemporaryFolder() const;
		std::string PITemporaryUploadFolder() const;
		std::string AgentTemporaryFolder() const;
		std::string PITemporaryFilePath(const std::string & prefix) const;
		std::string PILocalMetadataPath() const;
		std::string PILocalMetadataPathInTelemetryUploadFolder() const;
		std::string AgentLocalMetadataPath() const;
		std::string LocalPushclientSpecFile() const;
		std::string LocalEncryptedPasswordFile() const;
		std::string LocalEncryptionKeyFile() const;
		std::string LocalInstallerScriptPathForUnix() const;
		std::string LocalInstallerLogPath() const;
		std::string LocalInstallerErrorLogPath() const;
		std::string LocalInstallerExitStatusPath() const;
		std::string LocalInstallerJsonPath() const;
		std::string LocalInstallerExtendedErrorsJsonPath() const;
		std::string LocalConfiguratorJsonPath() const;
		VmWareWrapperCmdInput VmWareApiOsCmdInput() const;
		VmWareWrapperCmdInput InstallMobilityAgentCmdInput() const;

		std::string GetPassphraseTemporaryFilePath() const;

		void verifyMandatoryFields();
		void DeleteTemporaryPushInstallFiles();
		void DeleteTemporaryFile(boost::filesystem::path filePath);
		void MoveTelemetryFilesToTemporaryTelemetryUploadFolder();
		void RenameFileWithRetries(std::string &srcPath, std::string &destPath, int retries);
	};



}

#endif
