//---------------------------------------------------------------
//  <copyright file="pushinstallvmware.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines routines for push install using vmware APIs.
//  </summary>
//
//  History:     15-Nov-2017    Jaysha    Created
//----------------------------------------------------------------


#include <fstream>
#include "logger.h"
#include "portable.h"
#include "cdputil.h"
#include "getpassphrase.h"
#include "errorexception.h"
#include "remoteapi.h"
#include "remoteconnection.h"
#include "pushconfig.h"
#include "PushInstallSWMgrBase.h"
#include "pushjobspec.h"
#include "pushjob.h"
#include "pushinstalltelemetry.h"
#include "credentialerrorexception.h"
#include "host.h"
#include "converterrortostringminor.h"
#include "errorexceptionmajor.h"
#include "errormessage.h"
#include "nonretryableerrorexception.h"
#include "securityutils.h"
#include "appcommand.h"
#include "MobilityAgentNotFoundException.h"
#include "PushClientNotFoundException.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include <map>

using namespace remoteApiLib;

namespace PI 
{
	VmWareBasedInstallStatus PushJob::PushJobUsingVmWareApis()
	{
		VmWareBasedInstallStatus vmWareBasedInstallStatus = 
			VmWareBasedInstallStatus::Failed;
		PushinstallTelemetry pushinstallTelemetry;

		try
		{
			DebugPrintf(SV_LOG_DEBUG, "Attempting push install using vmware APIs.\n");
			pushinstallTelemetry.RecordJobStarted();
			m_jobdefinition.SetInstallMode(InstallationMode::VmWare);
			this->RunPreReqChecks();

			std::string os = m_jobdefinition.getOptionalParam(PI_OSNAME_KEY);
			if (os == std::string() || os == "null")
			{
				this->FetchOsNameUsingVmWareApis();
			}

			int installStatus = 0;
			if (m_jobdefinition.jobtype == PushJobType::fetchDistroDetails)
			{
				m_jobdefinition.jobStatus = PUSH_JOB_STATUS::INSTALL_JOB_COMPLETED;
				goto JobExecutionCompletionLabel;
			}

			this->FetchJobDetails();
			this->DownloadSoftwareFromCS();
			this->VerifySoftwareSignature();
			this->InstallMobilityAgentUsingVmWareApis();
			installStatus = this->GetInstallationStatus();

			JobExecutionCompletionLabel: 
			this->HandleInstallStatus(installStatus);
			pushinstallTelemetry.RecordJobSucceeded();
			DebugPrintf(SV_LOG_DEBUG, "Push install succeeded.\n");
		}
		catch (PushInstallException & pe)
		{
			vmWareBasedInstallStatus = VmWareBasedInstallStatus::Failed;
			m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
			this->UpdateInstallationFailureToCS(pe);
			pushinstallTelemetry.RecordJobFailed(pe);
		}
		catch (PushInstallFallbackException &e)
		{
			vmWareBasedInstallStatus = VmWareBasedInstallStatus::FailedWithFallbackError;
			pushinstallTelemetry.RecordJobFailed(e);

			DebugPrintf(SV_LOG_ERROR,
				"Push install failed with error code %s, error message %s.\n",
				e.ErrorCode.c_str(),
				e.ErrorMessage.c_str());
		}
		catch (std::exception & e)
		{
			vmWareBasedInstallStatus = VmWareBasedInstallStatus::FailedWithFallbackError;
			pushinstallTelemetry.RecordJobFailed(e);
			DebugPrintf(SV_LOG_ERROR,
				"Push install failed with error message %s.\n",
				e.what());
		}
		catch (...)
		{
			vmWareBasedInstallStatus = VmWareBasedInstallStatus::FailedWithFallbackError;
			pushinstallTelemetry.RecordJobFailed();
			DebugPrintf(SV_LOG_ERROR,
				"Push install failed with unhandled exception.\n");
		}

		pushinstallTelemetry.SendTelemetryData(m_jobdefinition);

		return vmWareBasedInstallStatus;
	}

	void PushJob::RunPreReqChecks()
	{
		// check if the provided credentials are root creds in case of linux
		remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
		if (os_id == os_idx::unix_idx && m_jobdefinition.userName() != "root") {
			DebugPrintf(SV_LOG_ERROR, "Root username not provided for linux push installation.\n");
			ThrowLinuxNonRootCreds();
		}
	}

	void PushJob::FetchOsNameUsingVmWareApis()
	{
		DebugPrintf(SV_LOG_DEBUG, "Discovering remote machine operating system type...\n");

		if (m_jobdefinition.isRemoteMachineWindows())
		{
			m_jobdefinition.setOptionalParam(PI_OSNAME_KEY, "Win");
			DebugPrintf(SV_LOG_DEBUG, "Remote machine operating system is Windows.\n");
			return;
		}

		std::string cmdOutput = this->RunVmWareWrapperCommand(
			m_jobdefinition.VmWareApiOsCmdInput());
		this->ParseOsDetails(cmdOutput);
	}

