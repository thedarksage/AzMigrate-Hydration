///
/// \file RcmPushConfig.h
///
/// \brief
///


#ifndef INMAGE_PI_RCMCONFIG_H
#define INMAGE_PI_RCMCONFIG_H

#include <locale>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include "supportedplatforms.h"
#include "host.h"
#include "pushconfig.h"
#include "RcmJob.h"
#include "RcmJobInputs.h"
#include "RcmConfigInputs.h"

#include "NotImplementedException.h"
#include "pushInstallationSettings.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace PI {

	class RcmPushConfig;
	typedef boost::shared_ptr<RcmPushConfig> RcmPushConfigPtr;

	class RcmPushConfig : public PushConfig
	{

	public:

		~RcmPushConfig() 
		{

		}

		virtual std::string PushServerName();
		virtual bool secure() const;
		virtual std::string hostid() const;

		virtual std::string installPath() const;
		virtual std::string tmpFolder() const;
		
		virtual std::string localRepositoryUAPath() const;
		virtual std::string localRepositoryPushclientPath() const;
		virtual std::string localRepositoryPatchPath() const;

		virtual std::string remoteRootFolder(enum remoteApiLib::os_idx idx) const;

		virtual bool isVmWarePushInstallationEnabled() const;
		virtual std::string vmWareWrapperCmd() const;
		virtual std::string osScript() const;
		virtual StackType stackType() const { return StackType::RCM; }

		virtual int jobTimeOutInSecs() const;
		virtual int jobRetries() const;
		virtual int jobRetryIntervalInSecs() const;
		virtual int vmWareCmdTimeOutInSecs() const;
		virtual bool SignVerificationEnabled() const;
		virtual bool SignVerificationEnabledOnRemoteMachine() const;

		virtual std::string PushInstallTelemetryLogsPath() const;
		virtual std::string AgentTelemetryLogsPath() const;

		static RcmPushConfigPtr Instance();
		static void Initialize(std::string & rcmTunablesFilePath, std::string & kustoLogsFolder);
		std::string credStoreCredsFilePath() const;
		INSTALL_TYPE typeOfInstall() const;
		std::string defaultAgentInstallationPath(enum remoteApiLib::os_idx idx) const;

	protected:

	private:

		RcmPushConfig();
		
		static std::string m_rcmConfigTunablesPath;
		static std::string m_kustoLogsFolder;

		static RcmServiceTunables m_tunables;
		static RcmPushInstallationProperties m_properties;

		static void PopulateRcmConfigInputs();
		static void PopulateRcmTunables(boost::property_tree::ptree tunablesPt);
		static void PopulateRcmPushInstallationProperties(boost::property_tree::ptree propertiesPt);
	};
}

#endif