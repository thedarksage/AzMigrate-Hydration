//---------------------------------------------------------------
//  <copyright file="RcmCommunicationImpl.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  RcmCommunicationImpl class implementation.
//  </summary>
//
//  History:     04-Sep-2018    rovemula    Created
//----------------------------------------------------------------

#ifndef __RCM_PROXY_CPP__
#define __RCM_PROXY_CPP__

#include <boost/thread/mutex.hpp>
#include "RcmPushConfig.h"
#include "RcmCommunicationImpl.h"
#include "pushconfig.h"
#include "securityutils.h"
#include "setpermissions.h"
#include "errormessage.h"
#include "ptreeparser.h"
#include "MobilityAgentNotFoundException.h"
#include "PushClientNotFoundException.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>

using namespace boost::property_tree;

namespace PI
{

	RcmCommunicationImplPtr g_rcmproxy;
	bool g_rcmproxycreated = false;
	boost::mutex g_rcmproxyMutex;
	std::string RcmCommunicationImpl::m_jobInputFilePath = "";
	std::string RcmCommunicationImpl::m_jobOutputFilePath = "";
	RcmJob RcmCommunicationImpl::m_rcmJob;
	const std::string sourceMachineAccountId = "AccountId";
	const std::string mobilityAgentRepositoryPath = "MobilityAgentRepository";
	const std::string pushClientRepositoryPath = "PushClientRepository";

	RcmCommunicationImplPtr RcmCommunicationImpl::Instance()
	{
		if (!g_rcmproxycreated) {
			boost::mutex::scoped_lock guard(g_rcmproxyMutex);
			if (!g_rcmproxycreated) {
				g_rcmproxy.reset(new RcmCommunicationImpl());
				g_rcmproxycreated = true;
			}
		}

		return g_rcmproxy;
	}

	RcmCommunicationImpl::RcmCommunicationImpl()
		: m_jobDefinition(PushJobDefinition(
			std::string(),
			0,
			std::string(),
			VM_TYPE::PHYSICAL,
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			false,
			PUSH_JOB_STATUS::INSTALL_JOB_PENDING,
			InstallationMode::WMISSH,
			(PushInstallCommunicationBasePtr)NULL,
			(PushConfigPtr)NULL,
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string()))
	{
		m_isRcmJobStatusUpdated = false;
	}

	void RcmCommunicationImpl::Initialize(std::string & rcmJobInputFilePath, std::string & rcmJobOutputFilePath)
	{
		if (!g_rcmproxycreated) {
			boost::mutex::scoped_lock guard(g_rcmproxyMutex);
			if (!g_rcmproxycreated) {
				g_rcmproxy.reset(new RcmCommunicationImpl());
				g_rcmproxycreated = true;
			}

			RcmCommunicationImpl::m_jobInputFilePath = rcmJobInputFilePath;
			RcmCommunicationImpl::m_jobOutputFilePath = rcmJobOutputFilePath;

			ReadRcmJobFromInputs();
		}
	}

	void RcmCommunicationImpl::ReadRcmJobFromInputs()
	{
		boost::property_tree::ptree jobPt;
		boost::property_tree::read_json(m_jobInputFilePath, jobPt);

		// populate m_rcmJob
		m_rcmJob.Id = GetKeyValueFromPtree<std::string>(jobPt, "Id");
		m_rcmJob.ConsumerId = GetKeyValueFromPtree<std::string>(jobPt, "ConsumerId");
		m_rcmJob.ComponentId = GetKeyValueFromPtree<std::string>(jobPt, "ComponentId");
		m_rcmJob.JobType = GetKeyValueFromPtree<std::string>(jobPt, "JobType");
		m_rcmJob.SessionId = GetKeyValueFromPtree<std::string>(jobPt, "SessionId");
		m_rcmJob.JobStatus = GetKeyValueFromPtree<std::string>(jobPt, "JobStatus");
		m_rcmJob.RcmServiceUri = GetKeyValueFromPtree<std::string>(jobPt, "RcmServiceUri");
		m_rcmJob.InputPayload = GetKeyValueFromPtree<std::string>(jobPt, "InputPayload");
		m_rcmJob.OutputPayload = GetKeyValueFromPtree<std::string>(jobPt, "OutputPayload");
		m_rcmJob.JobCreationimeUtc = GetKeyValueFromPtree<std::string>(jobPt, "JobCreationimeUtc");
		m_rcmJob.JobExpiryTimeUtc = GetKeyValueFromPtree<std::string>(jobPt, "JobExpiryTimeUtc");

		// read the context and populate RcmJob
		m_rcmJob.Context.WorkflowId = GetKeyValueFromPtree<std::string>(jobPt, "Context.WorkflowId");
		m_rcmJob.Context.ActivityId = GetKeyValueFromPtree<std::string>(jobPt, "Context.ActivityId");
		m_rcmJob.Context.ClientRequestId = GetKeyValueFromPtree<std::string>(jobPt, "Context.ClientRequestId");
		m_rcmJob.Context.ContainerId = GetKeyValueFromPtree<std::string>(jobPt, "Context.ContainerId");
		m_rcmJob.Context.ResourceId = GetKeyValueFromPtree<std::string>(jobPt, "Context.ResourceId");
		m_rcmJob.Context.ResourceLocation = GetKeyValueFromPtree<std::string>(jobPt, "Context.ResourceLocation");
		m_rcmJob.Context.AcceptLanguage = GetKeyValueFromPtree<std::string>(jobPt, "Context.AcceptLanguage");
		m_rcmJob.Context.ServiceActivityId = GetKeyValueFromPtree<std::string>(jobPt, "Context.ServiceActivityId");
		m_rcmJob.Context.SrsActivityId = GetKeyValueFromPtree<std::string>(jobPt, "Context.SrsActivityId");
		m_rcmJob.Context.SubscriptionId = GetKeyValueFromPtree<std::string>(jobPt, "Context.SubscriptionId");
		m_rcmJob.Context.AgentMachineId = GetKeyValueFromPtree<std::string>(jobPt, "Context.AgentMachineId");
		m_rcmJob.Context.AgentDiskId = GetKeyValueFromPtree<std::string>(jobPt, "Context.AgentDiskId");
	}

