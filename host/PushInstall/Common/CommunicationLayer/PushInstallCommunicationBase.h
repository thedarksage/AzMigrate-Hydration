//---------------------------------------------------------------
//  <copyright file="PushInstallCommunicationBase.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines base class for all the communication routines.
//  </summary>
//
//  History:     04-Sep-2018    rovemula    Created
//----------------------------------------------------------------

#ifndef __PUSH_INSTALL_COMMUNICATION_BASE_H__
#define __PUSH_INSTALL_COMMUNICATION_BASE_H__

#include <string>
#include "defs.h"
#include "proxy.h"
#include "pushInstallationSettings.h"

namespace PI
{

	typedef std::map<std::string, std::string> PlaceHoldersMap;
	class PushInstallCommunicationBase;
	typedef boost::shared_ptr<PushInstallCommunicationBase> PushInstallCommunicationBasePtr;

	/// \brief Base class for all communication implementers
	class PushInstallCommunicationBase
	{
		
	public:
		PushInstallCommunicationBase() {}
		virtual ~PushInstallCommunicationBase() 
		{
			
		}

		virtual std::string csip() const = 0;
		virtual int csport() const = 0;

		virtual bool RegisterPushInstaller() = 0;
		virtual bool UnregisterPushInstaller() = 0;
		virtual PushInstallationSettings GetSettings() = 0;

		virtual PUSH_INSTALL_JOB GetInstallDetails(
			const std::string & jobid,
			std::string osName,
			bool isFreshInstall = true) = 0;
		virtual std::string GetPushClientDetails(
			const std::string & jobId, 
			const std::string& osName) = 0;
		virtual std::string GetBuildPath(const std::string & jobId, const std::string& osName) = 0;
		virtual bool UpdateAgentInstallationStatus(
			const std::string& jobId,
			PUSH_JOB_STATUS state,
			const std::string& statusString,
			const std::string& hostId = "",
			const std::string& log = "",
			const std::string &errorCode = "",
			const PlaceHoldersMap &placeHolders = PlaceHoldersMap(),
			const std::string& installerExtendedErrorsJson = "",
			const std::string& distroName = "") = 0;
		virtual void DownloadInstallerFile(const std::string& path, const std::string& localFilePath) = 0;
		virtual std::string GetPassphrase() = 0;
		virtual std::string GetPassphraseFilePath() = 0;

	};
}

#endif
