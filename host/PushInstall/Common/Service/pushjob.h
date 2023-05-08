///
/// \file pushjob.h
///
/// \brief
///


#ifndef INMAGE_PI_JOB_H
#define INMAGE_PI_JOB_H

#include <map>
#include <string>

#include <ace/ACE.h>

#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

#include "svtypes.h"
#include "supportedplatforms.h"

#include "PushInstallCommunicationBase.h"
#include "pushInstallationSettings.h"
#include "pushconfig.h"
#include "pushjobdefinition.h"
#include "pushinstallexception.h"
#include "pushinstallfallbackexception.h"
#include "PushInstallSWMgrBase.h"

namespace PI {

	enum VmWareBasedInstallStatus { NotAttempted, Succeeded, Failed, FailedWithFallbackError};

	class PushJob
	{
	public:
		PushJob(PushJobDefinition & jobdefinition, PushInstallCommunicationBasePtr & proxyImpl, PushConfigPtr & configImpl, PushInstallSWMgrBasePtr & softwareMgrImpl)
			:m_jobdefinition(jobdefinition)
		{
			m_proxy = proxyImpl;
			m_config = configImpl;
			m_swmgr = softwareMgrImpl;
		}

		~PushJob() 
		{
			
		}
		void execute();

	protected:

	private:

		VmWareBasedInstallStatus PushJobUsingVmWareApis();
		void PushJobUsingWMISSHApis();

		std::string osName(std::string &error_code, std::map<std::string, std::string> &placeHolders, std::string &errMessageToDisplay);

		void downloadSW();
		void verifySW();
		std::string preparePushClientJobSpec();
		std::string preparePushJobScriptForUnix();
		void copyFilesToRemoteMachine();
		void executeJobOnRemoteMachine();
		void consumeExitStatus(std::string & output, int & exitStatus, std::string & installerPreReqErrorsJson);
		std::string GetSourceMachineIpv4AddressesAndFqdn(std::string& ipAddresses);
		void getRemoteMachineIp(const std::string &ipAddresses, std::string &reachableIpAddress);
		bool isRemoteMachineReachable(std::string &ipAddress);
		void installMobilityService(std::string & errorCode, std::map<std::string, std::string> &placeHolders, std::string &errMessageToDisplay);

		void PullAgentTelemetryFiles();
		void PullAgentTelemetryFile(const std::string & remoteFilepath, const std::string & localFilepath);

#ifdef WIN32
		void encryptcredentials(std::string & encrypted_passwd, std::string & encryption_key);
#endif

// start region: vmware based installation routines.

		void RunPreReqChecks();
		void FetchOsNameUsingVmWareApis();
		void ParseOsDetails(const std::string & cmdOutput);

		void FetchJobDetails();
		void fillMissingFields(std::string &failureMessage);

		void DownloadSoftwareFromCS();

		void VerifySoftwareSignature();

		void InstallMobilityAgentUsingVmWareApis();
		void CreatePushClientSpecFile();
		void CreateEncryptedPasswordAndKeyFile();
		void CreateLocalInstallerScriptForUnix();

		int GetInstallationStatus();
		int ParseExitStatus(const std::string &content);
		void HandleInstallStatus(int installStatus);
		void UpdateInstallationSuccessToCS();
		void UpdateInstallationFailureToCS(PushInstallException & pe);

		std::string RunVmWareWrapperCommand(const VmWareWrapperCmdInput & input);
		bool CommandSuceeded(SV_ULONG & exitCode) { return exitCode == 0; }

		void HandleVMwareFailureError(boost::property_tree::ptree & pTree);