	void PushJob::ParseOsDetails(const std::string & cmdOutput)
	{
		bool isOsSupported = false;
		std::string osName = "";

		std::stringstream exceptionMsg;
		exceptionMsg << "Failed to identify the OS version of remote machine "
			<< m_jobdefinition.remoteip
			<< "("
			<< m_jobdefinition.vmName
			<< ")"
			<< " from output "
			<< cmdOutput;

		if (cmdOutput.empty())
		{
			DebugPrintf(SV_LOG_ERROR,
				"%s : %s.\n",
				FUNCTION_NAME,
				exceptionMsg.str().c_str());

			throw NON_RETRYABLE_ERROR_EXCEPTION << exceptionMsg.str();
		}

		std::string::size_type idx = cmdOutput.find_first_of(":");
		if (idx == std::string::npos)
		{
			DebugPrintf(SV_LOG_ERROR,
				"%s : %s.\n",
				FUNCTION_NAME,
				exceptionMsg.str().c_str());

			throw NON_RETRYABLE_ERROR_EXCEPTION << exceptionMsg.str();
		}

		osName = cmdOutput.substr(0, idx);
		if (osName.empty())
		{
			DebugPrintf(SV_LOG_ERROR,
				"%s : %s.\n",
				FUNCTION_NAME,
				exceptionMsg.str().c_str());

			throw NON_RETRYABLE_ERROR_EXCEPTION << exceptionMsg.str();
		}

		std::string remaining = cmdOutput.substr(idx + 1);
		boost::algorithm::trim(remaining);

		idx = remaining.find_first_of(":");
		if (idx == std::string::npos)
		{
			DebugPrintf(SV_LOG_ERROR,
				"%s : %s.\n",
				FUNCTION_NAME,
				exceptionMsg.str().c_str());

			throw NON_RETRYABLE_ERROR_EXCEPTION << exceptionMsg.str();
		}

		std::string fullOsName = remaining.substr(0, idx);
		remaining = remaining.substr(idx + 1);
		boost::algorithm::trim(remaining);

		int supportValue = boost::lexical_cast<int>(remaining);
		isOsSupported = (supportValue == 0);

		if (!isOsSupported)
		{
			DebugPrintf(SV_LOG_ERROR,
				"%s : Operating system %s is not supported.\n",
				FUNCTION_NAME,
				fullOsName.c_str());

			osName = fullOsName;
			ThrowOsNotSupportedException(osName);
		}
		m_jobdefinition.setOptionalParam(PI_OSNAME_KEY, osName);
	}

