//---------------------------------------------------------------
//  <copyright file="pushinstalltelemetry.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Defines routines for push install telemetry.
//  </summary>
//
//  History:     15-Nov-2017    Jaysha    Created
//----------------------------------------------------------------

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <fstream>

#include "pushinstalltelemetry.h"
#include "portablehelpers.h"

using namespace boost::posix_time;

namespace PI
{
	void PushinstallTelemetry::SendTelemetryData(const PushJobDefinition & jobDefinition)
	{
		this->SendPushinstallTelemetry(jobDefinition);
		this->SendAgentinstallTelemetry(jobDefinition);
	}

	void PushinstallTelemetry::SendPushinstallTelemetry(const PushJobDefinition & jobDefinition)
	{
		try
		{
			std::string jobStartTime =
				boost::posix_time::to_iso_extended_string(this->StartTime) + "Z";
			std::string jobEndTime =
				boost::posix_time::to_iso_extended_string(this->EndTime) + "Z";

			std::string tempMetadataFilePath = jobDefinition.PILocalMetadataPath();
			this->WriteTelemetryMetadata(tempMetadataFilePath, jobDefinition);

			boost::property_tree::ptree pt;
			pt.put("ComponentName", "Microsoft Azure Site Recovery Pushinstall Service");
			pt.put("ClientRequestId", jobDefinition.clientRequestId);
			pt.put("ActivityId", jobDefinition.activityId);
			pt.put("ServiceActivityId", jobDefinition.serviceActivityId);
			pt.put("JobID", jobDefinition.jobId());
			pt.put("InstallMode", jobDefinition.GetInstallMode());
			pt.put("JobStatus", this->GetName(this->Status));
			pt.put("ErrorCode", this->ErrorCode);
			pt.put("ErrorCodeName", this->ErrorCodeName);
			pt.put("ComponentErrorCode", this->ComponentErrorCode);
			pt.put("ErrorTags", GetTagsForErrorcode(this->ErrorCodeName));

			pt.put("StartDateTime", jobStartTime);
			pt.put("EndDateTime", jobEndTime);
			pt.put("JobType", this->GetName(jobDefinition.jobtype));
			pt.put("PushServerName", jobDefinition.m_config->PushServerName());
			pt.put("PushServerIP", jobDefinition.m_config->PushServerIp());
			pt.put("PushServerHostId", jobDefinition.m_config->hostid());
			pt.put("CSIP", jobDefinition.m_proxy->csip());
			pt.put("VMtype", this->GetName(jobDefinition.vmType));

			if (jobDefinition.vmType == VMWARE)
			{
				pt.put("VCenterIP", jobDefinition.vCenterIP);
			}

			pt.put("AgentMachineOSType", this->GetOsType(jobDefinition.os_id));
			pt.put("AgentMachineOSName", jobDefinition.getOptionalParam(PI_OSNAME_KEY));
			pt.put("AgentMachineIP", jobDefinition.remoteip);
			pt.put("AgentMachineDisplayName", jobDefinition.vmName);

			std::string tempoutputfilepath = 
				jobDefinition.PITemporarySummaryLogFilePath();
			boost::property_tree::write_json(tempoutputfilepath, pt);
		}
		catch (std::exception &e) 
		{
			DebugPrintf(SV_LOG_ERROR, "%s %s.\n", FUNCTION_NAME, e.what());
		}
		catch (...) 
		{
			DebugPrintf(SV_LOG_ERROR, "%s: Unhandled exception", FUNCTION_NAME);
		}
	}

	void PushinstallTelemetry::SendAgentinstallTelemetry(const PushJobDefinition & jobDefinition)
	{
		try
		{
			std::string tempMetadataFilePath = jobDefinition.AgentLocalMetadataPath();
			this->WriteTelemetryMetadata(tempMetadataFilePath, jobDefinition);

			if (boost::filesystem::exists(jobDefinition.LocalInstallerJsonPath()))
			{
				boost::filesystem::rename(
					jobDefinition.LocalInstallerJsonPath(),
					getLongPathName(
						jobDefinition.AgentTemporaryInstallSummaryFilePath().c_str()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"File %s does not exists.\n",
					jobDefinition.LocalInstallerJsonPath().c_str());
			}

			if (boost::filesystem::exists(jobDefinition.LocalConfiguratorJsonPath()))
			{
				boost::filesystem::rename(
					jobDefinition.LocalConfiguratorJsonPath(),
					getLongPathName(
						jobDefinition.AgentTemporaryConfigurationSummaryFilePath().c_str()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"File %s does not exists.\n",
					jobDefinition.LocalConfiguratorJsonPath().c_str());
			}

			if (boost::filesystem::exists(jobDefinition.LocalInstallerLogPath()))
			{
				boost::filesystem::rename(
					jobDefinition.LocalInstallerLogPath(),
					getLongPathName(
						jobDefinition.AgentTemporaryVerboseLogFilePath().c_str()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"File %s does not exists.\n",
					jobDefinition.LocalInstallerLogPath().c_str());
			}

			if (boost::filesystem::exists(jobDefinition.LocalInstallerErrorLogPath()))
			{
				boost::filesystem::rename(
					jobDefinition.LocalInstallerErrorLogPath(),
					getLongPathName(
						jobDefinition.AgentTemporaryErrorLogFilePath().c_str()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"File %s does not exists.\n",
					jobDefinition.LocalInstallerErrorLogPath().c_str());
			}

			if (boost::filesystem::exists(jobDefinition.LocalInstallerExitStatusPath()))
			{
				boost::filesystem::rename(
					jobDefinition.LocalInstallerExitStatusPath(),
					getLongPathName(
						jobDefinition.AgentTemporaryExitStatusFilePath().c_str()));
			}
			else
			{
				DebugPrintf(
					SV_LOG_DEBUG,
					"File %s does not exists.\n",
					jobDefinition.LocalInstallerExitStatusPath().c_str());
			}
		}
		catch (std::exception &e)
		{
			DebugPrintf(SV_LOG_ERROR, "%s %s.\n", FUNCTION_NAME, e.what());
		}
		catch (...)
		{
			DebugPrintf(SV_LOG_ERROR, "%s: Unhandled exception", FUNCTION_NAME);
		}
	}

	void PushinstallTelemetry::WriteTelemetryMetadata(std::string & metadataFilePath, const PushJobDefinition & jobDefinition)
	{
		boost::property_tree::ptree pt;
		pt.put("ClientRequestId", jobDefinition.clientRequestId);
		pt.put("ActivityId", jobDefinition.activityId);
		pt.put("ServiceActivityId", jobDefinition.serviceActivityId);
		boost::property_tree::write_json(metadataFilePath, pt);
	}
}
