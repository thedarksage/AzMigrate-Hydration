///
/// \file CsPushConfig.h
///
/// \brief
///


#ifndef INMAGE_PI_CSCONFIG_H
#define INMAGE_PI_CSCONFIG_H

#include <locale>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include "supportedplatforms.h"
#include "host.h"
#include "pushconfig.h"

#include "NotImplementedException.h"

namespace PI {

	class CsPushConfig;
	typedef boost::shared_ptr<CsPushConfig> CsPushConfigPtr;

	class CsPushConfig : public PushConfig
	{

	public:
		
		~CsPushConfig() {}

		std::string csip() const;
		int csport() const;
		virtual bool secure() const;
		virtual std::string hostid() const;

		virtual std::string installPath() const;
		virtual std::string tmpFolder() const;
		int logLevel() const { return m_loglevel; }

		virtual std::string localRepositoryUAPath() const;
		virtual std::string localRepositoryPushclientPath() const;
		virtual std::string localRepositoryPatchPath() const;

		virtual std::string remoteRootFolder(enum remoteApiLib::os_idx idx) const;

		virtual std::string PushServerName();
		virtual bool isVmWarePushInstallationEnabled() const;
		virtual std::string vmWareWrapperCmd() const;
		virtual std::string osScript() const;
		virtual StackType stackType() const { return StackType::CS; }

		virtual int jobTimeOutInSecs() const;
		int jobFetchIntervalInSecs() const;
		virtual int jobRetries() const;
		virtual int jobRetryIntervalInSecs() const;
		virtual int vmWareCmdTimeOutInSecs() const;
		int CSRetries() const { return m_CSRetries; }
		int CSRetryIntervalInSecs() const { return m_CSRetryIntervalInSecs; }
		virtual bool SignVerificationEnabled() const;
		virtual bool SignVerificationEnabledOnRemoteMachine() const;
		virtual std::string credStoreCredsFilePath() const;

		virtual std::string PushInstallTelemetryLogsPath() const;
		virtual std::string AgentTelemetryLogsPath() const;

		std::string logFolder() const;

		static std::string configPath();
		static CsPushConfigPtr Instance();
		static void Initialize();

	protected:

	private:

		CsPushConfig();

		std::string m_csip;
		int m_csport;
		bool m_secure;
		std::string m_hostid;

		bool m_isVmWarePushInstallationEnabled;
		std::string m_installPath;
		std::string m_logfolder;
		std::string m_tmpfolder;
		int m_loglevel;

		std::string m_pushInstallTelemetryLogsPath;
		std::string m_agentTelemetryLogsPath;

		std::string m_localRepositoryUAPath;
		std::string m_localRepositoryPushclientPath;
		std::string m_localRepositoryPatchPath;

		std::string m_osnameScript;

		bool m_signVerification;
		bool m_signVerificationOnRemoteMachine;
		std::string m_remoteRootFolderWindows;
		std::string m_remoteRootFolderUnix;
		int m_jobTimeoutInSecs;

		int m_jobFetchIntervalInSecs;
		int m_jobRetries;
		int m_jobRetryIntervalInSecs;
		int m_CSRetries;
		int m_CSRetryIntervalInSecs;

		std::string m_vmwareApiWrapperCmd;
		int m_vmwareApiWrapperCmdTimeOutInSecs;
		bool m_isPushServerNameSet;
		std::string m_pushserverName;
	};
}

#endif