	void PushJob::FetchJobDetails()
	{
		std::string failureMessage = "";

		try
		{
			std::string currentStep = "Fetching job details from Configuration Server...";
			m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, currentStep);

			this->fillMissingFields(failureMessage /* out param */);
		}
		catch (MobilityAgentNotFoundException & mae)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s Exception: '%s'.\n Failure message: '%s'.\n",
				FUNCTION_NAME,
				mae.what(),
				failureMessage.c_str());

			ThrowMobilityAgentNotFoundException();
		}
		catch (PushClientNotFoundException & pce)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s Exception: '%s'.\n Failure message: '%s'.\n",
				FUNCTION_NAME,
				pce.what(),
				failureMessage.c_str());

			ThrowPushClientNotFoundException();
		}
		catch (std::exception & e)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s Exception: '%s'.\n Failure message: '%s'.\n",
				FUNCTION_NAME,
				e.what(),
				failureMessage.c_str());

			ThrowCsFetchJobDetailsFailedException();
		}
	}

	void PushJob::DownloadSoftwareFromCS()
	{
		try
		{
			this->downloadSW();
		}
		catch (std::exception & e)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s Exception: '%s'.\n",
				FUNCTION_NAME,
				e.what());

			ThrowSoftwareDownloadFromCSFailedException();
		}
	}

	void PushJob::VerifySoftwareSignature()
	{
		try
		{
			this->verifySW();
		}
		catch (std::exception & e)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s Exception: '%s'.\n",
				FUNCTION_NAME,
				e.what());

			ThrowSoftwareSignatureVerificationFailedException();
		}
	}

	void PushJob::InstallMobilityAgentUsingVmWareApis()
	{
		this->CreatePushClientSpecFile();
		if (m_jobdefinition.isRemoteMachineUnix())
		{
			this->CreateLocalInstallerScriptForUnix();
		}

		if (m_jobdefinition.isRemoteMachineWindows())
		{
			this->CreateEncryptedPasswordAndKeyFile();
		}

		std::string cmdOutput = this->RunVmWareWrapperCommand(
			m_jobdefinition.InstallMobilityAgentCmdInput());
	}

	void PushJob::CreatePushClientSpecFile()
	{
		bool rv = false;
		std::string jobSpecContent = preparePushClientJobSpec();

		ofstream filehandle(
			m_jobdefinition.LocalPushclientSpecFile(),
			ios::out | ios::binary);

		if (filehandle.good())
		{
			filehandle.write(jobSpecContent.c_str(), jobSpecContent.length());
			rv = filehandle.good();
			filehandle.close();
		}

		if (!rv)
		{
			std::string exceptionMsg = "Failed to create push client spec file.";
			DebugPrintf(SV_LOG_ERROR, "%s\n", exceptionMsg.c_str());
			std::map<std::string, std::string> placeHolders =
				std::map<std::string, std::string>();
			throw PushInstallFallbackException(
				"PI_INTERNAL_ERROR_SPECFILECREATIONFAILED",
				"PI_INTERNAL_ERROR_SPECFILECREATIONFAILED",
				exceptionMsg + m_jobdefinition.LocalPushclientSpecFile(),
				placeHolders);
		}
	}

	void PushJob::CreateEncryptedPasswordAndKeyFile()
	{
		bool rv = false;
		std::string encryptedPasswd, encryptionKey;
		encryptcredentials(encryptedPasswd, encryptionKey);

		ofstream encPassFilehandle(
			m_jobdefinition.LocalEncryptedPasswordFile(),
			ios::out | ios::binary);

		if (encPassFilehandle.good())
		{
			encPassFilehandle.write(encryptedPasswd.c_str(), encryptedPasswd.length());
			rv = encPassFilehandle.good();
			encPassFilehandle.close();
		}

		if (!rv)
		{
			std::string exceptionMsg = "Failed to create file ";
			exceptionMsg += m_jobdefinition.LocalEncryptedPasswordFile();
			exceptionMsg += ".";
			DebugPrintf(SV_LOG_ERROR, "%s\n", exceptionMsg.c_str());
			std::map<std::string, std::string> placeHolders =
				std::map<std::string, std::string>();
			throw PushInstallFallbackException(
				"PI_INTERNAL_ERROR_ENCRYPTIONPASSFILECREATIONFAILED",
				"PI_INTERNAL_ERROR_ENCRYPTIONPASSFILECREATIONFAILED",
				exceptionMsg + m_jobdefinition.LocalEncryptedPasswordFile(),
				placeHolders);
		}

		ofstream encKeyfilehandle(
			m_jobdefinition.LocalEncryptionKeyFile(),
			ios::out | ios::binary);

		if (encKeyfilehandle.good())
		{
			encKeyfilehandle.write(encryptionKey.c_str(), encryptionKey.length());
			rv = encKeyfilehandle.good();
			encKeyfilehandle.close();
		}

		if (!rv)
		{
			std::string exceptionMsg = "Failed to create file ";
			exceptionMsg += m_jobdefinition.LocalEncryptionKeyFile();
			exceptionMsg += ".";
			DebugPrintf(SV_LOG_ERROR, "%s\n", exceptionMsg.c_str());
			std::map<std::string, std::string> placeHolders =
				std::map<std::string, std::string>();
			throw PushInstallFallbackException(
				"PI_INTERNAL_ERROR_ENCRYPTIONKEYFILECREATIONFAILED",
				"PI_INTERNAL_ERROR_ENCRYPTIONKEYFILECREATIONFAILED",
				exceptionMsg + m_jobdefinition.LocalEncryptionKeyFile(),
				placeHolders);
		}
	}

	void PushJob::CreateLocalInstallerScriptForUnix()
	{
		bool rv = false;
		std::string scriptContent = preparePushJobScriptForUnix();

		ofstream filehandle(
			m_jobdefinition.LocalInstallerScriptPathForUnix(),
			ios::out | ios::binary);
		if (filehandle.good())
		{
			filehandle.write(scriptContent.c_str(), scriptContent.length());
			rv = filehandle.good();
			filehandle.close();
		}

		if (!rv)
		{
			std::string exceptionMsg = "Failed to create install script.";
			DebugPrintf(SV_LOG_ERROR, "%s\n", exceptionMsg.c_str());
			std::map<std::string, std::string> placeHolders =
				std::map<std::string, std::string>();
			throw PushInstallFallbackException(
				"PI_INTERNAL_ERROR_INSTALLSCRIPTFILECREATIONFAILED",
				"PI_INTERNAL_ERROR_INSTALLSCRIPTFILECREATIONFAILED",
				exceptionMsg + m_jobdefinition.LocalInstallerScriptPathForUnix(),
				placeHolders);
		}
	}

	int PushJob::GetInstallationStatus()
	{
		int installationStatus = 0;
		if (boost::filesystem::exists(m_jobdefinition.LocalInstallerExitStatusPath()))
		{
			std::ifstream ifs(m_jobdefinition.LocalInstallerExitStatusPath(), std::ios::binary);
			std::string content((std::istreambuf_iterator<char>(ifs)),
				(std::istreambuf_iterator<char>()));

			installationStatus = this->ParseExitStatus(content);
		}
		else
		{
			std::string errorMessage = m_jobdefinition.LocalInstallerExitStatusPath();
			errorMessage += "is not available.\n";
			errorMessage += "It is likely that the vmware wrapper failed to invoke the installation.\n";
			errorMessage += "Treating this is as vmware wrapper command internal error.";

			DebugPrintf(SV_LOG_ERROR, "%s\n", errorMessage.c_str());
			ThrowInstallerExitStatusFileNotAvailable();
		}

		return installationStatus;
	}

	int PushJob::ParseExitStatus(const std::string &content)
	{
		DebugPrintf(SV_LOG_DEBUG, "Exit status from push client: %s\n", content.c_str());

		int exitStatus = AGENT_INSTALLATION_SUCCESS;
		if (!content.empty())
		{
			std::string jobid;
			int prescriptExitCode = 0;
			int postscriptExitCode = 0;

			std::string::size_type idx = content.find_first_of(":");
			if (idx == std::string::npos)
			{
				throw ERROR_EXCEPTION << "internal error, push client did not return proper exit status (" << content << ")";
			}

			jobid = content.substr(0, idx);
			std::string remaining = content.substr(idx + 1);

			idx = remaining.find_first_of(":");
			if (idx == std::string::npos)
			{
				throw ERROR_EXCEPTION << "internal error, push client did not return proper exit status (" << content << ")";
			}

			prescriptExitCode = boost::lexical_cast<int>(remaining.substr(0, idx));
			remaining = remaining.substr(idx + 1);

			idx = remaining.find_first_of(":");
			if (idx == std::string::npos){
				throw ERROR_EXCEPTION << "internal error, push client did not return proper exit status (" << content << ")";
			}

			exitStatus = boost::lexical_cast<int>(remaining.substr(0, idx));
			remaining = remaining.substr(idx + 1);

			boost::algorithm::trim(remaining);
			postscriptExitCode = boost::lexical_cast<int>(remaining);
		}
		else
		{
			throw ERROR_EXCEPTION << "internal error, push client did not return any exit status";
		}

		return exitStatus;
	}

	void PushJob::HandleInstallStatus(int installStatus)
	{
		DebugPrintf(SV_LOG_DEBUG, "Installation completed with exit code :%d.\n", installStatus);

		if (AGENT_INSTALLATION_SUCCESS == installStatus ||
			AGENT_EXISTS_SAME_VERSION == installStatus)
		{
			this->UpdateInstallationSuccessToCS();
		}
		else
		{
			if (!m_jobdefinition.IsInstallationRequest())
			{
				this->ThrowUninstallFailed();
			}
			else
			{
				if (AGENT_INSTALL_COMPLETED_BUT_REQUIRES_REBOOT == installStatus)
				{
					this->ThrowAgentInstallCompletedRebootRequired();
				}

				else if (AGENT_EXISTS_DIFF_VERSION == installStatus)
				{
					this->ThrowAgentDifferentVersionAlreadyInstalled();
				}
				else if (AGENT_EXISTS_REGISTEREDWITH_DIFF_CS == installStatus)
				{
					this->ThrowAgentRegisteredWithDifferentCS();
				}
				else if (AGENT_INSTALL_FATAL_ERROR == installStatus)
				{
					this->ThrowAgentInstallerFatalError();
				}
				else if (AGENT_INSTALL_PREPARE_STAGE_FAILED == installStatus)
				{
					this->ThrowAgentInstallerPrepareStageFailed();
				}
				else if (AGENT_INSTALL_PREPARE_STATE_FAILED_REBOOTREQUIRED == installStatus)
				{
					this->ThrowAgentRebootPending();
				}
				else if (AGENT_INSTALL_INVALID_CS_IP_PORT_PASSPHRASE == installStatus)
				{
					this->ThrowInvalidCSDetails();
				}
				else if (AGENT_INSTALL_UNSUPPORTED_OS == installStatus)
				{
					this->ThrowInstallerUnsupportedOS();
				}
				else if (AGENT_INSTALL_SERVICES_STOP_FAILED == installStatus)
				{
					this->ThrowInstallerServiceStopFailed();
				}
				else if (AGENT_INSTALL_SERVICES_START_FAILED == installStatus)
				{
					this->ThrowInstallerServiceStartFailed();
				}
				else if (AGENT_INSTALL_HOST_ENTRY_NOTFOUND == installStatus)
				{
					this->ThrowInstallerHostEntryNotFound();
				}
				else if (AGENT_INSTALL_INSUFFICIENT_DISKSPACE == installStatus)
				{
					this->ThrowInstallerInsufficientDiskSpace();
				}
				else if (AGENT_INSTALL_CREATE_TEMPDIR_FAILED == installStatus)
				{
					this->ThrowInstallerCreateTmpFolderFailed();
				}
				else if (AGENT_INSTALLER_FILE_NOT_FOUND == installStatus)
				{
					this->ThrowInstallerFileNotFound();
				}
				else if (AGENT_INSTALL_PREV_INSTALL_DIR_DELETE_FAILED == installStatus)
				{
					this->ThrowInstallerPrevDirDeleteFailed();
				}
				else if (AGENT_INSTALL_PREV_INSTALL_REBOOT_PENDING == installStatus)
				{
					this->ThrowInstallerRebootPendingFromPrevInstall();
				}
				else if (AGENT_INSTALL_DOTNET35_MISSING == installStatus)
				{
					this->ThrowInstallerDotNet35Missing();
				}
				else if (AGENT_INSTALLED_REGISTER_FAILED == installStatus)
				{
					this->ThrowInstallerRegistrationFailed();
				}
				else if (AGENT_INSTALL_FAILED_WITH_VSS_ISSUE == installStatus)
				{
					this->ThrowInstallerVSSFailure();
				}
				else if (AGENT_INSTALLATION_INTERNAL_ERROR == installStatus)
				{
					this->ThrowInstallerInternalError();
				}
				else if (AGENT_INSTALL_INVALID_PARAMS == installStatus)
				{
					this->ThrowInstallerInvalidParams();
				}
				else if (AGENT_INSTALL_INVALID_COMMANDLINE_ARGUMENTS == installStatus)
				{
					this->ThrowInstallerInvalidCommandLineArguments();
				}
				else if (AGENT_INSTALLER_ALREADY_RUNNING == installStatus)
				{
					this->ThrowInstallerAlreadyRunning();
				}
				else if (AGENT_INSTALLER_OS_MISMATCH == installStatus)
				{
					this->ThrowInstallerOSMismatch();
				}
				else if (AGENT_INSTALL_MISSING_PARAMS == installStatus)
				{
					this->ThrowInstallerMissingParams();
				}
				else if (AGENT_INSTALL_PASSPHRASE_NOT_FOUND == installStatus)
				{
					this->ThrowInstallerPassphraseFileMissing();
				}
				else if (AGENT_INSTALL_VX_SERVICE_STOP_FAIL == installStatus ||
					AGENT_INSTALL_FX_SERVICE_STOP_FAIL == installStatus)
				{
					this->ThrowInstallerSpecificServiceStopFailed();
				}
				else if (AGENT_INSTALL_INSUFFICIENT_PRIVILEGE == installStatus)
				{
					this->ThrowInstallerInsufficientPrivelege();
				}
				else if (AGENT_INSTALL_PREV_INSTALL_NOT_FOUND == installStatus)
				{
					this->ThrowInstallerPrevInstallNotFound();
				}
				else if (AGENT_INSTALL_KERNEL_NOT_SUPPORTED == installStatus)
				{
					this->ThrowInstallerUnsupportedKernel();
				}
				else if (AGENT_INSTALL_LINUX_VERSION_NOT_SUPPORTED == installStatus ||
					AGENT_INSTALL_SLES_VERSION_NOT_SUPPORTED == installStatus)
				{
					this->ThrowInstallerLinuxVersionNotSupported();
				}
				else if (AGENT_INSTALL_BOOT_DRIVER_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerBootupDriverNotAvailable();
				}
				else if (AGENT_INSTALL_FAILED_ON_UNIFIED_SETUP == installStatus)
				{
					this->ThrowInstallerCSInstalled();
				}
				else if (AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT == installStatus)
				{
					this->ThrowInstallerRebootRecommended();
				}
				else if (AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT_LINUX == installStatus)
				{
					this->ThrowInstallerLinuxRebootRecommended();
				}
				else if (AGENT_INSTALL_CURRENT_KERNEL_NOT_SUPPORTED == installStatus)
				{
					this->ThrowInstallerCurrentKernelNotSupported();
				}
				else if (AGENT_INSTALL_LIS_COMPONENTS_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerLISComponentsMissing();
				}
				else if (AGENT_INSTALL_INSUFFICIENT_SPACE_IN_ROOT == installStatus)
				{
					this->ThrowInstallerInsufficientSpaceOnRoot();
				}
				else if (AGENT_INSTALL_SPACE_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerSpaceNotAvailable();
				}
				else if (AGENT_INSTALL_VMWARE_TOOLS_NOT_RUNNING == installStatus)
				{
					this->ThrowInstallerVMwareToolsNotRunning();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED == installStatus)
				{
					this->ThrowInstallerVSSInstallationFailed();
				}
				else if (AGENT_INSTALL_DRIVER_MANIFEST_INSTALLATION_FAILED == installStatus)
				{
					this->ThrowInstallerDriverManifestInstallationFailed();
				}
				else if (AGENT_INSTALL_MASTER_TARGET_EXISTS == installStatus)
				{
					this->ThrowInstallerMasterTargetExists();
				}
				else if (AGENT_INSTALL_PLATFORM_MISMATCH == installStatus)
				{
					this->ThrowInstallerPlatformMismatch();
				}
				else if (AGENT_INSTALL_DRIVER_ENTRY_REGISTRY_UPDATE_FAILED == installStatus)
				{
					this->ThrowRegistryEntryUpdateFailed();
				}
				else if (AGENT_INSTALL_CERT_GEN_FAIL == installStatus)
				{
					this->ThrowInstallerGenCertFailed();
				}
				else if (AGENT_INSTALL_DISABLE_SERVICE_FAIL == installStatus)
				{
					this->ThrowInstallerDisableLmv2ServiceFailed();
				}
				else if (AGENT_INSTALL_UPGRADE_PLATFORM_CHANGE_NOT_SUPPORTED == installStatus)
				{
					this->ThrowInstallerUpgradePlatformChangeNotSupported();
				}
				else if (AGENT_INSTALL_HOST_NAME_NOT_FOUND == installStatus)
				{
					this->ThrowInstallerHostNameNotFound();
				}
				else if (AGENT_INSTALL_FAILED_OUT_OF_MEMORY == installStatus)
				{
					this->ThrowInstallerOutOfMemory();
				}
				else if (AGENT_INSTALL_INSTALLATION_DIRECTORY_ABS_PATH_NEEDED == installStatus)
				{
					this->ThrowInstallerAbsInstallationDirectoryPathNotPresent();
				}
				else if (AGENT_INSTALL_PARTITION_SPACE_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerPartitionSpaceNotAvailable();
				}
				else if (AGENT_INSTALL_OPERATING_SYSTEM_NOT_SUPPORTED == installStatus)
				{
					this->ThrowInstallerOperatingSystemNotSupported();
				}
				else if (AGENT_INSTALL_THIRDPARTY_NOTICES_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerThirdPartyNoticeNotAvailable();
				}
				else if (AGENT_INSTALL_LOCALE_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerLocaleNotAvailable();
				}
				else if (AGENT_INSTALL_RPM_COMMAND_NOT_AVAILABLE == installStatus)
				{
					this->ThrowInstallerRpmCommandNotAvailable();
				}
				else if (AGENT_INSTALL_CONFIG_DIRECTORY_CREATION_FAILED == installStatus)
				{
					this->ThrowInstallerConfigDirectoryCreationFailed();
				}
				else if (AGENT_INSTALL_CONF_FILE_COPY_FAILED == installStatus)
				{
					this->ThrowInstallerConfFileCopyFailed();
				}
				else if (AGENT_INSTALL_RPM_INSTALLATION_FAILED == installStatus)
				{
					this->ThrowInstallerRpmInstallationFailed();
				}
				else if (AGENT_INSTALL_TEMP_FOLDER_CREATION_FAILED == installStatus)
				{
					this->ThrowInstallerTempFolderCreationFailed();
				}
				else if (AGENT_INSTALL_DRIVER_INSTALLATION_FAILED == installStatus)
				{
					this->ThrowInstallerDriverInstallationFailed();
				}
				else if (AGENT_INSTALL_CACHE_FOLDER_CREATION_FAILED == installStatus)
				{
					this->ThrowInstallerCacheFolderCreationFailed();
				}
				else if (AGENT_INSTALL_SNAPSHOT_DRIVER_IN_USE == installStatus)
				{
					this->ThrowInstallerSnapshotDriverInUse();
				}
				else if (AGENT_INSTALL_SNAPSHOT_DRIVER_UNLOAD_FAILED == installStatus)
				{
					this->ThrowInstallerSnapshotDriverUnloadFailed();
				}
				else if (AGENT_INSTALL_INVALID_VX_INSTALLATION_OPTIONS == installStatus)
				{
					this->ThrowInstallerInvalidUxInstallationOptions();
				}
				else if (AGENT_INSTALL_MUTEX_ACQUISITION_FAILED == installStatus)
				{
					this->ThrowInstallerMutexAcquisitionFailed();
				}
				else if (AGENT_INSTALL_LINUX_INSUFFICIENT_DISK_SPACE == installStatus)
				{
					this->ThrowInstallerLinuxInsufficientDiskSpace();
				}
				else if (AGENT_SETUP_FAILED_WITH_ERRORS == installStatus)
				{
					this->ThrowInstallerVSSInstallationFailedWithErrors();
				}
				else if (AGENT_SETUP_COMPLETED_WITH_WARNINGS == installStatus)
				{
					this->ThrowInstallerVSSInstallationFailedWithWarnings();
				}
				else if (AGENT_INSTALL_PREREQS_FAIL == installStatus)
				{
					this->ThrowInstallerPreReqsFailed();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_DATABASE_LOCKED == installStatus)
				{
					this->ThrowInstallerVSSInstallDatabaseLocked();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_ALREADY_EXISTS == installStatus)
				{
					this->ThrowInstallerVSSInstallServiceAlreadyExists();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_MARKED_FOR_DELETION == installStatus)
				{
					this->ThrowInstallerVSSInstallServiceMarkedForDeletion();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_CSSCRIPT_ACCESS_DENIED == installStatus)
				{
					this->ThrowInstallerVSSInstallCSScriptAccessDenied();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_PATH_NOT_FOUND == installStatus)
				{
					this->ThrowInstallerVSSInstallPathNotFound();
				}
				else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_OUT_OF_MEMORY == installStatus)
				{
					this->ThrowInstallerVSSInstallOutOfMemory();
				}
				else if (AGENT_INSTALL_MSI_INSTALLATION_FAILED == installStatus)
				{
					this->ThrowInstallerMSIInstallationFailed();
				}
				else if (AGENT_INSTALL_FAILED_TO_SET_SERVICE_STARTUP_TYPE == installStatus)
				{
					this->ThrowInstallerSetServiceStartupTypeFailed();
				}
				else if (AGENT_INSTALL_INITRD_BACKUP_FAILED == installStatus)
				{
					this->ThrowInstallerInitRdBackupFailed();
				}
				else if (AGENT_INSTALL_INITRD_RESTORE_FAILED == installStatus)
				{
					this->ThrowInstallerInitRdRestoreFailed();
				}
				else if (AGENT_INSTALL_INITRD_IMAGE_UPDATE_FAILED == installStatus)
				{
					this->ThrowInstallerInitRdImageUpdateFailed();
				}
				else if (AGENT_INSTALL_FAILED_TO_CREATE_SYMLINKS == installStatus)
				{
					this->ThrowInstallerCreateSymlinksFailed();
				}
				else if (AGENT_INSTALL_FAILED_TO_INSTALL_AND_CONFIGURE_SERVICES == installStatus)
				{
					this->ThrowInstallerInstallAndConfigureServicesFailed();
				}
				else if (PI_BIOS_ID_MISMATCH == installStatus)
				{
					this->ThrowBiosIdMismatchError();
				}
				else if (PI_FQDN_MISMATCH == installStatus)
				{
					this->ThrowFqdnMismatchException();
				}
				else
				{
					this->ThrowAgentInstallerFatalError();
				}
			}
		}
	}

	void PushJob::UpdateInstallationSuccessToCS()
	{
		std::string output;
		std::string errors;
		std::string logMessage = "";

		if (boost::filesystem::exists(m_jobdefinition.LocalInstallerLogPath()))
		{
			std::ifstream ifs(m_jobdefinition.LocalInstallerLogPath(), std::ios::binary);
			output.assign((std::istreambuf_iterator<char>(ifs)),
				(std::istreambuf_iterator<char>()));
		}
		else
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s not found.\n",
				m_jobdefinition.LocalInstallerLogPath().c_str());
		}

		if (boost::filesystem::exists(m_jobdefinition.LocalInstallerErrorLogPath()))
		{
			std::ifstream ifs(m_jobdefinition.LocalInstallerErrorLogPath(), std::ios::binary);
			errors.assign((std::istreambuf_iterator<char>(ifs)),
				(std::istreambuf_iterator<char>()));
		}
		else
		{
			DebugPrintf(
				SV_LOG_DEBUG,
				"%s not found.\n",
				m_jobdefinition.LocalInstallerErrorLogPath().c_str());
		}

		if (!output.empty()){
			logMessage += "push client log:\n";
			logMessage += "----------------\n";
			logMessage += output;
			logMessage += "----------------\n";
		}

		if (!errors.empty()){
			logMessage += "push client errors:\n";
			logMessage += "----------------\n";
			logMessage += errors;
			logMessage += "----------------\n";
		}

		if (m_jobdefinition.IsInstallationRequest())
		{
			m_jobdefinition.jobStatus = INSTALL_JOB_COMPLETED;
		}
		else
		{
			m_jobdefinition.jobStatus = UNINSTALL_JOB_COMPLETED;
		}

		DebugPrintf(
			SV_LOG_DEBUG,
			"Sending installation success message to configuration server. Message: %s\n",
			logMessage.c_str());

		if (!m_proxy->UpdateAgentInstallationStatus(
			m_jobdefinition.jobId(),
			m_jobdefinition.jobStatus,
			"Job completed.",
			"UNKNOWN-HOST-ID",
			logMessage,
			"",
			PI::PlaceHoldersMap(),
			"",
			m_jobdefinition.getOptionalParam(PI_OSNAME_KEY)))
		{
			std::map<std::string, std::string> placeHolders =
				std::map<std::string, std::string>();
			throw PushInstallFallbackException(
				"PI_INTERNAL_ERROR_CSCOMMUNICATIONFAILED",
				"PI_INTERNAL_ERROR_CSCOMMUNICATIONFAILED",
				"Failure in sending success to the configuration server.",
				placeHolders);
		}
	}

	void PushJob::UpdateInstallationFailureToCS(PushInstallException & pe)
	{
		try
		{
			DebugPrintf(SV_LOG_ERROR,
				"Push install failed with error code %s, error message %s.\n",
				pe.ErrorCode.c_str(),
				pe.ErrorMessage.c_str());

			std::string output;
			std::string errors;
			std::string logMessage = "";

			if (boost::filesystem::exists(m_jobdefinition.LocalInstallerLogPath()))
			{
				std::ifstream ifs(m_jobdefinition.LocalInstallerLogPath(), std::ios::binary);
				output.assign((std::istreambuf_iterator<char>(ifs)),
					(std::istreambuf_iterator<char>()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"%s not found.\n",
					m_jobdefinition.LocalInstallerLogPath().c_str());
			}

			if (boost::filesystem::exists(m_jobdefinition.LocalInstallerErrorLogPath()))
			{
				std::ifstream ifs(m_jobdefinition.LocalInstallerErrorLogPath(), std::ios::binary);
				errors.assign((std::istreambuf_iterator<char>(ifs)),
					(std::istreambuf_iterator<char>()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"%s not found.\n",
					m_jobdefinition.LocalInstallerErrorLogPath().c_str());
			}

			if (!output.empty()){
				logMessage += "push client log:\n";
				logMessage += "----------------\n";
				logMessage += output;
				logMessage += "----------------\n";
			}

			if (!errors.empty()){
				logMessage += "push client errors:\n";
				logMessage += "----------------\n";
				logMessage += errors;
				logMessage += "----------------\n";
			}

			if (m_jobdefinition.IsInstallationRequest())
			{
				m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
			}
			else
			{
				m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;
			}

			DebugPrintf(SV_LOG_DEBUG, "Sending installation failure message to configuration server.");

			bool csUpdateReturnStatus = false;

			if (pe.ErrorCode == "EP0915" || pe.ErrorCode == "EP0964" || pe.ErrorCode == "EP0965")
			{
				std::ifstream ifs(m_jobdefinition.LocalInstallerExtendedErrorsJsonPath(), std::ios::binary);

				std::string content("");
				if (!ifs.fail())
				{
					content = std::string((std::istreambuf_iterator<char>(ifs)),
						(std::istreambuf_iterator<char>()));
				}
				else
				{
					DebugPrintf(
						SV_LOG_ERROR,
						"Reading installer extended errors file %s failed with error %s.",
						m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str(),
						strerror(errno));
				}

				DebugPrintf(SV_LOG_DEBUG, "Agent installation pre-req failure json content: %s.\n", content.c_str());

				std::string errorCode = pe.ErrorCode;
				if (content == "")
				{
					if (errorCode == "EP0915")
					{
						errorCode = "EP2003";
					}
					else
					{
						errorCode = "EP2004";
					}

					pe.PlaceHolders[errCode] = errorCode;
				}

				csUpdateReturnStatus = m_proxy->UpdateAgentInstallationStatus(
					m_jobdefinition.jobId(),
					m_jobdefinition.jobStatus,
					pe.ErrorMessage,
					"UNKNOWN-HOST-ID",
					logMessage,
					pe.ErrorCode,
					pe.PlaceHolders,
					content);
			}
			else
			{
				csUpdateReturnStatus = m_proxy->UpdateAgentInstallationStatus(
					m_jobdefinition.jobId(),
					m_jobdefinition.jobStatus,
					pe.ErrorMessage,
					"UNKNOWN-HOST-ID",
					logMessage,
					pe.ErrorCode,
					pe.PlaceHolders);
			}
			if (!csUpdateReturnStatus)
			{
				DebugPrintf(SV_LOG_ERROR, "Failure in sending failure message to configuration server.\n");
			}
		}
		catch (std::exception &e)
		{
			DebugPrintf(SV_LOG_ERROR, "%s exception: %s.\n", FUNCTION_NAME, e.what());
		}
	}

	void PushJob::HandleVMwareFailureError(boost::property_tree::ptree & pTree)
	{
		std::string status = pTree.get<std::string>("Status");
		std::string errorCode = pTree.get<std::string>("ErrorCode");
		std::string errorMessage = pTree.get<std::string>("ErrorMessage");
		std::string componentErrorCode = pTree.get<std::string>("ComponentErrorCode");

		std::map<std::string, std::string> placeHolders =
			std::map<std::string, std::string>();

		throw PushInstallFallbackException(
			errorCode,
			errorCode,
			errorMessage,
			componentErrorCode,
			placeHolders);
	}

	std::string PushJob::RunVmWareWrapperCommand(const VmWareWrapperCmdInput & input)
	{
		SV_ULONG lExitCode = SVS_OK;
		std::string cmdOutput;
		std::string cmd = input.cmd;

		DebugPrintf(SV_LOG_DEBUG, "Running command '{%s}'\n", cmd.c_str());

		AppCommand command(
			cmd,
			m_config->vmWareCmdTimeOutInSecs(),
			"",
			false,
			false);

		std::string appCmdOutputIgnored;
		bool  bProcessActive = true;
		SVERROR ret = command.Run(lExitCode, appCmdOutputIgnored, bProcessActive);

		if (boost::filesystem::exists(input.logFilepath))
		{
			std::string cmdLogOutput;
			std::ifstream ifs(input.logFilepath, std::ios::binary);
			cmdLogOutput.assign((std::istreambuf_iterator<char>(ifs)),
				(std::istreambuf_iterator<char>()));
			ifs.close();

			DebugPrintf(SV_LOG_DEBUG, "Verbose log: %s\n", cmdLogOutput.c_str());
			
			boost::system::error_code ec;
			if (!boost::filesystem::remove(input.logFilepath, ec))
			{
				DebugPrintf(SV_LOG_ERROR,
					"%s: failed to delete file %s with error %s\n",
					FUNCTION_NAME,
					input.logFilepath.c_str(),
					ec.message().c_str());
			}
		}
		else
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s not found.\n",
				input.logFilepath.c_str());
		}

		// DebugPrintf(SV_LOG_DEBUG, "App command console output:\n%s\n", appCmdOutputIgnored.c_str());

		if (ret.failed())
		{
			std::stringstream exceptionMsg;
			exceptionMsg << "Failure in spawning command "
				<< cmd;

			DebugPrintf(SV_LOG_ERROR,
				"%s : %s.\n",
				FUNCTION_NAME,
				exceptionMsg.str().c_str());

			throw NON_RETRYABLE_ERROR_EXCEPTION << exceptionMsg.str();
		}

		if (!this->CommandSuceeded(lExitCode))
		{
			std::stringstream exceptionMsg;
			exceptionMsg << "Command "
				<< cmd
				<< " failed with exit code "
				<< lExitCode;

			DebugPrintf(SV_LOG_ERROR,
				"%s : %s.\n",
				FUNCTION_NAME,
				exceptionMsg.str().c_str());

			if (boost::filesystem::exists(input.summaryLogFilepath))
			{
				std::string cmdExecutionSummary;
				std::ifstream ifs(input.summaryLogFilepath, std::ios::binary);
				cmdExecutionSummary.assign((std::istreambuf_iterator<char>(ifs)),
					(std::istreambuf_iterator<char>()));
				ifs.close();

				DebugPrintf(
					SV_LOG_DEBUG,
					"Command execution summary: %s\n",
					cmdExecutionSummary.c_str());

				boost::system::error_code ec;
				if (!boost::filesystem::remove(input.summaryLogFilepath, ec))
				{
					DebugPrintf(SV_LOG_ERROR,
						"%s: failed to delete file %s with error %s\n",
						FUNCTION_NAME,
						input.logFilepath.c_str(),
						ec.message().c_str());
				}

				boost::property_tree::ptree pt;
				std::stringstream ss;
				ss << cmdExecutionSummary;
				boost::property_tree::read_json(ss, pt);

				this->HandleVMwareFailureError(pt);
			}
			else
			{
				DebugPrintf(
					SV_LOG_ERROR,
					"%s not found.\n",
					input.logFilepath.c_str());

				throw NON_RETRYABLE_ERROR_EXCEPTION << exceptionMsg.str();
			}
		}

		if (boost::filesystem::exists(input.outputFilepath))
		{
			std::ifstream ifs(input.outputFilepath, std::ios::binary);
			cmdOutput.assign((std::istreambuf_iterator<char>(ifs)),
				(std::istreambuf_iterator<char>()));
			ifs.close();

			DebugPrintf(
				SV_LOG_DEBUG,
				"Command output: %s\n",
				cmdOutput.c_str());

			boost::system::error_code ec;
			if (!boost::filesystem::remove(input.outputFilepath, ec))
			{
				DebugPrintf(SV_LOG_ERROR,
					"%s: failed to delete file %s with error %s\n",
					FUNCTION_NAME,
					input.outputFilepath.c_str(),
					ec.message().c_str());
			}
		}
		else
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"%s not found.\n",
				input.outputFilepath.c_str());
		}
		
		return cmdOutput;
	}

}
