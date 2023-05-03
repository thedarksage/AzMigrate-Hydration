///
/// \file pushconfig.h
///
/// \brief
///


#ifndef INMAGE_PI_CONFIG_H
#define INMAGE_PI_CONFIG_H

#include <locale>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>

#include "supportedplatforms.h"
#include "host.h"

enum StackType { CS, RCM };

namespace PI {

	class PushConfig;
	typedef boost::shared_ptr<PushConfig> PushConfigPtr;

	class PushConfig {

	public:
		PushConfig() {}
		virtual ~PushConfig() 
		{

		}

		std::string PushServerIp();
		virtual std::string PushServerName() = 0;

		virtual bool secure() const = 0;
		virtual std::string hostid() const = 0;

		virtual std::string installPath() const = 0;
		virtual std::string tmpFolder() const = 0;

		virtual std::string localRepositoryUAPath() const = 0;
		virtual std::string localRepositoryPushclientPath() const = 0;
		virtual std::string localRepositoryPatchPath() const = 0;

		virtual std::string remoteRootFolder(enum remoteApiLib::os_idx idx) const = 0;
		virtual bool isVmWarePushInstallationEnabled() const = 0;
		virtual std::string vmWareWrapperCmd() const = 0;
		virtual std::string osScript() const = 0;
		virtual StackType stackType() const = 0;

		virtual int jobTimeOutInSecs() const = 0;
		virtual int jobRetries() const = 0;
		virtual int jobRetryIntervalInSecs() const = 0;
		virtual int vmWareCmdTimeOutInSecs() const = 0;
		virtual bool SignVerificationEnabled() const = 0;
		virtual bool SignVerificationEnabledOnRemoteMachine() const = 0;

		virtual std::string credStoreCredsFilePath() const = 0;

		virtual std::string PushInstallTelemetryLogsPath() const = 0;
		virtual std::string AgentTelemetryLogsPath() const = 0;

	protected:

	private:

		std::string m_pushserverIp;
		bool m_isPushServerIpSet;
	};
}

#endif