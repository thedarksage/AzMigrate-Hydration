/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	RcmJobInputs.h

Description	:   This file contains the input definitions for install and upgrade mobility service 
				tasks received from RCM.

+------------------------------------------------------------------------------------+
*/

#ifndef __RCM_JOB_INPUTS_H__
#define __RCM_JOB_INPUTS_H__

#include <string>
#include <vector>
#include "json_reader.h"
#include "json_writer.h"
#include "json_adapter.h"
#include "svtypes.h"

namespace PI
{

	class RcmProxyDetails
	{
	public:
		~RcmProxyDetails() { }

		std::string Id;
		std::string FriendlyName;
		std::string AgentVersion;
		std::string InstallationLocation;
		std::string ServiceName;
		std::string ServiceDisplayName;
		std::string LastHeartbeatUtc;
		std::string UnregisterCallReceivedTimeUtc;
		std::vector<std::string> IpAddresses;
		std::string NatIpAddress;
		SV_INT Port;

		/// \brief serializes RcmProxyDetails class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "RcmProxyDetails", false);

			JSON_E(adapter, Id);
			JSON_E(adapter, FriendlyName);
			JSON_E(adapter, AgentVersion);
			JSON_E(adapter, InstallationLocation);
			JSON_E(adapter, ServiceName);
			JSON_E(adapter, ServiceDisplayName);
			JSON_E(adapter, LastHeartbeatUtc);
			JSON_E(adapter, UnregisterCallReceivedTimeUtc);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, NatIpAddress);
			JSON_T(adapter, Port);
		}

		RcmProxyDetails &operator=(const RcmProxyDetails & rhs)
		{
			if (this == &rhs) {
				return *this;
			}

			Id = rhs.Id;
			FriendlyName = rhs.FriendlyName;
			AgentVersion = rhs.AgentVersion;
			InstallationLocation = rhs.InstallationLocation;
			ServiceName = rhs.ServiceName;
			ServiceDisplayName = rhs.ServiceDisplayName;
			LastHeartbeatUtc = rhs.LastHeartbeatUtc;
			UnregisterCallReceivedTimeUtc = rhs.UnregisterCallReceivedTimeUtc;
			NatIpAddress = rhs.NatIpAddress;
			Port = rhs.Port;

			BOOST_FOREACH(std::string ipAddress, rhs.IpAddresses)
			{
				IpAddresses.push_back(ipAddress);
			}
		}
	};

	class FetchDistroDetailsInputBase
	{
	public:
		~FetchDistroDetailsInputBase() {}

		std::string FullyQualifiedDomainName;
		std::vector<std::string> IpAddresses;
		std::string BiosUniqueId;
		std::string OperatingSystemType;

		/// \brief serializes FetchDistroDetailsInputBase class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "FetchDistroDetailsInputBase", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, BiosUniqueId);
			JSON_T(adapter, OperatingSystemType);
		}
	};

	class FetchDistroDetailsOfVMwareVmInput : public FetchDistroDetailsInputBase
	{
	public:
		~FetchDistroDetailsOfVMwareVmInput() {}

		std::string VcenterServerName;
		std::string VcenterServerPort;
		std::string VcenterRunAsAccountId;
		std::string VmInstanceUuid;
		std::string VmRunAsAccountId;
		std::string VmDisplayName;

		/// \brief serializes FetchDistroDetailsOfVMwareVmInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "FetchDistroDetailsOfVMwareVmInput", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, VcenterServerName);
			JSON_E(adapter, VcenterServerPort);
			JSON_E(adapter, VcenterRunAsAccountId);
			JSON_E(adapter, VmInstanceUuid);
			JSON_E(adapter, VmRunAsAccountId);
			JSON_E(adapter, VmDisplayName);
			JSON_T(adapter, BiosUniqueId);
		}
	};

	class FetchDistroDetailsOfPhysicalServerInput : public FetchDistroDetailsInputBase
	{
	public:
		~FetchDistroDetailsOfPhysicalServerInput() { }

		std::string RunAsAccountId;

		/// \brief serializes FetchDistroDetailsOfPhysicalServerInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "FetchDistroDetailsOfPhysicalServerInput", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, RunAsAccountId);
			JSON_T(adapter, BiosUniqueId);
		}
	};

	class InstallMobilityServiceInputBase
	{
	public:
		~InstallMobilityServiceInputBase() { }

		std::string FullyQualifiedDomainName;
		std::vector<std::string> IpAddresses;
		std::string OperatingSystemType;
		RcmProxyDetails RcmProxyDetails;
		std::string BiosId;
		std::string MobilityAgentInstallationPath;
		std::string ClientAuthenticationType;
		std::string MobilityAgentConfigurationFilePath;
		std::string DistroName;

		/// \brief serializes InstallMobilityServiceInputBase class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "InstallMobilityServiceInputBase", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, OperatingSystemType);
			JSON_E(adapter, RcmProxyDetails);
			JSON_E(adapter, BiosId);
			JSON_E(adapter, MobilityAgentInstallationPath);
			JSON_E(adapter, ClientAuthenticationType);
			JSON_E(adapter, MobilityAgentConfigurationFilePath);
			JSON_T(adapter, DistroName);
		}
	};

	class InstallMobilityServiceOnVMwareVmInput : public InstallMobilityServiceInputBase
	{
	public:
		~InstallMobilityServiceOnVMwareVmInput() { }

		std::string VcenterServerName;
		std::string VcenterServerPort;
		std::string VcenterRunAsAccountId;
		std::string VmInstanceUuid;
		std::string VmRunAsAccountId;
		std::string VmDisplayName;

		/// \brief serializes InstallMobilityServiceOnVMwareVmInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "InstallMobilityServiceOnVMwareVmInput", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, OperatingSystemType);
			JSON_E(adapter, RcmProxyDetails);
			JSON_E(adapter, VcenterServerName);
			JSON_E(adapter, VcenterServerPort);
			JSON_E(adapter, VcenterRunAsAccountId);
			JSON_E(adapter, VmInstanceUuid);
			JSON_E(adapter, VmRunAsAccountId);
			JSON_E(adapter, VmDisplayName);
			JSON_E(adapter, BiosId);
			JSON_E(adapter, MobilityAgentInstallationPath);
			JSON_E(adapter, ClientAuthenticationType);
			JSON_E(adapter, MobilityAgentConfigurationFilePath);
			JSON_T(adapter, DistroName);
		}
	};

	class InstallMobilityServiceOnPhysicalServerInput : public InstallMobilityServiceInputBase
	{
	public:
		~InstallMobilityServiceOnPhysicalServerInput() { }

		std::string RunAsAccountId;

		/// \brief serializes InstallMobilityServiceOnPhysicalServerInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "InstallMobilityServiceOnPhysicalServerInput", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, OperatingSystemType);
			JSON_E(adapter, RcmProxyDetails);
			JSON_E(adapter, RunAsAccountId);
			JSON_E(adapter, BiosId);
			JSON_E(adapter, MobilityAgentInstallationPath);
			JSON_E(adapter, ClientAuthenticationType);
			JSON_E(adapter, MobilityAgentConfigurationFilePath);
			JSON_T(adapter, DistroName);
		}
	};

	class UpdateMobilityServiceOnVMwareVmInput
	{
	public:
		~UpdateMobilityServiceOnVMwareVmInput() { }

		std::string JobId;
		std::string SourceMachineFullyQualifiedDomainName;
		std::vector<std::string> SourceMachineIpAddresses;
		std::string SourceMachineOperatingSystemType;
		RcmProxyDetails RcmProxyDetails;
		std::string VcenterServerName;
		std::string VcenterRunAsAccountId;
		std::string VmInstanceUuid;
		std::string VmRunAsAccountId;
		std::string VmDisplayName;
		std::string BiosId;
		std::string MobilityAgentInstallationPath;

		/// \brief serializes UpdateMobilityServiceOnVMwareVmInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "UpdateMobilityServiceOnVMwareVmInput", false);

			JSON_E(adapter, JobId);
			JSON_E(adapter, SourceMachineFullyQualifiedDomainName);
			JSON_E(adapter, SourceMachineIpAddresses);
			JSON_E(adapter, SourceMachineOperatingSystemType);
			JSON_E(adapter, RcmProxyDetails);
			JSON_E(adapter, VcenterServerName);
			JSON_E(adapter, VcenterRunAsAccountId);
			JSON_E(adapter, VmInstanceUuid);
			JSON_E(adapter, VmRunAsAccountId);
			JSON_E(adapter, VmDisplayName);
			JSON_E(adapter, BiosId);
			JSON_T(adapter, MobilityAgentInstallationPath);
		}
	};

	class UpdateMobilityServiceOnPhysicalServerInput
	{
	public:
		~UpdateMobilityServiceOnPhysicalServerInput() { }

		std::string JobId;
		std::string SourceMachineFullyQualifiedDomainName;
		std::vector<std::string> SourceMachineIpAddresses;
		std::string SourceMachineOperatingSystemType;
		RcmProxyDetails RcmProxyDetails;
		std::string RunAsAccountId;
		std::string BiosId;
		std::string MobilityAgentInstallationPath;

		/// \brief serializes UpdateMobilityServiceOnPhysicalServerInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "UpdateMobilityServiceOnPhysicalServerInput", false);

			JSON_E(adapter, JobId);
			JSON_E(adapter, SourceMachineFullyQualifiedDomainName);
			JSON_E(adapter, SourceMachineIpAddresses);
			JSON_E(adapter, SourceMachineOperatingSystemType);
			JSON_E(adapter, RcmProxyDetails);
			JSON_E(adapter, RunAsAccountId);
			JSON_E(adapter, BiosId);
			JSON_T(adapter, MobilityAgentInstallationPath);
		}
	};

	class ConfigureMobilityServiceInputBase
	{
	public:
		~ConfigureMobilityServiceInputBase() { }

		std::string FullyQualifiedDomainName;
		std::vector<std::string> IpAddresses;
		std::string OperatingSystemType;
		std::string BiosId;
		std::string MobilityAgentInstallationPath;
		std::string MobilityAgentConfigurationFilePath;
		std::string DistroName;

		/// \brief serializes ConfigureMobilityServiceInputBase class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "ConfigureMobilityServiceInputBase", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, OperatingSystemType);
			JSON_E(adapter, BiosId);
			JSON_E(adapter, MobilityAgentInstallationPath);
			JSON_E(adapter, MobilityAgentConfigurationFilePath);
			JSON_T(adapter, DistroName);
		}
	};

	class ConfigureMobilityServiceOnVMwareVmInput : public ConfigureMobilityServiceInputBase
	{
	public:
		~ConfigureMobilityServiceOnVMwareVmInput() { }

		std::string VcenterServerName;
		std::string VcenterServerPort;
		std::string VcenterRunAsAccountId;
		std::string VmInstanceUuid;
		std::string VmRunAsAccountId;
		std::string VmDisplayName;

		/// \brief serializes ConfigureMobilityServiceOnVMwareVmInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "ConfigureMobilityServiceOnVMwareVmInput", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, OperatingSystemType);
			JSON_E(adapter, VcenterServerName);
			JSON_E(adapter, VcenterServerPort);
			JSON_E(adapter, VcenterRunAsAccountId);
			JSON_E(adapter, VmInstanceUuid);
			JSON_E(adapter, VmRunAsAccountId);
			JSON_E(adapter, VmDisplayName);
			JSON_E(adapter, BiosId);
			JSON_E(adapter, MobilityAgentInstallationPath);
			JSON_E(adapter, MobilityAgentConfigurationFilePath);
			JSON_T(adapter, DistroName);
		}
	};

	class ConfigureMobilityServiceOnPhysicalServerInput : public ConfigureMobilityServiceInputBase
	{
	public:
		~ConfigureMobilityServiceOnPhysicalServerInput() { }

		std::string RunAsAccountId;

		/// \brief serializes ConfigureMobilityServiceOnPhysicalServerInput class
		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "ConfigureMobilityServiceOnPhysicalServerInput", false);

			JSON_E(adapter, FullyQualifiedDomainName);
			JSON_E(adapter, IpAddresses);
			JSON_E(adapter, OperatingSystemType);
			JSON_E(adapter, RunAsAccountId);
			JSON_E(adapter, BiosId);
			JSON_E(adapter, MobilityAgentInstallationPath);
			JSON_E(adapter, MobilityAgentConfigurationFilePath);
			JSON_T(adapter, DistroName);
		}
	};
}

#endif
