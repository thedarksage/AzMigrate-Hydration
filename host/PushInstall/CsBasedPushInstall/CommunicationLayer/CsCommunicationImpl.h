//---------------------------------------------------------------
//  <copyright file="CsCommunicationImpl.h" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines proxy routines and member variables of CsProxy class.
//  </summary>
//
//  History:     04-Sep-2018    rovemula    Created
//----------------------------------------------------------------

#ifndef __CS_PROXY_H__
#define __CS_PROXY_H__

#include <string>
#include "PushInstallCommunicationBase.h"
#include "CsPushConfig.h"
#include "talwrapper.h"
#include "pushInstallationSettings.h"

#include <boost/shared_ptr.hpp>


namespace PI
{

	class CsCommunicationImpl;
	typedef boost::shared_ptr<CsCommunicationImpl> CsCommunicationImplPtr;

	/// \brief proxy class to communicate with Cs
	class CsCommunicationImpl : public PushInstallCommunicationBase
	{

	public:
		
		virtual ~CsCommunicationImpl() {}

		/// \brief gets the pending jobs from Cs
		virtual PushInstallationSettings GetSettings();

		/// \brief gets the extra job details necessary
		virtual PUSH_INSTALL_JOB GetInstallDetails(
			const std::string & jobid,
			std::string osName,
			bool isFreshInstall);

		/// \brief gets the push client details
		virtual std::string GetPushClientDetails(
			const std::string & jobId,
			const std::string& osName);

		/// \brief gets the mobility agent build path
		virtual std::string GetBuildPath(const std::string & jobId, const std::string& osName);

		/// \brief updates the job status to Cs
		virtual bool UpdateAgentInstallationStatus(
			const std::string& jobId,
			PUSH_JOB_STATUS state,
			const std::string& statusString,
			const std::string& hostId = "",
			const std::string& log = "",
			const std::string &errorCode = "",
			const PlaceHoldersMap &placeHolders = PlaceHoldersMap(),
			const std::string& installerExtendedErrorsJson = "",
			const std::string& distroName = "");

		/// \brief gets the passphrase for agent registration with CS.
		virtual std::string GetPassphrase();

		/// \brief gets the passphrase file path
		virtual std::string GetPassphraseFilePath();

		/// \brief registers push installer with Cs
		virtual bool RegisterPushInstaller();

		/// \brief unregisters push installer with Cs
		virtual bool UnregisterPushInstaller();

		virtual std::string csip() const;
		virtual int csport() const;

		/// \brief downloads the mobility agent installer file from repository
		virtual void DownloadInstallerFile(
			const std::string& remotepath,
			const std::string& localpath);

		static CsCommunicationImplPtr Instance();
		static void Initialize();


	private:

		CsCommunicationImpl();

		/// \brief Ip of the Cs machine
		std::string m_csip;
		/// \brief Port of the Cs machine for communication
		SV_UINT m_csport;
		bool m_secure;
		/// \brief Host Id of the push installer
		std::string m_hostid;

		std::string m_dpc;
		TalWrapper m_tal;

		int m_csRetries;
		int m_csRetryInterval;


	};
}

#endif
