///
/// \file RcmPushConfig.cpp
///
/// \brief
///

// adding NOMINMAX to avoid expanding min() and max() macros
// defined in windows.h leading to build error
#define NOMINMAX
#undef max
#undef min

#include <string>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

#include "RcmJobInputs.h"
#include "localconfigurator.h"
#include "RcmPushConfig.h"
#include "supportedplatforms.h"
#include "host.h"
#include "NotInitializedException.h"
#include "CredStoreCredsFileNotFoundException.h"
#include "ptreeparser.h"

#include "defaultdirs.h"


namespace PI {

	RcmPushConfigPtr g_rcmpushconfig;
	bool g_rcmpushconfigcreated = false;
	boost::mutex g_rcmpushconfigMutex;

	std::string RcmPushConfig::m_rcmConfigTunablesPath = "";
	std::string RcmPushConfig::m_kustoLogsFolder = "";
	RcmPushInstallationProperties RcmPushConfig::m_properties;
	RcmServiceTunables RcmPushConfig::m_tunables;
	
	RcmPushConfigPtr RcmPushConfig::Instance()
	{
		if (!g_rcmpushconfigcreated) {
			boost::mutex::scoped_lock guard(g_rcmpushconfigMutex);
			if (!g_rcmpushconfigcreated) {

				if (RcmPushConfig::m_rcmConfigTunablesPath == "")
				{
					throw NotInitializedException("RcmPushConfig");
				}

				g_rcmpushconfig.reset(new RcmPushConfig());
				g_rcmpushconfigcreated = true;
			}
		}

		return g_rcmpushconfig;
	}

	RcmPushConfig::RcmPushConfig()
	{

	}

	void RcmPushConfig::PopulateRcmPushInstallationProperties(boost::property_tree::ptree configPt)
	{
		m_properties.PushInstallAgentMachineId = 
			GetKeyValueFromPtree<std::string>(configPt, "PushInstallJobProperties.PushInstallAgentMachineId", "");
		m_properties.PushInstallAgentFriendlyName =
			GetKeyValueFromPtree<std::string>(configPt, "PushInstallJobProperties.PushInstallAgentFriendlyName", "");

		m_properties.PushInstallAgentInstallationPath =
			GetKeyValueFromPtree<std::string>(
				configPt,
				"PushInstallJobProperties.PushInstallAgentInstallationPath");

		m_properties.RemoteStagingFolderForWindows =
			GetKeyValueFromPtree<std::string>(configPt, "PushInstallJobProperties.RemoteStagingFolderForWindows");
		m_properties.RemoteStagingFolderForLinux =
			GetKeyValueFromPtree<std::string>(configPt, "PushInstallJobProperties.RemoteStagingFolderForLinux");

		m_properties.VmwarePushInstallationWrapperCommand = 
			GetKeyValueFromPtree<std::string>(
				configPt,
				"PushInstallJobProperties.VmwarePushInstallationWrapperCommand", "");

		m_properties.PushinstallClientsLocalRepository =
			GetKeyValueFromPtree<std::string>(
				configPt,
				"PushInstallJobProperties.PushinstallClientsLocalRepository");

		m_properties.MobilityAgentsLocalRepository =
			GetKeyValueFromPtree<std::string>(
				configPt,
				"PushInstallJobProperties.MobilityAgentsLocalRepository");

		m_properties.AgentInstallationPathForWindows =
			GetKeyValueFromPtree<std::string>(
				configPt,
				"PushInstallJobProperties.AgentInstallationPathForWindows");

		m_properties.AgentInstallationPathForLinux =
			GetKeyValueFromPtree<std::string>(
				configPt,
				"PushInstallJobProperties.AgentInstallationPathForLinux");
	}

	void RcmPushConfig::PopulateRcmTunables(boost::property_tree::ptree configPt)
	{
		m_tunables.IsVmwarePushInstallationEnabled = 
			GetKeyValueFromPtree<bool>(configPt, "PushInstallJobTunables.IsVmwarePushInstallationEnabled");

		m_tunables.IsSignVerificationEnabled =
			GetKeyValueFromPtree<bool>(configPt, "PushInstallJobTunables.IsSignVerificationEnabled");

		m_tunables.IsSignVerificationEnabledOnSourceMachine =
			GetKeyValueFromPtree<bool>(configPt, "PushInstallJobTunables.IsSignVerificationEnabledOnSourceMachine");
	}

	void RcmPushConfig::PopulateRcmConfigInputs()
	{
		boost::property_tree::ptree pt;
		boost::property_tree::read_json(m_rcmConfigTunablesPath, pt);

		PopulateRcmTunables(pt);
		
		PopulateRcmPushInstallationProperties(pt);
	}

	void RcmPushConfig::Initialize(
		std::string & rcmTunablesFilePath,
		std::string & kustoLogsFolder)
	{
		if (!g_rcmpushconfigcreated) {
			boost::mutex::scoped_lock guard(g_rcmpushconfigMutex);
			if (!g_rcmpushconfigcreated) {
				RcmPushConfig::m_rcmConfigTunablesPath = rcmTunablesFilePath;
				RcmPushConfig::m_kustoLogsFolder = kustoLogsFolder;
				g_rcmpushconfig.reset(new RcmPushConfig());
				g_rcmpushconfigcreated = true;

				PopulateRcmConfigInputs();
			}
		}
	}

