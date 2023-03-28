/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	RcmCommunicationImpl.h

Description	:   RcmCommunicationImpl class provides APIs to communicate with the wrapper
				service layer which invokes push install job.

+------------------------------------------------------------------------------------+
*/

#ifndef __RCM_COMMUNICATION_IMPL_H__
#define __RCM_COMMUNICATION_IMPL_H__

#include <string>
#include "pushInstallationSettings.h"
#include "PushInstallCommunicationBase.h"
#include "pushjobdefinition.h"
#include "CredentialMgr.h"
#include "RcmJob.h"
#include "RcmJobInputs.h"
#include "json_reader.h"
#include "json_writer.h"
#include "json_adapter.h"

#include <boost/property_tree/json_parser.hpp>

#include "MobilityServiceInstallerNotAvailableException.h"
#include "NotImplementedException.h"

namespace PI
{

	class RcmCommunicationImpl;
	typedef boost::shared_ptr<RcmCommunicationImpl> RcmCommunicationImplPtr;

	/// \brief proxy class to communicate with Rcm
	class RcmCommunicationImpl : public PushInstallCommunicationBase
	{

	public:
		
		virtual ~RcmCommunicationImpl() 
		{
		}

		static RcmCommunicationImplPtr Instance();

		virtual std::string csip() const;
		virtual int csport() const;
		
		virtual bool RegisterPushInstaller();
		virtual bool UnregisterPushInstaller();
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
		virtual std::string GetBuildPath(
			const std::string & jobId,
			const std::string& osName);

		/// \brief updates the job status to Rcm
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

		/// \brief downloads the latest agent software from DLC
		virtual void DownloadInstallerFile(
			const std::string& remotepath,
			const std::string& localpath);

		/// \brief gets the passphrase from config store
		virtual std::string GetPassphrase();

		/// \brief gegts the passphrase file path
		virtual std::string GetPassphraseFilePath();

		PushJobDefinition GetPushJobDefinition();

		/// \brief initializes rcmproxy
		static void Initialize(std::string & rcmJobInputFilePath, std::string & rcmJobOutputFilePath);


	private:

		RcmCommunicationImpl();

		std::string m_AgentInstallationPath;
		std::string m_PassphraseFilePath;
		bool m_isRcmJobStatusUpdated;

		/// \brief stores the RcmJob entity to be written to the output while updating the caller.
		static RcmJob m_rcmJob;

		std::string m_rcmProxyIp;
		int m_rcmProxyPort;
		PushJobDefinition m_jobDefinition;

		std::string m_sourceCredAccountId;

		/// \brief file path from which job details would be read.
		static std::string m_jobInputFilePath;

		/// \brief file path to which job output details would be written.
		static std::string m_jobOutputFilePath;

		/// \brief reads job inputs from input file.
		static void ReadRcmJobFromInputs();

		boost::property_tree::ptree GetRcmJobPtree();
	};
}



#endif