	PushJobDefinition RcmCommunicationImpl::GetPushJobDefinition()
	{
		std::string vCenterAccountId;
		Credential vCenterAccount;
		std::string vmAccountId;
		Credential vmAccount;

		VM_TYPE vmType;
		int os_id;
		std::string remoteIp;
		std::string vCenterIp;
		std::string virtualMachineDisplayName;
		std::string rcmJobType;
		std::string sourceMachineBiosId;
		std::string sourceMachineFqdn;
		std::string clientAuthenticationType;
		std::string mobilityAgentConfigurationFilePath;
		std::string distroName;

		remoteApiLib::os_idx osidx;

		ptree inputPayloadPt;
		std::stringstream sstream;
		sstream << m_rcmJob.InputPayload;
		boost::property_tree::read_json(sstream, inputPayloadPt);

		if (m_rcmJob.JobType == "FetchDistroDetailsOfVMwareVm")
		{
			FetchDistroDetailsOfVMwareVmInput input;

			input.BiosUniqueId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.FullyQualifiedDomainName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "FullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("IpAddresses"))
			{
				input.IpAddresses.push_back(valueType.second.data());
			}

			input.VcenterRunAsAccountId =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterRunAsAccountId");
			input.VcenterServerName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterServerName");
			input.VmDisplayName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmDisplayName");
			input.VmInstanceUuid =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmInstanceUuid");
			input.VmRunAsAccountId =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmRunAsAccountId");
			input.OperatingSystemType =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "OperatingSystemType");
			m_sourceCredAccountId = input.VmRunAsAccountId;

			if (input.OperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			vCenterAccountId = input.VcenterRunAsAccountId;
			vmAccountId = input.VmRunAsAccountId;
			vmType = VM_TYPE::VMWARE;
			vCenterIp = input.VcenterServerName;

			std::string joinedIpList = boost::algorithm::join(input.IpAddresses, ",");
			remoteIp = input.FullyQualifiedDomainName + "," + joinedIpList;
			sourceMachineBiosId = input.BiosUniqueId;
			sourceMachineFqdn = input.FullyQualifiedDomainName;
			virtualMachineDisplayName = input.VmDisplayName;
		}
		else if (m_rcmJob.JobType == "FetchDistroDetailsOfPhysicalServer")
		{
			FetchDistroDetailsOfPhysicalServerInput input;

			input.BiosUniqueId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.FullyQualifiedDomainName = GetKeyValueFromPtree<std::string>(inputPayloadPt, "FullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("IpAddresses"))
			{
				input.IpAddresses.push_back(valueType.second.data());
			}

			input.OperatingSystemType =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "OperatingSystemType");
			if (input.OperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			input.RunAsAccountId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RunAsAccountId");
			m_sourceCredAccountId = input.RunAsAccountId;

			vmAccountId = input.RunAsAccountId;
			vmType = VM_TYPE::PHYSICAL;

			sourceMachineBiosId = input.BiosUniqueId;
			sourceMachineFqdn = input.FullyQualifiedDomainName;
			std::string joinedIpList = boost::algorithm::join(input.IpAddresses, ",");
			remoteIp = input.FullyQualifiedDomainName + "," + joinedIpList;
		}
		else if (m_rcmJob.JobType == "InstallMobilityServiceOnVMwareVm")
		{
			clientAuthenticationType =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "ClientAuthenticationType", "");
			distroName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "DistroName", "");
			RcmProxyDetails rcmProxy;

			if (clientAuthenticationType == "Certificate")
			{
				mobilityAgentConfigurationFilePath =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "MobilityAgentConfigurationFilePath");
			}
			else
			{
				// read RcmProxyDetails
				rcmProxy.AgentVersion =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.AgentVersion");
				rcmProxy.FriendlyName =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.FriendlyName");
				rcmProxy.Id = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.Id");
				rcmProxy.InstallationLocation =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.InstallationLocation");
				rcmProxy.NatIpAddress =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.NatIpAddress");
				rcmProxy.LastHeartbeatUtc =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.LastHeartbeatUtc");
				rcmProxy.Port = GetKeyValueFromPtree<int>(inputPayloadPt, "RcmProxyDetails.Port");
				rcmProxy.ServiceDisplayName =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceDisplayName");
				rcmProxy.ServiceName =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceName");
				rcmProxy.UnregisterCallReceivedTimeUtc =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.UnregisterCallReceivedTimeUtc");

				BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("RcmProxyDetails.IpAddresses"))
				{
					rcmProxy.IpAddresses.push_back(valueType.second.data());
				}
			}

			InstallMobilityServiceOnVMwareVmInput input;

			input.BiosId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.FullyQualifiedDomainName = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "FullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("IpAddresses"))
			{
				input.IpAddresses.push_back(valueType.second.data());
			}

			input.OperatingSystemType = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "OperatingSystemType");
			input.RcmProxyDetails = rcmProxy;
			input.VcenterRunAsAccountId = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterRunAsAccountId");
			input.VcenterServerName = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterServerName");
			input.VmDisplayName = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmDisplayName");
			input.VmInstanceUuid = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmInstanceUuid");
			input.VmRunAsAccountId = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmRunAsAccountId");
			m_sourceCredAccountId = input.VmRunAsAccountId;
			
			vCenterAccountId = input.VcenterRunAsAccountId;
			vmAccountId = input.VmRunAsAccountId;
			vmType = VM_TYPE::VMWARE;
			vCenterIp = input.VcenterServerName;

			if (input.OperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			if (inputPayloadPt.get_child_optional("AgentInstallationPath"))
			{
				std::string installPath = inputPayloadPt.get<std::string>("AgentInstallationPath");
				if (!installPath.empty() && installPath != "null")
				{
					input.MobilityAgentInstallationPath = installPath;
				}
				else
				{
					input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
				}
			}
			else
			{
				input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
			}

			if (clientAuthenticationType == "Certificate")
			{
				// not making this std::string() as push client has a validation for empty csip
				// and removing that check and passing empty csip will break backward compatibility
				m_rcmProxyIp = "NA";
				m_rcmProxyPort = 0;
			}
			else
			{
				if (!rcmProxy.NatIpAddress.empty())
				{
					m_rcmProxyIp = rcmProxy.NatIpAddress;
				}
				else
				{
					m_rcmProxyIp = rcmProxy.IpAddresses.front();
				}

				m_rcmProxyPort = rcmProxy.Port;
			}

			std::string joinedIpList = boost::algorithm::join(input.IpAddresses, ",");
			remoteIp = input.FullyQualifiedDomainName + "," + joinedIpList;
			sourceMachineBiosId = input.BiosId;
			sourceMachineFqdn = input.FullyQualifiedDomainName;
			virtualMachineDisplayName = input.VmDisplayName;
			m_AgentInstallationPath = input.MobilityAgentInstallationPath;
		}
		else if (m_rcmJob.JobType == "InstallMobilityServiceOnPhysicalServer")
		{
			clientAuthenticationType =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "ClientAuthenticationType", "");
			distroName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "DistroName", "");
			RcmProxyDetails rcmProxy;

			if (clientAuthenticationType == "Certificate")
			{
				mobilityAgentConfigurationFilePath =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "MobilityAgentConfigurationFilePath");
			}
			else
			{
				// read RcmProxyDetails
				rcmProxy.AgentVersion =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.AgentVersion");
				rcmProxy.FriendlyName =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.FriendlyName");
				rcmProxy.Id = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.Id");
				rcmProxy.InstallationLocation =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.InstallationLocation");
				rcmProxy.NatIpAddress =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.NatIpAddress");
				rcmProxy.LastHeartbeatUtc =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.LastHeartbeatUtc");
				rcmProxy.Port = GetKeyValueFromPtree<int>(inputPayloadPt, "RcmProxyDetails.Port");
				rcmProxy.ServiceDisplayName =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceDisplayName");
				rcmProxy.ServiceName =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceName");
				rcmProxy.UnregisterCallReceivedTimeUtc =
					GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.UnregisterCallReceivedTimeUtc");

				BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("RcmProxyDetails.IpAddresses"))
				{
					rcmProxy.IpAddresses.push_back(valueType.second.data());
				}
			}

			InstallMobilityServiceOnPhysicalServerInput input;

			input.BiosId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.FullyQualifiedDomainName = GetKeyValueFromPtree<std::string>(inputPayloadPt, "FullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("IpAddresses"))
			{
				input.IpAddresses.push_back(valueType.second.data());
			}

			input.OperatingSystemType = GetKeyValueFromPtree<std::string>(inputPayloadPt, "OperatingSystemType");
			input.RcmProxyDetails = rcmProxy;
			input.RunAsAccountId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RunAsAccountId");
			m_sourceCredAccountId = input.RunAsAccountId;

			vmAccountId = input.RunAsAccountId;
			vmType = VM_TYPE::PHYSICAL;

			if (input.OperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			if (inputPayloadPt.get_child_optional("AgentInstallationPath"))
			{
				std::string installPath = inputPayloadPt.get<std::string>("AgentInstallationPath");
				if (!installPath.empty() && installPath != "null")
				{
					input.MobilityAgentInstallationPath = installPath;
				}
				else
				{
					input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
				}
			}
			else
			{
				input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
			}

			if (clientAuthenticationType == "Certificate")
			{
				// not making this std::string() as push client has a validation for empty csip
				// and removing that check and passing empty csip will break backward compatibility
				m_rcmProxyIp = "NA";
				m_rcmProxyPort = 0;
			}
			else
			{
				if (!rcmProxy.NatIpAddress.empty())
				{
					m_rcmProxyIp = rcmProxy.NatIpAddress;
				}
				else
				{
					m_rcmProxyIp = rcmProxy.IpAddresses.front();
				}

				m_rcmProxyPort = rcmProxy.Port;
			}

			sourceMachineBiosId = input.BiosId;
			sourceMachineFqdn = input.FullyQualifiedDomainName;
			std::string joinedIpList = boost::algorithm::join(input.IpAddresses, ",");
			remoteIp = input.FullyQualifiedDomainName + "," + joinedIpList;
			m_AgentInstallationPath = input.MobilityAgentInstallationPath;
		}
		else if (m_rcmJob.JobType == "UpdateMobilityServiceOnVMwareVm")
		{
			// read RcmProxyDetails
			RcmProxyDetails rcmProxy;
			rcmProxy.AgentVersion =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.AgentVersion");
			rcmProxy.FriendlyName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.FriendlyName");
			rcmProxy.Id = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.Id");
			rcmProxy.InstallationLocation =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.InstallationLocation");
			rcmProxy.NatIpAddress =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.NatIpAddress");
			rcmProxy.LastHeartbeatUtc =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.LastHeartbeatUtc");
			rcmProxy.Port = GetKeyValueFromPtree<int>(inputPayloadPt, "RcmProxyDetails.Port");
			rcmProxy.ServiceDisplayName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceDisplayName");
			rcmProxy.ServiceName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceName");
			rcmProxy.UnregisterCallReceivedTimeUtc =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.UnregisterCallReceivedTimeUtc");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("RcmProxyDetails.IpAddresses"))
			{
				rcmProxy.IpAddresses.push_back(valueType.second.data());
			}

			UpdateMobilityServiceOnVMwareVmInput input;

			input.BiosId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.SourceMachineFullyQualifiedDomainName = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "SourceMachineFullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("SourceMachineIpAddresses"))
			{
				input.SourceMachineIpAddresses.push_back(valueType.second.data());
			}

			input.SourceMachineOperatingSystemType = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "SourceMachineOperatingSystemType");
			input.RcmProxyDetails = rcmProxy;
			input.VcenterRunAsAccountId = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterRunAsAccountId");
			input.VcenterServerName = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterServerName");
			input.VmDisplayName = GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmDisplayName");
			input.VmInstanceUuid = GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmInstanceUuid");
			input.VmRunAsAccountId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmRunAsAccountId");
			m_sourceCredAccountId = input.VmRunAsAccountId;

			sourceMachineBiosId = input.BiosId;
			sourceMachineFqdn = input.SourceMachineFullyQualifiedDomainName;
			vCenterAccountId = input.VcenterRunAsAccountId;
			vmAccountId = input.VmRunAsAccountId;
			vmType = VM_TYPE::VMWARE;
			vCenterIp = input.VcenterServerName;

			if (input.SourceMachineOperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			if (inputPayloadPt.get_child_optional("AgentInstallationPath"))
			{
				std::string installPath = inputPayloadPt.get<std::string>("AgentInstallationPath");
				if (!installPath.empty() && installPath != "null")
				{
					input.MobilityAgentInstallationPath = installPath;
				}
				else
				{
					input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
				}
			}
			else
			{
				input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
			}

			std::string joinedIpList = boost::algorithm::join(input.SourceMachineIpAddresses, ",");
			remoteIp = input.SourceMachineFullyQualifiedDomainName + "," + joinedIpList;
			virtualMachineDisplayName = input.VmDisplayName;
			m_rcmProxyIp = input.RcmProxyDetails.IpAddresses.front();
			m_rcmProxyPort = input.RcmProxyDetails.Port;
			m_AgentInstallationPath = input.MobilityAgentInstallationPath;
		}
		else if (m_rcmJob.JobType == "UpdateMobilityServiceOnPhysicalServer")
		{
			// read RcmProxyDetails
			RcmProxyDetails rcmProxy;
			rcmProxy.AgentVersion =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.AgentVersion");
			rcmProxy.FriendlyName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.FriendlyName");
			rcmProxy.Id = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.Id");
			rcmProxy.InstallationLocation =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.InstallationLocation");
			rcmProxy.NatIpAddress =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.NatIpAddress");
			rcmProxy.LastHeartbeatUtc =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.LastHeartbeatUtc");
			rcmProxy.Port = GetKeyValueFromPtree<int>(inputPayloadPt, "RcmProxyDetails.Port");
			rcmProxy.ServiceDisplayName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceDisplayName");
			rcmProxy.ServiceName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.ServiceName");
			rcmProxy.UnregisterCallReceivedTimeUtc =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "RcmProxyDetails.UnregisterCallReceivedTimeUtc");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("RcmProxyDetails.IpAddresses"))
			{
				rcmProxy.IpAddresses.push_back(valueType.second.data());
			}

			UpdateMobilityServiceOnPhysicalServerInput input;

			input.BiosId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.SourceMachineFullyQualifiedDomainName = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "SourceMachineFullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("SourceMachineIpAddresses"))
			{
				input.SourceMachineIpAddresses.push_back(valueType.second.data());
			}

			input.SourceMachineOperatingSystemType = 
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "SourceMachineOperatingSystemType");
			input.RcmProxyDetails = rcmProxy;
			input.RunAsAccountId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RunAsAccountId");
			m_sourceCredAccountId = input.RunAsAccountId;

			sourceMachineBiosId = input.BiosId;
			sourceMachineFqdn = input.SourceMachineFullyQualifiedDomainName;
			vmAccountId = input.RunAsAccountId;
			vmType = VM_TYPE::PHYSICAL;

			if (input.SourceMachineOperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			if (inputPayloadPt.get_child_optional("AgentInstallationPath"))
			{
				std::string installPath = inputPayloadPt.get<std::string>("AgentInstallationPath");
				if (!installPath.empty() && installPath != "null")
				{
					input.MobilityAgentInstallationPath = installPath;
				}
				else
				{
					input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
				}
			}
			else
			{
				input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
			}

			std::string joinedIpList = boost::algorithm::join(input.SourceMachineIpAddresses, ",");
			remoteIp = input.SourceMachineFullyQualifiedDomainName + "," + joinedIpList;
			m_rcmProxyIp = input.RcmProxyDetails.IpAddresses.front();
			m_rcmProxyPort = input.RcmProxyDetails.Port;
			m_AgentInstallationPath = input.MobilityAgentInstallationPath;
		}
		else if (m_rcmJob.JobType == "ConfigureMobilityServiceOnVMwareVm")
		{
			clientAuthenticationType = "Certificate";
			distroName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "DistroName", "");
			mobilityAgentConfigurationFilePath =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "MobilityAgentConfigurationFilePath");

			ConfigureMobilityServiceOnVMwareVmInput input;

			input.BiosId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.FullyQualifiedDomainName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "FullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("IpAddresses"))
			{
				input.IpAddresses.push_back(valueType.second.data());
			}

			input.OperatingSystemType =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "OperatingSystemType");
			input.VcenterRunAsAccountId =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterRunAsAccountId");
			input.VcenterServerName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VcenterServerName");
			input.VmDisplayName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmDisplayName");
			input.VmInstanceUuid =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmInstanceUuid");
			input.VmRunAsAccountId =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "VmRunAsAccountId");
			m_sourceCredAccountId = input.VmRunAsAccountId;

			vCenterAccountId = input.VcenterRunAsAccountId;
			vmAccountId = input.VmRunAsAccountId;
			vmType = VM_TYPE::VMWARE;
			vCenterIp = input.VcenterServerName;

			if (input.OperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			if (inputPayloadPt.get_child_optional("AgentInstallationPath"))
			{
				std::string installPath = inputPayloadPt.get<std::string>("AgentInstallationPath");
				if (!installPath.empty() && installPath != "null")
				{
					input.MobilityAgentInstallationPath = installPath;
				}
				else
				{
					input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
				}
			}
			else
			{
				input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
			}

			m_rcmProxyIp = "NA";
			m_rcmProxyPort = 0;

			std::string joinedIpList = boost::algorithm::join(input.IpAddresses, ",");
			remoteIp = input.FullyQualifiedDomainName + "," + joinedIpList;
			sourceMachineBiosId = input.BiosId;
			sourceMachineFqdn = input.FullyQualifiedDomainName;
			virtualMachineDisplayName = input.VmDisplayName;
			m_AgentInstallationPath = input.MobilityAgentInstallationPath;
		}
		else if (m_rcmJob.JobType == "ConfigureMobilityServiceOnPhysicalServer")
		{
			clientAuthenticationType = "Certificate";
			distroName =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "DistroName", "");
			mobilityAgentConfigurationFilePath =
				GetKeyValueFromPtree<std::string>(inputPayloadPt, "MobilityAgentConfigurationFilePath");

			ConfigureMobilityServiceOnPhysicalServerInput input;

			input.BiosId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "BiosUniqueId");
			input.FullyQualifiedDomainName = GetKeyValueFromPtree<std::string>(inputPayloadPt, "FullyQualifiedDomainName");

			BOOST_FOREACH(boost::property_tree::ptree::value_type &valueType, inputPayloadPt.get_child("IpAddresses"))
			{
				input.IpAddresses.push_back(valueType.second.data());
			}

			input.OperatingSystemType = GetKeyValueFromPtree<std::string>(inputPayloadPt, "OperatingSystemType");
			input.RunAsAccountId = GetKeyValueFromPtree<std::string>(inputPayloadPt, "RunAsAccountId");
			m_sourceCredAccountId = input.RunAsAccountId;

			vmAccountId = input.RunAsAccountId;
			vmType = VM_TYPE::PHYSICAL;

			if (input.OperatingSystemType == "Windows")
			{
				os_id = WINDOWS_OS;
				osidx = remoteApiLib::os_idx::windows_idx;
			}
			else
			{
				os_id = LINUX_OS;
				osidx = remoteApiLib::os_idx::unix_idx;
			}

			if (inputPayloadPt.get_child_optional("AgentInstallationPath"))
			{
				std::string installPath = inputPayloadPt.get<std::string>("AgentInstallationPath");
				if (!installPath.empty() && installPath != "null")
				{
					input.MobilityAgentInstallationPath = installPath;
				}
				else
				{
					input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
				}
			}
			else
			{
				input.MobilityAgentInstallationPath = RcmPushConfig::Instance()->defaultAgentInstallationPath(osidx);
			}

			m_rcmProxyIp = "NA";
			m_rcmProxyPort = 0;

			sourceMachineBiosId = input.BiosId;
			sourceMachineFqdn = input.FullyQualifiedDomainName;
			std::string joinedIpList = boost::algorithm::join(input.IpAddresses, ",");
			remoteIp = input.FullyQualifiedDomainName + "," + joinedIpList;
			m_AgentInstallationPath = input.MobilityAgentInstallationPath;
		}
		else
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Unknown job type %s received from RCM.",
				m_rcmJob.JobType.c_str());

			throw NotImplementedException(__FUNCTION__, m_rcmJob.JobType);
		}

		m_jobDefinition.jobid = m_rcmJob.Id;
		m_jobDefinition.os_id = os_id;
		m_jobDefinition.remoteip = remoteIp;
		m_jobDefinition.vmType = vmType;
		m_jobDefinition.vCenterIP = vCenterIp;
		m_jobDefinition.vcenterAccountId = vCenterAccountId;
		m_jobDefinition.vmName = virtualMachineDisplayName;
		m_jobDefinition.vmAccountId = vmAccountId;
		m_jobDefinition.clientRequestId = m_rcmJob.Context.ClientRequestId;
		m_jobDefinition.activityId = m_rcmJob.Context.ActivityId;
		m_jobDefinition.serviceActivityId = m_rcmJob.Context.ServiceActivityId;
		m_jobDefinition.installation_path = m_AgentInstallationPath;
		m_jobDefinition.rebootRemoteMachine = false;
		m_jobDefinition.jobStatus = PUSH_JOB_STATUS::INSTALL_JOB_PENDING;
		m_jobDefinition.installationMode = InstallationMode::WMISSH;
		m_jobDefinition.m_proxy = (PushInstallCommunicationBasePtr)(RcmCommunicationImpl::Instance());
		m_jobDefinition.m_config = (PushConfigPtr)(RcmPushConfig::Instance());
		m_jobDefinition.sourceMachineBiosId = sourceMachineBiosId;
		m_jobDefinition.sourceMachineFqdn = sourceMachineFqdn;
		m_jobDefinition.pushInstallIdFolderName = std::string();
		m_jobDefinition.pushInstallIdFileNamePrefix = std::string();

		if (clientAuthenticationType == "Certificate")
		{
			m_jobDefinition.m_clientAuthenticationType = ClientAuthenticationType::Certificate;
		}
		else
		{
			m_jobDefinition.m_clientAuthenticationType = ClientAuthenticationType::Passphrase;
		}

		if (distroName != "")
		{
			if (distroName == "Windows")
			{
				m_jobDefinition.setOptionalParam(PI_OSNAME_KEY, "Win");
			}
			else
			{
				m_jobDefinition.setOptionalParam(PI_OSNAME_KEY, distroName);
			}
		}

		m_jobDefinition.mobilityAgentConfigurationFilePath = mobilityAgentConfigurationFilePath;
		if (m_jobDefinition.IsDistroFetchRequest(m_rcmJob.JobType))
		{
			m_jobDefinition.jobtype = PushJobType::fetchDistroDetails;
		}
		else if (m_jobDefinition.IsConfigurationRequest(m_rcmJob.JobType))
		{
			m_jobDefinition.jobtype = PushJobType::pushConfigure;
		}
		else if (m_jobDefinition.IsInstallationRequest())
		{
			m_jobDefinition.jobtype = PushJobType::pushFreshInstall;
		}
		else
		{
			m_jobDefinition.jobtype = PushJobType::pushUninstall;
		}

		if (vCenterAccountId != "")
		{
			std::string::size_type idx = vCenterAccountId.find_last_of("/");
			vCenterAccountId = vCenterAccountId.substr(idx + 1);

			vCenterAccount =
				CredentialMgr::Instance()->GetDecryptedAccount(vCenterAccountId);
		}

		std::string vmDomain = "";
		std::string vmUsername = "";

		if (vmAccountId != "")
		{
			vmAccount = CredentialMgr::Instance()->GetDecryptedAccount(vmAccountId);

			vmUsername = vmAccount.UserName;

			std::string::size_type idx = vmAccount.UserName.find("\\");
			if (idx != std::string::npos)
			{
				vmDomain = vmAccount.UserName.substr(0, idx);
				vmUsername = vmAccount.UserName.substr(idx + 1);
			}

			idx = vmAccount.UserName.find("@");
			if (idx != std::string::npos)
			{
				vmDomain = vmAccount.UserName.substr(idx + 1);
				vmUsername = vmAccount.UserName.substr(0, idx);
			}
		}

		m_jobDefinition.vCenterUserName = vCenterAccount.UserName;
		m_jobDefinition.vCenterPassword = vCenterAccount.Password;
		m_jobDefinition.domain = vmDomain;
		m_jobDefinition.username = vmUsername;
		m_jobDefinition.password = vmAccount.Password;
		m_PassphraseFilePath = m_jobDefinition.GetPassphraseTemporaryFilePath();

		return m_jobDefinition;
	}

	PUSH_INSTALL_JOB RcmCommunicationImpl::GetInstallDetails(
		const std::string& jobId,
		std::string osName,
		bool isFreshInstall)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s jobid: %s  os: %s\n",
			__FUNCTION__,
			jobId.c_str(),
			osName.c_str());

		PUSH_INSTALL_JOB job;
		// m_OsType field is not being used anywhere. Hence, storing some default value here.
		job.m_OsType = 1;
		INSTALL_TASK task;

		if (osName == "Win")
		{
			if (isFreshInstall)
			{
				task.m_commandLineOptions = " /Role \"MS\" /Platform VmWare /Silent ";
				task.m_commandLineOptions += " /InstallationType Install /CSType CSPrime ";
			}
			else
			{
				task.m_commandLineOptions = " /Platform VmWare /Silent ";
				task.m_commandLineOptions += " /InstallationType Upgrade /CSType CSPrime ";
			}

			task.m_commandLineOptions += " /InstallLocation \"" + m_AgentInstallationPath + "\" ";
		}
		else
		{
			if (isFreshInstall)
			{
				task.m_commandLineOptions = "-r \"MS\" -v VmWare -q";
				task.m_commandLineOptions += " -a Install -c CSPrime";
			}
			else
			{
				task.m_commandLineOptions = "-v VmWare -q";
				task.m_commandLineOptions += " -a Upgrade -c CSPrime";
			}

			task.m_commandLineOptions += " -d \"" + m_AgentInstallationPath + "\"";
		}

		task.type_of_install = 
			isFreshInstall ? INSTALL_TYPE::FRESH_INSTALL : INSTALL_TYPE::UPGRADE_INSTALL;
		job.m_installTasks.push_back(task);

		return job;
	}

	bool RcmCommunicationImpl::UpdateAgentInstallationStatus(
		const std::string& jobId,
		PUSH_JOB_STATUS state,
		const std::string& statusString,
		const std::string& hostId,
		const std::string& log,
		const std::string &errorCode,
		const PlaceHoldersMap &placeHolders,
		const std::string& installerExtendedErrorsJson,
		const std::string& distroName)
	{
		if (!m_isRcmJobStatusUpdated && 
			(state == INSTALL_JOB_COMPLETED ||
			state == INSTALL_JOB_FAILED ||
			state == UNINSTALL_JOB_COMPLETED ||
			state == UNINSTALL_JOB_FAILED ||
			state == INSTALL_JOB_COMPLETED_WITH_WARNINGS))
		{
			std::string mappedErrorCode = GetErrorNameForErrorcode(errorCode);
			if (mappedErrorCode == std::string())
			{
				mappedErrorCode = errorCode;
			}

			DebugPrintf(
				SV_LOG_ALWAYS,
				"Updating job status for job %s, jobtype %d. ErrorCode: %s, mappedErrorCode: %s\n",
				jobId.c_str(),
				m_jobDefinition.jobtype,
				errorCode.c_str(),
				mappedErrorCode.c_str());

			// AGENT_SETUP_COMPLETED_WITH_WARNINGS is also reported as INSTALL_JOB_FAILED (common in both CS and RCM stack).
			// So, adding a check for this error code as well (job status should be success for this error code).
			if (state == INSTALL_JOB_COMPLETED ||
				state == INSTALL_JOB_COMPLETED_WITH_WARNINGS ||
				mappedErrorCode == "AGENT_INSTALLATION_SUCCESS" ||
				mappedErrorCode == "AGENT_SETUP_COMPLETED_WITH_WARNINGS")
			{
				m_rcmJob.JobStatus = "Finished";
			}
			else
			{
				m_rcmJob.JobStatus = "Failed";
			}

			std::map<std::string, std::string> jobPlaceHolders;
			jobPlaceHolders[sourceMachineAccountId] = m_sourceCredAccountId;
			jobPlaceHolders[mobilityAgentRepositoryPath] = RcmPushConfig::Instance()->localRepositoryUAPath();
			jobPlaceHolders[pushClientRepositoryPath] = RcmPushConfig::Instance()->localRepositoryPushclientPath();
			if (placeHolders.find(psIp) == placeHolders.end()) {
				std::string pushServerIp = Host::GetInstance().GetIPAddress();
				jobPlaceHolders[psIp] = pushServerIp;
			}
			if (placeHolders.find(psName) == placeHolders.end()) {
				std::string pushServerName = Host::GetInstance().GetHostName();
				jobPlaceHolders[psName] = pushServerName;
			}
			if (placeHolders.find(sourceIp) == placeHolders.end()) {
				jobPlaceHolders[sourceIp] = m_jobDefinition.remoteip;
			}
			if (placeHolders.find(sourceName) == placeHolders.end()) {
				jobPlaceHolders[sourceName] = m_jobDefinition.vmName;
			}
			if (placeHolders.find(csIp) == placeHolders.end()) {
				jobPlaceHolders[csIp] = RcmCommunicationImpl::Instance()->csip();
			}
			if (placeHolders.find(csPort) == placeHolders.end()) {
				jobPlaceHolders[csPort] = RcmCommunicationImpl::Instance()->csport();
			}
			if (placeHolders.find(errCode) == placeHolders.end()) {
				jobPlaceHolders[errCode] = mappedErrorCode;
			}

			std::string tag = GetTagsForErrorcode(mappedErrorCode);

			// Adding extended errors in rcmJob.
			if (installerExtendedErrorsJson != "")
			{
				std::stringstream ss;
				ss << installerExtendedErrorsJson;

				boost::property_tree::iptree extendedErrorsPt;
				boost::property_tree::read_json(ss, extendedErrorsPt);

				ExtendedErrors extendedErrors;
				BOOST_FOREACH(boost::property_tree::iptree::value_type &valueType, extendedErrorsPt.get_child("errors"))
				{
					ExtendedError error;

					// getting error_name which is mandatory field. if this is not present, we will skip adding this extended error.
					if (!valueType.second.get_child_optional("error_name"))
					{
						DebugPrintf(
							SV_LOG_ERROR,
							"Could not find error_name mandatory parameter in extended errors json. Json content: %s", installerExtendedErrorsJson.c_str());
						continue;
					}

					error.error_name = valueType.second.get<std::string>("error_name");

					if (valueType.second.get_child_optional("error_params"))
					{
						BOOST_FOREACH(boost::property_tree::iptree::value_type &errorParamValueType, valueType.second.get_child("error_params"))
						{
							error.error_params.insert(std::pair<std::string, std::string>(errorParamValueType.first, errorParamValueType.second.data()));
						}
					}

					if (valueType.second.get_child_optional("default_message"))
					{
						error.default_message = valueType.second.get<std::string>("default_message");
					}

					extendedErrors.errors.push_back(error);
				}

				std::string errorSeverity = "Error";
				if (mappedErrorCode == "AGENT_SETUP_COMPLETED_WITH_WARNINGS")
				{
					errorSeverity = "Warning";
				}
				else
				{
					errorSeverity = "Error";
				}

				for each (ExtendedError extendedError in extendedErrors.errors)
				{
					RcmJobError rcmError;
					rcmError.ErrorCode = extendedError.error_name;
					rcmError.Message = extendedError.default_message;
					rcmError.Severity = errorSeverity;
					rcmError.MessageParameters.insert(
						extendedError.error_params.begin(),
						extendedError.error_params.end());

					rcmError.ComponentError.ComponentId = "SourceAgent";
					rcmError.ComponentError.ErrorCode = extendedError.error_name;
					rcmError.ComponentError.Message = extendedError.default_message;
					rcmError.ComponentError.Severity = errorSeverity;
					rcmError.ComponentError.MessageParameters.insert(
						extendedError.error_params.begin(),
						extendedError.error_params.end());

					m_rcmJob.Errors.push_back(rcmError);
				}
			}

			if (tag == "AgentError")
			{
				if (state == INSTALL_JOB_FAILED ||
					state == INSTALL_JOB_COMPLETED_WITH_WARNINGS ||
					state == UNINSTALL_JOB_FAILED)
				{
					std::string errorSeverity = "";
					if (mappedErrorCode == "AGENT_SETUP_COMPLETED_WITH_WARNINGS")
					{
						errorSeverity = "Warning";
					}
					else
					{
						errorSeverity = "Error";
					}

					RcmJobError error;
					error.ErrorCode = mappedErrorCode;
					error.Severity = errorSeverity;
					error.MessageParameters.insert(placeHolders.begin(), placeHolders.end());
					error.MessageParameters.insert(jobPlaceHolders.begin(), jobPlaceHolders.end());

					error.ComponentError.ComponentId = "SourceAgent";
					error.ComponentError.ErrorCode = mappedErrorCode;
					error.ComponentError.Severity = errorSeverity;
					error.ComponentError.MessageParameters.insert(
						placeHolders.begin(),
						placeHolders.end());
					error.ComponentError.MessageParameters.insert(
						jobPlaceHolders.begin(),
						jobPlaceHolders.end());

					m_rcmJob.Errors.push_back(error);
				}
			}
			else
			{
				if (state == INSTALL_JOB_FAILED ||
					state == INSTALL_JOB_COMPLETED_WITH_WARNINGS ||
					state == UNINSTALL_JOB_FAILED)
				{
					std::string errorSeverity = "Error";
					RcmJobError error;
					error.ErrorCode = mappedErrorCode;
					error.Severity = errorSeverity;
					error.MessageParameters.insert(placeHolders.begin(), placeHolders.end());
					error.MessageParameters.insert(jobPlaceHolders.begin(), jobPlaceHolders.end());

					m_rcmJob.Errors.push_back(error);
				}
			}

			if (state == INSTALL_JOB_COMPLETED && m_jobDefinition.jobtype == PushJobType::fetchDistroDetails)
			{
				ptree outputPayloadPt;

				if (distroName == "Win")
				{
					outputPayloadPt.put("DistroName", "Windows");
				}
				else
				{
					outputPayloadPt.put("DistroName", distroName);
				}
				std::ostringstream buf;
				boost::property_tree::json_parser::write_json(buf, outputPayloadPt);
				m_rcmJob.OutputPayload = buf.str();
			}

			ptree jobPt = GetRcmJobPtree();
			if (!m_jobOutputFilePath.empty())
			{
				ofstream ofs(m_jobOutputFilePath);
				boost::property_tree::json_parser::write_json(ofs, jobPt);
				ofs.flush();
				ofs.close();
			}

			m_isRcmJobStatusUpdated = true;
		}

		return true;
	}

	void RcmCommunicationImpl::DownloadInstallerFile(const std::string & remotepath, const std::string & localpath)
	{
		throw NotImplementedException(__FUNCTION__, "RCM stack");
	}

	std::string RcmCommunicationImpl::GetPushClientDetails(
		const std::string & jobId,
		const std::string& osName)
	{
		throw PushClientNotFoundException(osName);
	}


	std::string RcmCommunicationImpl::GetBuildPath(const std::string & jobId, const std::string& osName)
	{
		throw MobilityAgentNotFoundException(osName);
	}

	std::string RcmCommunicationImpl::csip() const
	{
		return m_rcmProxyIp;
	}

	int RcmCommunicationImpl::csport() const
	{
		return m_rcmProxyPort;
	}

	std::string RcmCommunicationImpl::GetPassphrase()
	{
		return CredentialMgr::Instance()->GetPassphrase();
	}

	std::string RcmCommunicationImpl::GetPassphraseFilePath()
	{
		std::string passphrase = CredentialMgr::Instance()->GetPassphrase();

		ofstream passphraseFile;
		passphraseFile.open(m_PassphraseFilePath);
		passphraseFile << passphrase;
		passphraseFile.flush();
		passphraseFile.close();

		securitylib::setPermissions(m_PassphraseFilePath);

		return m_PassphraseFilePath;
	}

	bool RcmCommunicationImpl::RegisterPushInstaller()
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s\n",
			__FUNCTION__);

		throw NotImplementedException(__FUNCTION__, "RCM stack");
	}

	bool RcmCommunicationImpl::UnregisterPushInstaller()
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s\n",
			__FUNCTION__);

		throw NotImplementedException(__FUNCTION__, "RCM stack");
	}

	PushInstallationSettings RcmCommunicationImpl::GetSettings()
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s\n",
			__FUNCTION__);

		throw NotImplementedException(__FUNCTION__, "RCM stack");
	}

	boost::property_tree::ptree RcmCommunicationImpl::GetRcmJobPtree()
	{
		boost::property_tree::ptree pt;

		pt.put("Id", m_rcmJob.Id);
		pt.put("ConsumerId", m_rcmJob.ConsumerId);
		pt.put("ComponentId", m_rcmJob.ComponentId);
		pt.put("JobType", m_rcmJob.JobType);
		pt.put("SessionId", m_rcmJob.SessionId);
		pt.put("JobStatus", m_rcmJob.JobStatus);
		pt.put("RcmServiceUri", m_rcmJob.RcmServiceUri);
		pt.put("InputPayload", m_rcmJob.InputPayload);
		pt.put("OutputPayload", m_rcmJob.OutputPayload);
		pt.put("JobCreationimeUtc", m_rcmJob.JobCreationimeUtc);
		pt.put("JobExpiryTimeUtc", m_rcmJob.JobExpiryTimeUtc);

		pt.put("Context.WorkflowId", m_rcmJob.Context.WorkflowId);
		pt.put("Context.ActivityId", m_rcmJob.Context.ActivityId);
		pt.put("Context.ClientRequestId", m_rcmJob.Context.ClientRequestId);
		pt.put("Context.ContainerId", m_rcmJob.Context.ContainerId);
		pt.put("Context.ResourceId", m_rcmJob.Context.ResourceId);
		pt.put("Context.ResourceLocation", m_rcmJob.Context.ResourceLocation);
		pt.put("Context.AcceptLanguage", m_rcmJob.Context.AcceptLanguage);
		pt.put("Context.ServiceActivityId", m_rcmJob.Context.ServiceActivityId);
		pt.put("Context.SrsActivityId", m_rcmJob.Context.SrsActivityId);
		pt.put("Context.SubscriptionId", m_rcmJob.Context.SubscriptionId);
		pt.put("Context.AgentMachineId", m_rcmJob.Context.AgentMachineId);
		pt.put("Context.AgentDiskId", m_rcmJob.Context.AgentDiskId);

		ptree errorsPt;

		for each (RcmJobError error in m_rcmJob.Errors)
		{
			ptree errorPt;
			errorPt.put("ErrorCode", error.ErrorCode);
			errorPt.put("Message", error.Message);
			errorPt.put("PossibleCauses", error.PossibleCauses);
			errorPt.put("RecommendedAction", error.RecommendedAction);
			errorPt.put("Severity", error.Severity);

			if (error.ComponentError.ErrorCode != "")
			{
				errorPt.put("ComponentError.ErrorCode", error.ComponentError.ErrorCode);
				errorPt.put("ComponentError.ComponentId", error.ComponentError.ComponentId);
				errorPt.put("ComponentError.Message", error.ComponentError.Message);
				errorPt.put("ComponentError.PossibleCauses", error.ComponentError.PossibleCauses);
				errorPt.put("ComponentError.RecommendedAction", error.ComponentError.RecommendedAction);
				errorPt.put("ComponentError.Severity", error.ComponentError.Severity);

				ptree componentErrorsMessageParametersPt;
				for each (auto param in error.ComponentError.MessageParameters)
				{
					componentErrorsMessageParametersPt.put(param.first, param.second);
				}
				errorPt.add_child("ComponentError.MessageParameters", componentErrorsMessageParametersPt);
			}

			ptree messageParametersPt;
			for each (auto param in error.MessageParameters)
			{
				messageParametersPt.put(param.first, param.second);
			}
			errorPt.add_child("MessageParameters", messageParametersPt);

			errorsPt.add_child("", errorPt);
		}

		pt.add_child("Errors", errorsPt);

		return pt;
	}
}


#endif