		void FillPlaceHolders(const std::string & errorCode, std::map<std::string, std::string> & placeHolders);
		void ThrowOsVersionIdentificationFailedException();
		void ThrowOsNotSupportedException(const std::string & osName);
		void ThrowCsFetchJobDetailsFailedException();
		void ThrowMobilityAgentNotFoundException();
		void ThrowPushClientNotFoundException();
		void ThrowSoftwareDownloadFromCSFailedException();
		void ThrowSoftwareSignatureVerificationFailedException();
		void ThrowAgentInstallCompletedRebootRequired();
		void ThrowAgentDifferentVersionAlreadyInstalled();
		void ThrowAgentRegisteredWithDifferentCS();
		void ThrowAgentInstallerFatalError();
		void ThrowAgentInstallerPrepareStageFailed();
		void ThrowAgentRebootPending();
		void ThrowInvalidCSDetails();
		void ThrowLinuxNonRootCreds();
		void ThrowInstallerUnsupportedOS();
		void ThrowInstallerServiceStopFailed();
		void ThrowInstallerServiceStartFailed();
		void ThrowInstallerHostEntryNotFound();
		void ThrowInstallerInsufficientDiskSpace();
		void ThrowInstallerCreateTmpFolderFailed();
		void ThrowInstallerFileNotFound();
		void ThrowInstallerPrevDirDeleteFailed();
		void ThrowInstallerDotNet35Missing();
		void ThrowInstallerRebootPendingFromPrevInstall();
		void ThrowInstallerRegistrationFailed();
		void ThrowInstallerVSSFailure();
		void ThrowInstallerInternalError();
		void ThrowInstallerInvalidParams();
		void ThrowInstallerInvalidCommandLineArguments();
		void ThrowInstallerAlreadyRunning();
		void ThrowInstallerOSMismatch();
		void ThrowInstallerMissingParams();
		void ThrowInstallerPassphraseFileMissing();
		void ThrowInstallerSpecificServiceStopFailed();
		void ThrowInstallerInsufficientPrivelege();
		void ThrowInstallerPrevInstallNotFound();
		void ThrowInstallerUnsupportedKernel();
		void ThrowInstallerLinuxVersionNotSupported();
		void ThrowInstallerBootupDriverNotAvailable();
		void ThrowInstallerCSInstalled();
		void ThrowInstallerRebootRecommended();
		void ThrowInstallerLinuxRebootRecommended();
		void ThrowInstallerCurrentKernelNotSupported();
		void ThrowInstallerLISComponentsMissing();
		void ThrowInstallerInsufficientSpaceOnRoot();
		void ThrowUninstallFailed();
		void ThrowPushinstallVmwareInternalError();
		void ThrowInstallerExitStatusFileNotAvailable();
		void ThrowInstallerPrereqJsonFileNotAvailable();
		void ThrowInstallerVSSInstallationFailureJsonFileNotAvailable();
		void ThrowInstallerSpaceNotAvailable();
		void ThrowInstallerVMwareToolsNotRunning();
		void ThrowInstallerVSSInstallationFailed();
		void ThrowInstallerDriverManifestInstallationFailed();
		void ThrowInstallerMasterTargetExists();
		void ThrowInstallerPlatformMismatch();
		void ThrowInstallerVSSInstallationFailedWithErrors();
		void ThrowInstallerVSSInstallationFailedWithWarnings();
		void ThrowInstallerPreReqsFailed();
		void ThrowRegistryEntryUpdateFailed();
		void ThrowInstallerGenCertFailed();
		void ThrowInstallerDisableLmv2ServiceFailed();
		void ThrowInstallerUpgradePlatformChangeNotSupported();
		void ThrowInstallerHostNameNotFound();
		void ThrowInstallerAbsInstallationDirectoryPathNotPresent();
		void ThrowInstallerPartitionSpaceNotAvailable();
		void ThrowInstallerOperatingSystemNotSupported();
		void ThrowInstallerThirdPartyNoticeNotAvailable();
		void ThrowInstallerLocaleNotAvailable();
		void ThrowInstallerRpmCommandNotAvailable();
		void ThrowInstallerConfigDirectoryCreationFailed();
		void ThrowInstallerConfFileCopyFailed();
		void ThrowInstallerRpmInstallationFailed();
		void ThrowInstallerTempFolderCreationFailed();
		void ThrowInstallerDriverInstallationFailed();
		void ThrowInstallerCacheFolderCreationFailed();
		void ThrowInstallerSnapshotDriverInUse();
		void ThrowInstallerSnapshotDriverUnloadFailed();
		void ThrowInstallerInvalidUxInstallationOptions();
		void ThrowInstallerMutexAcquisitionFailed();
		void ThrowInstallerLinuxInsufficientDiskSpace();
		void ThrowInstallerVSSInstallOutOfMemory();
		void ThrowInstallerVSSInstallDatabaseLocked();
		void ThrowInstallerVSSInstallServiceAlreadyExists();
		void ThrowInstallerVSSInstallServiceMarkedForDeletion();
		void ThrowInstallerVSSInstallCSScriptAccessDenied();
		void ThrowInstallerVSSInstallPathNotFound();
		void ThrowInstallerMSIInstallationFailed();
		void ThrowInstallerSetServiceStartupTypeFailed();
		void ThrowInstallerInitRdBackupFailed();
		void ThrowInstallerInitRdRestoreFailed();
		void ThrowInstallerInitRdImageUpdateFailed();
		void ThrowInstallerCreateSymlinksFailed();
		void ThrowInstallerInstallAndConfigureServicesFailed();
		void ThrowBiosIdMismatchError();
		void ThrowFqdnMismatchException();
		void ThrowInstallerOutOfMemory();

		bool IsVmwareBasedInstall() { return m_installationMode == InstallationMode::VmWare; }
		bool IsWmiSshBasedInstall() { return m_installationMode == InstallationMode::WMISSH; }

// end region : vmware based installation routines.


		PushJobDefinition m_jobdefinition;
		PushInstallCommunicationBasePtr m_proxy;
		PushConfigPtr m_config;
		PushInstallSWMgrBasePtr m_swmgr;
		InstallationMode m_installationMode;
	};
	
}

#endif