	bool RcmPushConfig::secure() const { return true; }

	std::string RcmPushConfig::hostid() const 
	{ 
		return m_properties.PushInstallAgentMachineId;
	}

	std::string RcmPushConfig::installPath() const 
	{
		return m_properties.PushInstallAgentInstallationPath;
	}

	std::string RcmPushConfig::tmpFolder() const 
	{
		return m_kustoLogsFolder;
	}

	std::string RcmPushConfig::localRepositoryUAPath() const 
	{
		if (m_properties.MobilityAgentsLocalRepository == "")
		{
			return installPath() +
				remoteApiLib::pathSeperator() +
				"Software" +
				remoteApiLib::pathSeperator() +
				"Agents";
		}
		else
		{
			return m_properties.MobilityAgentsLocalRepository;
		}
	}

	std::string RcmPushConfig::localRepositoryPushclientPath() const 
	{
		if (m_properties.PushinstallClientsLocalRepository == "")
		{
			return installPath() +
				remoteApiLib::pathSeperator() +
				"Software" +
				remoteApiLib::pathSeperator() +
				"PushClients";
		}
		else
		{
			return m_properties.PushinstallClientsLocalRepository;
		}
	}

	std::string RcmPushConfig::localRepositoryPatchPath() const
	{
		return installPath() + remoteApiLib::pathSeperator() + "repository";
	}

	std::string RcmPushConfig::remoteRootFolder(enum remoteApiLib::os_idx idx) const
	{
		std::string defaultRootFolder = "";
		if (idx == remoteApiLib::windows_idx) {
			// default value is "C:\\ProgramData\\Microsoft Azure Site Recovery\\Mobility Service"
			return m_properties.RemoteStagingFolderForWindows;
		}
		else if (idx == remoteApiLib::unix_idx){
			// default value is "/tmp/remoteinstall"
			return m_properties.RemoteStagingFolderForLinux;
		}
		else {
			throw NotImplementedException(__FUNCTION__, "OS index " + idx);
		}
	}

	std::string RcmPushConfig::defaultAgentInstallationPath(enum remoteApiLib::os_idx idx) const
	{
		if (idx == remoteApiLib::windows_idx) {
			return m_properties.AgentInstallationPathForWindows;
		}
		else if (idx == remoteApiLib::unix_idx) {
			return m_properties.AgentInstallationPathForLinux;
		}
		else {
			throw NotImplementedException(__FUNCTION__, "OS index " + idx);
		}
	}

	std::string RcmPushConfig::credStoreCredsFilePath() const 
	{
		std::string legacyCredentialsFilePath = 
			"C:\\ProgramData\\Microsoft Azure Site Recovery\\CredStore\\credentials.json";
		std::string newCredentialsFilePath =
			"C:\\ProgramData\\Microsoft Azure\\CredStore\\credentials.json";

		if (boost::filesystem::exists(newCredentialsFilePath))
		{
			return newCredentialsFilePath;
		}
		else if (boost::filesystem::exists(legacyCredentialsFilePath))
		{
			return legacyCredentialsFilePath;
		}
		else
		{
			throw CredStoreCredsFileNotFoundException(newCredentialsFilePath);
		}
	}

	bool RcmPushConfig::isVmWarePushInstallationEnabled() const 
	{
		return m_tunables.IsVmwarePushInstallationEnabled;
	}

	std::string RcmPushConfig::vmWareWrapperCmd() const 
	{
		std::string defaultWrapperCmd = 
			installPath() + remoteApiLib::pathSeperator() + "vmwarepushinstall.exe";

		if (m_properties.VmwarePushInstallationWrapperCommand == "")
		{
			return defaultWrapperCmd;
		}
		else
		{
			return m_properties.VmwarePushInstallationWrapperCommand;
		}
	}
	std::string RcmPushConfig::osScript() const 
	{
		return installPath() + remoteApiLib::pathSeperator() + "OS_details.sh";
	}

	int RcmPushConfig::jobTimeOutInSecs() const 
	{
		// TODO (rovemula) - Get the ideal value from kusto data.
		return 1800;
	}

	int RcmPushConfig::jobRetries() const 
	{
		return 10;
	}

	int RcmPushConfig::jobRetryIntervalInSecs() const 
	{
		return 30;
	}

	int RcmPushConfig::vmWareCmdTimeOutInSecs() const 
	{
		return 0;
	}

	bool RcmPushConfig::SignVerificationEnabled() const 
	{ 
		return m_tunables.IsSignVerificationEnabled;
	}

	bool RcmPushConfig::SignVerificationEnabledOnRemoteMachine() const
	{
		return m_tunables.IsSignVerificationEnabledOnSourceMachine;
	}

	std::string RcmPushConfig::PushInstallTelemetryLogsPath() const
	{
		throw NotImplementedException(__FUNCTION__, "RCM stack");
	}

	std::string RcmPushConfig::AgentTelemetryLogsPath() const
	{
		throw NotImplementedException(__FUNCTION__, "RCM stack");
	}

	std::string RcmPushConfig::PushServerName()
	{
		return m_properties.PushInstallAgentFriendlyName;
	}
}