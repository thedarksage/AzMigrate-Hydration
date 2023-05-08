///
/// \file CsPushConfig.cpp
///
/// \brief
///

#include <string>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/thread/mutex.hpp>

#include "localconfigurator.h"
#include "CsPushConfig.h"
#include "supportedplatforms.h"
#include "host.h"

#include "defaultdirs.h"


namespace PI {

	CsPushConfigPtr g_cspushconfig;
	bool g_cspushconfigcreated = false;
	boost::mutex g_cspushconfigMutex;
	static Lockable m_CsSyncLock;
	
	CsPushConfigPtr CsPushConfig::Instance()
	{
		if (!g_cspushconfigcreated) {
			boost::mutex::scoped_lock guard(g_cspushconfigMutex);
			if (!g_cspushconfigcreated) {
				g_cspushconfig.reset(new CsPushConfig());
				g_cspushconfigcreated = true;
			}
		}

		return g_cspushconfig;
	}

	void CsPushConfig::Initialize()
	{
		if (!g_cspushconfigcreated) {
			boost::mutex::scoped_lock guard(g_cspushconfigMutex);
			if (!g_cspushconfigcreated) {
				g_cspushconfig.reset(new CsPushConfig());
				g_cspushconfigcreated = true;
			}
		}
	}

	CsPushConfig::CsPushConfig()
	{
		LocalConfigurator lc(CsPushConfig::configPath());
		HTTP_CONNECTION_SETTINGS csSettings = lc.pushHttpSettings();
		
		m_csip = csSettings.ipAddress;
		m_csport = csSettings.port;
		m_secure = lc.pushHttps();
		m_hostid = lc.pushHostid();
		m_installPath = lc.pushInstallPath();
		m_isVmWarePushInstallationEnabled = lc.isVmWarePushInstallationEnabled();
		m_pushInstallTelemetryLogsPath = lc.pushInstallTelemetryLogsPath();
		m_agentTelemetryLogsPath = lc.agentTelemetryLogsPath();
		m_logfolder = lc.pushLogFolder();
		m_tmpfolder = lc.pushTmpFolder();
		m_loglevel = lc.pushLogLevel();
		m_localRepositoryUAPath = lc.pushLocalRepositoryPathUA();
		m_localRepositoryPushclientPath = lc.pushLocalRepositoryPathPushclient();
		m_localRepositoryPatchPath = lc.pushLocalRepositoryPathPatch();
		m_remoteRootFolderWindows = lc.pushRemoteRootFolderWindows();
		m_remoteRootFolderUnix = lc.pushRemoteRootFolderUnix();
		m_osnameScript = lc.pushOsScriptPath();
		m_vmwareApiWrapperCmd = lc.pushVmwareApiWrapperCmd();
		m_jobTimeoutInSecs = lc.pushJobTimeoutSecs();
		m_jobFetchIntervalInSecs = lc.pushJobFetchIntervalInSecs();
		m_vmwareApiWrapperCmdTimeOutInSecs = lc.pushVmwareApiWrapperCmdTimeOutSecs();
		m_jobRetries = lc.pushJobRetries();
		m_jobRetryIntervalInSecs = lc.pushJobRetryIntervalInsecs();
		m_CSRetries = lc.pushJobCSRetries();
		m_CSRetryIntervalInSecs = lc.pushJobCSRetryIntervalInsecs();

		m_signVerification = lc.pushSignVerificationEnabled();
		m_signVerificationOnRemoteMachine = lc.pushSignVerificationEnabledOnRemoteMachine();

		if (m_logfolder.empty()) {
			m_logfolder = m_installPath;
		}

		if (m_tmpfolder.empty()) {
			m_tmpfolder = m_installPath + remoteApiLib::pathSeperator() + "temporaryfiles";
		}

		if (m_pushInstallTelemetryLogsPath.empty()) {
			m_pushInstallTelemetryLogsPath = "C:\\ProgramData\\ASR\\home\\svsystems\\PushInstallLogs";
		}

		if (m_agentTelemetryLogsPath.empty()) {
			m_agentTelemetryLogsPath = "C:\\ProgramData\\ASR\\home\\svsystems\\AgentSetupLogs\\PushInstall";
		}

		if (m_localRepositoryUAPath.empty()) {
			//m_localRepositoryUAPath = m_installPath + remoteApiLib::pathSeperator() + "repositoryUA";
			m_localRepositoryUAPath = m_installPath + remoteApiLib::pathSeperator() + "repository";
		}

		if (m_localRepositoryPushclientPath.empty()) {
			//m_localRepositoryPushclientPath = m_installPath + remoteApiLib::pathSeperator() + "repositoryPushClient";
			m_localRepositoryPushclientPath = m_installPath + remoteApiLib::pathSeperator() + "repository";
		}

		if (m_localRepositoryPatchPath.empty()) {
			//m_localRepositoryPatchPath = m_installPath + remoteApiLib::pathSeperator() + "repositorypatch";
			m_localRepositoryPatchPath = m_installPath + remoteApiLib::pathSeperator() + "repository";
		}

		if (m_remoteRootFolderWindows.empty()){
			m_remoteRootFolderWindows = "C:\\ProgramData\\Microsoft Azure Site Recovery\\Mobility Service";
		}

		if (m_remoteRootFolderUnix.empty()) {
			m_remoteRootFolderUnix = "/tmp/remoteinstall";
		}

		if (m_osnameScript.empty()) {
			m_osnameScript = m_installPath + remoteApiLib::pathSeperator() + "OS_details.sh";
		}

		if (m_vmwareApiWrapperCmd.empty()) {
			m_vmwareApiWrapperCmd = m_installPath + remoteApiLib::pathSeperator() + "vmwarepushinstall.exe";
		}

		boost::replace_all(m_installPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(m_logfolder, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(m_tmpfolder, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(m_localRepositoryUAPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(m_localRepositoryPushclientPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(m_localRepositoryPatchPath, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(m_remoteRootFolderWindows, remoteApiLib::pathSeperator(remoteApiLib::unix_idx), remoteApiLib::pathSeperator(remoteApiLib::windows_idx));
		boost::replace_all(m_remoteRootFolderUnix, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::replace_all(m_osnameScript, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
	}

	std::string CsPushConfig::PushServerName()
	{
		if (!m_isPushServerNameSet)
		{
			AutoLock CreateGuard(m_CsSyncLock);
			if (!m_isPushServerNameSet)
			{
				m_pushserverName = Host::GetInstance().GetHostName();
				m_isPushServerNameSet = true;
			}
		}

		return m_pushserverName;
	}

	std::string CsPushConfig::csip() const { return m_csip; }
	int CsPushConfig::csport() const { return m_csport; }
	bool CsPushConfig::secure() const { return m_secure; }
	std::string CsPushConfig::hostid() const { return m_hostid; }

	std::string CsPushConfig::installPath() const { return m_installPath; }
	std::string CsPushConfig::logFolder() const { return m_logfolder; }
	std::string CsPushConfig::tmpFolder() const { return m_tmpfolder; }

	std::string CsPushConfig::localRepositoryUAPath() const { return m_localRepositoryUAPath; }
	std::string CsPushConfig::localRepositoryPushclientPath() const { return m_localRepositoryPushclientPath; }
	std::string CsPushConfig::localRepositoryPatchPath() const { return m_localRepositoryPatchPath; }

	std::string CsPushConfig::remoteRootFolder(enum remoteApiLib::os_idx idx) const
	{
		if (idx == remoteApiLib::windows_idx) {
			return m_remoteRootFolderWindows;
		}
		else if (idx == remoteApiLib::unix_idx){
			return m_remoteRootFolderUnix;
		}
		else {
			throw NotImplementedException(__FUNCTION__, "OS index " + idx);
		}
	}

	std::string CsPushConfig::credStoreCredsFilePath() const { return ""; }

	std::string CsPushConfig::PushInstallTelemetryLogsPath() const
	{
		return m_pushInstallTelemetryLogsPath;
	}
	std::string CsPushConfig::AgentTelemetryLogsPath() const
	{
		return m_agentTelemetryLogsPath;
	}

	bool CsPushConfig::isVmWarePushInstallationEnabled() const { return m_isVmWarePushInstallationEnabled; }
	std::string CsPushConfig::vmWareWrapperCmd() const { return m_vmwareApiWrapperCmd; }
	std::string CsPushConfig::osScript() const { return m_osnameScript; }

	int CsPushConfig::jobTimeOutInSecs() const { return m_jobTimeoutInSecs; }
	int CsPushConfig::jobFetchIntervalInSecs() const { return m_jobFetchIntervalInSecs; }
	int CsPushConfig::jobRetries() const { return m_jobRetries; }
	int CsPushConfig::jobRetryIntervalInSecs() const { return m_jobRetryIntervalInSecs; }
	int CsPushConfig::vmWareCmdTimeOutInSecs() const { return m_vmwareApiWrapperCmdTimeOutInSecs; }
	bool CsPushConfig::SignVerificationEnabled() const { return m_signVerification; }
	bool CsPushConfig::SignVerificationEnabledOnRemoteMachine() const { return m_signVerificationOnRemoteMachine; }
}