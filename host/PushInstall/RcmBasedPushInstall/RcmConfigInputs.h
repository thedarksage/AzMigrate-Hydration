/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	RcmConfigInputs.h

Description	:   This file contains the tunables and properties for install and upgrade mobility 
				service tasks received from RCM.

+------------------------------------------------------------------------------------+
*/

#ifndef __RCM_CONFIG_INPUTS_H__
#define __RCM_CONFIG_INPUTS_H__

#include <string>

namespace PI
{

	class RcmServiceTunables
	{
	public:
		~RcmServiceTunables() { }

		bool IsVmwarePushInstallationEnabled;
		bool IsSignVerificationEnabled;
		bool IsSignVerificationEnabledOnSourceMachine;
	};

	class RcmPushInstallationProperties
	{
	public:
		~RcmPushInstallationProperties() { }

		std::string PushInstallAgentMachineId;
		std::string PushInstallAgentFriendlyName;
		std::string PushInstallAgentInstallationPath;
		std::string RemoteStagingFolderForWindows;
		std::string RemoteStagingFolderForLinux;
		std::string VmwarePushInstallationWrapperCommand;
		std::string PushinstallClientsLocalRepository;
		std::string MobilityAgentsLocalRepository;
		std::string AgentInstallationPathForWindows;
		std::string AgentInstallationPathForLinux;
	};
}

#endif