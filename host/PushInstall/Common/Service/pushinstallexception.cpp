//---------------------------------------------------------------
//  <copyright file="pushinstallerrormessage.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines routines for throwing apprpriate push install exception.
//  </summary>
//
//  History:     15-Nov-2017    Jaysha    Created
//----------------------------------------------------------------

#include "..\Config\pushconfig.h"
#include "pushjob.h"
#include "pushInstallationSettings.h"
#include "pushjobdefinition.h"
#include "pushinstallexception.h"
#include "pushinstallfallbackexception.h"
#include "errormessage.h"
#include "logger.h"

#include <boost/filesystem.hpp>

namespace PI
{
	void PushJob::ThrowCsFetchJobDetailsFailedException()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0851";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0851(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowSoftwareDownloadFromCSFailedException()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0852";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0852(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowSoftwareSignatureVerificationFailedException()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0853";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0853(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowAgentInstallCompletedRebootRequired()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0884";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0884(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowOsVersionIdentificationFailedException()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0860";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0860(placeHolders);

		if (this->IsVmwareBasedInstall())
		{
			throw  PushInstallFallbackException(
				errorCode,
				GetErrorNameForErrorcode(errorCode),
				errMessage,
				placeHolders);
		}
		else if (this->IsWmiSshBasedInstall())
		{
			throw  PushInstallException(
				errorCode,
				GetErrorNameForErrorcode(errorCode),
				errMessage,
				placeHolders);
		}
	}

	void PushJob::ThrowAgentDifferentVersionAlreadyInstalled()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0864";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0864(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowAgentRegisteredWithDifferentCS()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0867";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0867(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowAgentInstallerFatalError()
	{
		std::string errorCode = "EP0870";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0870(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowAgentInstallerPrepareStageFailed()
	{
		std::string errorCode = "EP0871";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0871(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowAgentRebootPending()
	{
		std::string errorCode = "EP0872";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0872(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInvalidCSDetails()
	{
		std::string errorCode = "EP0873";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0873(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowLinuxNonRootCreds()
	{
		std::string errorCode = "EP0963";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerUnsupportedOS()
	{
		std::string errorCode = "EP0874";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0874(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerServiceStopFailed()
	{
		std::string errorCode = "EP0875";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0875(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerServiceStartFailed()
	{
		std::string errorCode = "EP0876";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0876(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerHostEntryNotFound()
	{
		std::string errorCode = "EP0877";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0877(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInsufficientDiskSpace()
	{
		std::string errorCode = "EP0878";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0878(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerCreateTmpFolderFailed()
	{
		std::string errorCode = "EP0879";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0879(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerFileNotFound()
	{
		std::string errorCode = "EP0880";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0880(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowOsNotSupportedException(const std::string & osName)
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0881";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0881(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPrevDirDeleteFailed()
	{
		std::string errorCode = "EP0882";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0882(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerRebootPendingFromPrevInstall()
	{
		std::string errorCode = "EP0883";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0883(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}


	void PushJob::ThrowInstallerDotNet35Missing()
	{
		std::string errorCode = "EP0885";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0885(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerRegistrationFailed()
	{
		std::string errorCode = "EP0886";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0886(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSFailure()
	{
		std::string errorCode = "EP0887";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0887(placeHolders);

		throw PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInternalError()
	{
		std::string errorCode = "EP0888";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0888(placeHolders);

		throw PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInvalidParams()
	{
		std::string errorCode = "EP0889";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0889(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerAlreadyRunning()
	{
		std::string errorCode = "EP0890";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0890(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerOSMismatch()
	{
		std::string errorCode = "EP0891";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0891(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerMissingParams()
	{
		std::string errorCode = "EP0892";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0892(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPassphraseFileMissing()
	{
		std::string errorCode = "EP0894";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0894(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerSpecificServiceStopFailed()
	{
		std::string errorCode = "EP0895";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0895(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInsufficientPrivelege()
	{
		std::string errorCode = "EP0896";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0896(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPrevInstallNotFound()
	{
		std::string errorCode = "EP0897";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0897(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerUnsupportedKernel()
	{
		std::string errorCode = "EP0898";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0898(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerLinuxVersionNotSupported()
	{
		std::string errorCode = "EP0899";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0899(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerBootupDriverNotAvailable()
	{
		std::string errorCode = "EP0900";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0900(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerCSInstalled()
	{
		std::string errorCode = "EP0901";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0901(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerRebootRecommended()
	{
		std::string errorCode = "EP0902";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0902(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerLinuxRebootRecommended()
	{
		std::string errorCode = "EP0903";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0903(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerCurrentKernelNotSupported()
	{
		std::string errorCode = "EP0904";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0904(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerLISComponentsMissing()
	{
		std::string errorCode = "EP0905";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0905(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInsufficientSpaceOnRoot()
	{
		std::string errorCode = "EP0906";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0906(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerSpaceNotAvailable()
	{
		std::string errorCode = "EP0907";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0907(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVMwareToolsNotRunning()
	{
		std::string errorCode = "EP0908";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0908(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallationFailed()
	{
		std::string errorCode = "EP0909";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0909(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerDriverManifestInstallationFailed()
	{
		std::string errorCode = "EP0910";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0910(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerMasterTargetExists()
	{
		std::string errorCode = "EP0911";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0911(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPlatformMismatch()
	{
		std::string errorCode = "EP0912";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0912(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInvalidCommandLineArguments()
	{
		std::string errorCode = "EP0913";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0913(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowRegistryEntryUpdateFailed()
	{
		std::string errorCode = "EP0914";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0914(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerGenCertFailed()
	{
		std::string errorCode = "EP0936";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerDisableLmv2ServiceFailed()
	{
		std::string errorCode = "EP0916";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerUpgradePlatformChangeNotSupported()
	{
		std::string errorCode = "EP0917";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerHostNameNotFound()
	{
		std::string errorCode = "EP0918";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerAbsInstallationDirectoryPathNotPresent()
	{
		std::string errorCode = "EP0919";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPartitionSpaceNotAvailable()
	{
		std::string errorCode = "EP0920";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerOperatingSystemNotSupported()
	{
		std::string errorCode = "EP0921";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerThirdPartyNoticeNotAvailable()
	{
		std::string errorCode = "EP0922";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerLocaleNotAvailable()
	{
		std::string errorCode = "EP0923";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerRpmCommandNotAvailable()
	{
		std::string errorCode = "EP0924";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerConfigDirectoryCreationFailed()
	{
		std::string errorCode = "EP0925";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerConfFileCopyFailed()
	{
		std::string errorCode = "EP0926";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerRpmInstallationFailed()
	{
		std::string errorCode = "EP0927";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerTempFolderCreationFailed()
	{
		std::string errorCode = "EP0928";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerDriverInstallationFailed()
	{
		std::string errorCode = "EP0929";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerCacheFolderCreationFailed()
	{
		std::string errorCode = "EP0930";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerSnapshotDriverInUse()
	{
		std::string errorCode = "EP0931";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}



	void PushJob::ThrowInstallerSnapshotDriverUnloadFailed()
	{
		std::string errorCode = "EP0932";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInvalidUxInstallationOptions()
	{
		std::string errorCode = "EP0933";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerMutexAcquisitionFailed()
	{
		std::string errorCode = "EP0934";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerLinuxInsufficientDiskSpace()
	{
		std::string errorCode = "EP0935";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallationFailedWithErrors()
	{
		std::string errorCode = "EP0964";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallationFailedWithWarnings()
	{
		std::string errorCode = "EP0965";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPreReqsFailed()
	{
		std::string errorCode = "EP0915";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0915(placeHolders);

		throw PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowUninstallFailed()
	{
		std::string errorCode = "EP0868";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP0868(placeHolders);

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallDatabaseLocked()
	{
		std::string errorCode = "EP0938";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallServiceAlreadyExists()
	{
		std::string errorCode = "EP0939";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallServiceMarkedForDeletion()
	{
		std::string errorCode = "EP0940";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallCSScriptAccessDenied()
	{
		std::string errorCode = "EP0941";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallPathNotFound()
	{
		std::string errorCode = "EP0942";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallOutOfMemory()
	{
		std::string errorCode = "EP0943";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerMSIInstallationFailed()
	{
		std::string errorCode = "EP0944";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerSetServiceStartupTypeFailed()
	{
		std::string errorCode = "EP0945";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInitRdBackupFailed()
	{
		std::string errorCode = "EP0946";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInitRdRestoreFailed()
	{
		std::string errorCode = "EP0947";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInitRdImageUpdateFailed()
	{
		std::string errorCode = "EP0948";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerCreateSymlinksFailed()
	{
		std::string errorCode = "EP0953";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerInstallAndConfigureServicesFailed()
	{
		std::string errorCode = "EP0954";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowBiosIdMismatchError()
	{
		std::string errorCode = "EP0966";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		if (m_jobdefinition.vmType == VM_TYPE::PHYSICAL)
		{
			errorCode = "EP0970";
		}

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerOutOfMemory()
	{
		std::string errorCode = "EP0967";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowMobilityAgentNotFoundException()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0968";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowPushClientNotFoundException()
	{
		std::map<std::string, std::string> placeHolders;
		std::string errorCode = "EP0969";
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowFqdnMismatchException()
	{
		std::string errorCode = "EP2005";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = "";

		throw  PushInstallException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}
	
	void PushJob::ThrowPushinstallVmwareInternalError()
	{
		std::string errorCode = "EP2001";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP2001(placeHolders);

		throw PushInstallFallbackException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerExitStatusFileNotAvailable()
	{
		std::string errorCode = "EP2002";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP2002(placeHolders);

		throw PushInstallFallbackException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerPrereqJsonFileNotAvailable()
	{
		std::string errorCode = "EP2003";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP2003(placeHolders);

		throw PushInstallFallbackException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::ThrowInstallerVSSInstallationFailureJsonFileNotAvailable()
	{
		std::string errorCode = "EP2004";
		std::map<std::string, std::string> placeHolders;
		this->FillPlaceHolders(errorCode, placeHolders);
		std::string errMessage = getErrorEP2004(placeHolders);

		throw PushInstallFallbackException(
			errorCode,
			GetErrorNameForErrorcode(errorCode),
			errMessage,
			placeHolders);
	}

	void PushJob::FillPlaceHolders(const std::string & errorCode, std::map<std::string, std::string> & placeHolders)
	{
		placeHolders.clear();
		placeHolders[psIp] = m_config->PushServerIp();
		placeHolders[psName] = m_config->PushServerName();
		placeHolders[sourceIp] = m_jobdefinition.remoteip;
		placeHolders[sourceName] = m_jobdefinition.vmName;
		placeHolders[csIp] = m_proxy->csip();
		placeHolders[csPort] = boost::lexical_cast<std::string>(m_proxy->csport());
		placeHolders[pushLogPath] = m_jobdefinition.remoteLogPath();
		placeHolders[errCode] = errorCode;

		try
		{
			if ((enum remoteApiLib::os_idx)m_jobdefinition.os_id == remoteApiLib::os_idx::windows_idx) 
			{
				placeHolders[operatingSystemName] = "Windows";
			}
			else
			{
				placeHolders[operatingSystemName] = m_jobdefinition.getOptionalParam(PI_OSNAME_KEY);
			}
			
			if (m_jobdefinition.vmType == VM_TYPE::VMWARE)
			{
				placeHolders[vCenterIp] = m_jobdefinition.vCenterIP;
			}
		}
		catch (...)
		{
			DebugPrintf(SV_LOG_DEBUG, "Operating system name has not been populated. So, not adding it to placeholders\n");
		}

		try 
		{
			placeHolders[remoteUAPath] = m_jobdefinition.remoteUaPath();
		}
		catch (...)
		{
			DebugPrintf(SV_LOG_DEBUG, "RemoteUAPath has not been populated. So, not adding it to placeholders\n");
		}
		
		placeHolders[cspsConfigToolPath] = "<PS install path>\\home\\svsystems\\bin";
		placeHolders[installPath] = m_jobdefinition.installation_path;
		placeHolders[passphrasePath] = m_jobdefinition.remoteConnectionPassPhrasePath();
	}

}
