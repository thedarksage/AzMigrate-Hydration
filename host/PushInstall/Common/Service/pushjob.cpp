///
/// \file pushjob.cpp
///
/// \brief
///


#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/ip/address.hpp>
#include <fstream>

#include "logger.h"
#include "portable.h"

#include "cdputil.h"
#include "getpassphrase.h"

#include "errorexception.h"
#include "remoteapi.h"
#include "remoteconnection.h"
#include "pushconfig.h"
#include "PushInstallSWMgrBase.h"
#include "pushjobspec.h"
#include "pushjob.h"
#include "credentialerrorexception.h"
#include "WmiErrorException.h"
#include "host.h"
#include "converterrortostringminor.h"
#include "errorexceptionmajor.h"
#include "errormessage.h"
#include "nonretryableerrorexception.h"
#include "securityutils.h"
#include "appcommand.h"
#include "pushinstalltelemetry.h"
#include "MobilityAgentNotFoundException.h"
#include "PushClientNotFoundException.h"
#include "RemoteApiErrorException.h"
#include "RemoteApiErrorCode.h"

using namespace remoteApiLib;
using boost::property_tree::ptree;
using boost::property_tree::write_json;
using namespace boost::posix_time;

namespace PI {

	void PushJob::execute()
	{
		VmWareBasedInstallStatus vmWareBasedInstallStatus = NotAttempted;
		if (m_jobdefinition.UseVmwareBasedInstall())
		{
			vmWareBasedInstallStatus = this->PushJobUsingVmWareApis();
		}

		if ((vmWareBasedInstallStatus == NotAttempted ||
			vmWareBasedInstallStatus == FailedWithFallbackError) &&
			m_jobdefinition.UseWMISSHBasedInstall())
		{
			this -> PushJobUsingWMISSHApis();
		}
	}

	void PushJob::PushJobUsingWMISSHApis()
	{
		PushinstallTelemetry pushinstallTelemetry;

		std::string runningStage;
		std::string errMessageToDisplay;//This is a status message and will be sent to CS upon failure.	

		std::string errorCode;
		std::string failureMessage = "";
		std::map<std::string, std::string> placeHolders;

		if (m_jobdefinition.IsInstallationRequest() || m_jobdefinition.IsUninstallRequest()){

			try {

				DebugPrintf(SV_LOG_DEBUG, "Attempting push install using WMI/SSH APIs.\n");
				pushinstallTelemetry.RecordJobStarted();
				m_jobdefinition.SetInstallMode(InstallationMode::WMISSH);

				std::string reachableIpAddress;
				std::string pushServerIp = Host::GetInstance().GetIPAddress();
				std::string pushServerName = Host::GetInstance().GetHostName();
				
				// filter IPV4/FQDN from the remote IP list.
				m_jobdefinition.remoteip = GetSourceMachineIpv4AddressesAndFqdn(m_jobdefinition.remoteip);
				
				// ensure that the username for linux push installation is "root". Otherwise,
				// mobility agent installation will fail since agent only accepts root account
				runningStage = "Validating prerequisites..";
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psName] = pushServerName;
				placeHolders[psIp] = pushServerIp;

				DebugPrintf(SV_LOG_DEBUG, "Running stage : %s \n", runningStage.c_str());

				remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
				if (os_id == os_idx::unix_idx && m_jobdefinition.userName() != "root") {
					errorCode = "EP0963";
					placeHolders[errCode] = errorCode;
					throw ERROR_EXCEPTION
						<< "Non-root user account provided for linux push installation.";
				}

				//get reachable/valid remote machine ip
				runningStage = "verifying connection to remote machine...";

				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psName] = pushServerName;
				placeHolders[psIp] = pushServerIp;

				if (m_jobdefinition.remoteip == std::string()) {
					errorCode = "EP0971";
					placeHolders[errCode] = errorCode;
					// error message would be picked up from SRS xml.
					errMessageToDisplay = "";
					throw ERROR_EXCEPTION << "No IPV4 addresses/FQDN are discovered for the machine.";
				}
				else if (remoteApiLib::windows_idx == m_jobdefinition.os_id){
					errorCode = "EP0865";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0865(placeHolders);
				}
				else{
					errorCode = "EP0866";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0866(placeHolders);
				}

				DebugPrintf(SV_LOG_DEBUG, "Running stage : %s \n", runningStage.c_str());

				getRemoteMachineIp(m_jobdefinition.remoteip, reachableIpAddress);
				m_jobdefinition.remoteip = reachableIpAddress;

				std::string os = m_jobdefinition.getOptionalParam(PI_OSNAME_KEY);
				DebugPrintf(SV_LOG_DEBUG, "OS name received: %s \n", os.c_str());
				if (os == std::string() || os == "null")
				{
					// get os name
					runningStage = "discovering remote machine operating system type...";

					errorCode = "EP0860";
					placeHolders.clear();
					placeHolders[sourceIp] = m_jobdefinition.remoteip;
					placeHolders[sourceName] = m_jobdefinition.vmName;
					placeHolders[psName] = pushServerName;
					placeHolders[psIp] = pushServerIp;
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0860(placeHolders);

					DebugPrintf(SV_LOG_DEBUG, "Running stage : %s \n", runningStage.c_str());

					std::string osname = osName(errorCode, placeHolders, errMessageToDisplay);

					m_jobdefinition.setOptionalParam(PI_OSNAME_KEY, osname);
				}

				if (m_jobdefinition.jobtype == PushJobType::fetchDistroDetails)
				{
					m_jobdefinition.jobStatus = PUSH_JOB_STATUS::INSTALL_JOB_COMPLETED;
					m_proxy->UpdateAgentInstallationStatus(
						m_jobdefinition.jobId(),
						m_jobdefinition.jobStatus,
						runningStage,
						"UNKNOWN-HOST-ID",
						"",
						"",
						PI::PlaceHoldersMap(),
						"",
						m_jobdefinition.getOptionalParam(PI_OSNAME_KEY));
					goto JobExecutionCompletionLabel;
				}

				m_jobdefinition.verifyMandatoryFields();

				//This error message is same for fillMissingFields() & downloadSW()
				errorCode = "EP0851";
				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psIp] = pushServerIp;
				placeHolders[psName] = pushServerName;
				placeHolders[csIp] = m_proxy->csip();
				placeHolders[errCode] = errorCode;
				errMessageToDisplay = getErrorEP0851(placeHolders);
				// this cs call is not requied if cs is filling up all the fields
				runningStage = "fetching job details from Configuration Server...";
				DebugPrintf(SV_LOG_DEBUG, "Running stage : %s \n", runningStage.c_str());
				this->fillMissingFields(failureMessage);

				errorCode = "EP0852";
				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psIp] = pushServerIp;
				placeHolders[psName] = pushServerName;
				placeHolders[csIp] = m_proxy->csip();
				placeHolders[errCode] = errorCode;
				errMessageToDisplay = getErrorEP0852(placeHolders);
				// download require sw
				runningStage = "downloading mobility agent from CS...";
				DebugPrintf(SV_LOG_DEBUG, "Running stage : %s \n", runningStage.c_str());
				std::string uaOrPatch, pushclientSw;
				downloadSW();

				errorCode = "EP0853";
				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psIp] = pushServerIp;
				placeHolders[psName] = pushServerName;
				placeHolders[errCode] = errorCode;
				placeHolders[cspsConfigToolPath] = "<PS install path>\\home\\svsystems\\bin";
				errMessageToDisplay = getErrorEP0853(placeHolders);
				// verify require sw
				runningStage = "verifying signature of mobility agent software...";
				DebugPrintf(SV_LOG_DEBUG, "Running stage : %s \n", runningStage.c_str());
				verifySW();

				// copy push client and ua/patch (for install job) and rest of files to remote machine
				// execute the job on remote machine
				installMobilityService(errorCode, placeHolders, errMessageToDisplay);

				// consume status of the job
				std::string output;
				int exitStatus;
				std::string installerExtendedErrorsJson = "";

				consumeExitStatus(output, exitStatus, installerExtendedErrorsJson);

				// send status and log to cs
				if (m_jobdefinition.IsInstallationRequest()){

					if (output.empty()){
						output += "agent installation returned exit code: " + boost::lexical_cast<std::string>(exitStatus);
						output += "\n";
					}


					if (AGENT_INSTALLATION_SUCCESS == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_COMPLETED;
						runningStage = "agent installation completed\n";
						DebugPrintf(SV_LOG_DEBUG, "%s %s\n", runningStage.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, runningStage, "UNKNOWN-HOST-ID", output);
					}
					else if (AGENT_EXISTS_SAME_VERSION == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_COMPLETED;
						runningStage = "Mobility service is already installed on ";
						runningStage += m_jobdefinition.remoteip;
						runningStage += " and registered with same CS(";
						runningStage += m_proxy->csip();
						runningStage += "). No action required.";
						DebugPrintf(SV_LOG_DEBUG, "%s %s\n", runningStage.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, runningStage, "UNKNOWN-HOST-ID", output);
					}
					else if (AGENT_INSTALL_COMPLETED_BUT_REQUIRES_REBOOT == exitStatus) {
						// initial plan was to introduce new job status for ASR to display this as warning
						// for now, we are looking at treating this also as failure
						//m_jobdefinition.jobStatus = INSTALL_JOB_COMPLETED_WITH_WARNINGS;
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;

						runningStage = "Mobility service installation on ";
						runningStage += m_jobdefinition.remoteip;
						runningStage += "completed with an error. Setup requires you to restart the server before replication can be started.";

						errorCode = "EP0884";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0884(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}

					else if (AGENT_EXISTS_DIFF_VERSION == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0864";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0864(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_EXISTS_REGISTEREDWITH_DIFF_CS == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0867";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0867(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FATAL_ERROR == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0870";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0870(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PREPARE_STAGE_FAILED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0871";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0871(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PREPARE_STATE_FAILED_REBOOTREQUIRED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0872";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0872(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INVALID_CS_IP_PORT_PASSPHRASE == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0873";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[csIp] = m_proxy->csip();
						placeHolders[csPort] = boost::lexical_cast<std::string>(m_proxy->csport());
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0873(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_UNSUPPORTED_OS == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0874";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0874(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_SERVICES_STOP_FAILED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0875";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0875(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_SERVICES_START_FAILED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0876";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0876(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_HOST_ENTRY_NOTFOUND == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0877";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0877(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INSUFFICIENT_DISKSPACE == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0878";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0878(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_CREATE_TEMPDIR_FAILED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0879";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0879(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALLER_FILE_NOT_FOUND == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0880";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[remoteUAPath] = m_jobdefinition.remoteUaPath();
						placeHolders[pushLogPath] = m_jobdefinition.remoteLogPath();
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0880(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PREV_INSTALL_DIR_DELETE_FAILED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0882";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						placeHolders[installPath] = m_jobdefinition.installation_path;
						errMessageToDisplay = getErrorEP0882(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PREV_INSTALL_REBOOT_PENDING == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0883";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0883(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_DOTNET35_MISSING == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0885";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0885(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALLED_REGISTER_FAILED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0886";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						placeHolders[installPath] = m_jobdefinition.installation_path;
						placeHolders[csIp] = m_proxy->csip();
						errMessageToDisplay = getErrorEP0886(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FAILED_WITH_VSS_ISSUE == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0887";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						placeHolders[installPath] = m_jobdefinition.installation_path;
						placeHolders[csIp] = m_proxy->csip();
						errMessageToDisplay = getErrorEP0887(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALLATION_INTERNAL_ERROR == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0888";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						placeHolders[pushLogPath] = pushLogPath;
						errMessageToDisplay = getErrorEP0888(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INVALID_PARAMS == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0889";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0889(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALLER_ALREADY_RUNNING == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0890";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0890(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALLER_OS_MISMATCH == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0891";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0891(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_MISSING_PARAMS == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0892";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0892(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PASSPHRASE_NOT_FOUND == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0894";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						placeHolders[passphrasePath] = m_jobdefinition.remoteConnectionPassPhrasePath();
						errMessageToDisplay = getErrorEP0894(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VX_SERVICE_STOP_FAIL == exitStatus ||
						AGENT_INSTALL_FX_SERVICE_STOP_FAIL == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0895";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0895(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INSUFFICIENT_PRIVILEGE == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0896";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0896(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PREV_INSTALL_NOT_FOUND == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0897";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0897(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_KERNEL_NOT_SUPPORTED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0898";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0898(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_LINUX_VERSION_NOT_SUPPORTED == exitStatus ||
						AGENT_INSTALL_SLES_VERSION_NOT_SUPPORTED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0899";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0899(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_BOOT_DRIVER_NOT_AVAILABLE == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0900";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0900(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FAILED_ON_UNIFIED_SETUP == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0901";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0901(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0902";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0902(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT_LINUX == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0903";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0903(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_CURRENT_KERNEL_NOT_SUPPORTED == exitStatus) {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0904";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0904(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_LIS_COMPONENTS_NOT_AVAILABLE == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0905";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0905(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INSUFFICIENT_SPACE_IN_ROOT == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0906";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0906(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_SPACE_NOT_AVAILABLE == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0907";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[installPath] = m_jobdefinition.installation_path;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0907(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VMWARE_TOOLS_NOT_RUNNING == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0908";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0908(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0909";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0909(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_DRIVER_MANIFEST_INSTALLATION_FAILED == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0910";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0910(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_MASTER_TARGET_EXISTS == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0911";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0911(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PLATFORM_MISMATCH == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0912";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0912(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INVALID_COMMANDLINE_ARGUMENTS == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0913";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0913(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_DRIVER_ENTRY_REGISTRY_UPDATE_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0914";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0914(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PREREQS_FAIL == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());

						if (installerExtendedErrorsJson == "")
						{
							errorCode = "EP2003";
							placeHolders[errCode] = errorCode;
							errMessageToDisplay = getErrorEP2003(placeHolders);
						}
						else
						{
							errorCode = "EP0915";
							placeHolders[errCode] = errorCode;
							errMessageToDisplay = getErrorEP0915(placeHolders);

							ofstream filehandle(
								m_jobdefinition.LocalInstallerExtendedErrorsJsonPath(),
								ios::out | ios::binary);

							if (filehandle.good())
							{
								filehandle.write(installerExtendedErrorsJson.c_str(), installerExtendedErrorsJson.length());
								bool rv = filehandle.good();
								if (!rv)
								{
									DebugPrintf(SV_LOG_ERROR, "Failed to write to installer pre-req json file %s.\n", m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str());
								}
								filehandle.close();
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR, "Failed to create installer pre-req json file %s with error %s.\n", m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str(), strerror(errno));
							}
						}

						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders, installerExtendedErrorsJson);
					}
					else if (AGENT_SETUP_FAILED_WITH_ERRORS == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());

						if (installerExtendedErrorsJson == "")
						{
							errorCode = "EP2004";
							placeHolders[errCode] = errorCode;
							errMessageToDisplay = getErrorEP2004(placeHolders);
						}
						else
						{
							errorCode = "EP0964";
							placeHolders[errCode] = errorCode;
							errMessageToDisplay = "";

							ofstream filehandle(
								m_jobdefinition.LocalInstallerExtendedErrorsJsonPath(),
								ios::out | ios::binary);

							if (filehandle.good())
							{
								filehandle.write(installerExtendedErrorsJson.c_str(), installerExtendedErrorsJson.length());
								bool rv = filehandle.good();
								if (!rv)
								{
									DebugPrintf(SV_LOG_ERROR, "Failed to write to installer VSS installation error json file %s with error %s.\n", m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str(), strerror(errno));
								}
								filehandle.close();
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR, "Failed to create installer VSS installation error json file %s with error %s.\n", m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str(), strerror(errno));
							}
						}

						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders, installerExtendedErrorsJson);
					}
					else if (AGENT_SETUP_COMPLETED_WITH_WARNINGS == exitStatus) {

						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());

						if (installerExtendedErrorsJson == "")
						{
							errorCode = "EP2004";
							placeHolders[errCode] = errorCode;
							errMessageToDisplay = getErrorEP2004(placeHolders);
						}
						else
						{
							errorCode = "EP0965";
							placeHolders[errCode] = errorCode;
							errMessageToDisplay = "";
							ofstream filehandle(
								m_jobdefinition.LocalInstallerExtendedErrorsJsonPath(),
								ios::out | ios::binary);

							if (filehandle.good())
							{
								filehandle.write(installerExtendedErrorsJson.c_str(), installerExtendedErrorsJson.length());
								bool rv = filehandle.good();
								if (!rv)
								{
									DebugPrintf(SV_LOG_ERROR, "Failed to write to installer VSS installation error json file %s with error %s.\n", m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str(), strerror(errno));
								}
								filehandle.close();
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR, "Failed to create installer VSS installation error json file %s with error %s.\n", m_jobdefinition.LocalInstallerExtendedErrorsJsonPath().c_str(), strerror(errno));
							}
						}

						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders, installerExtendedErrorsJson);
					}
					else if (AGENT_INSTALL_DISABLE_SERVICE_FAIL == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0916";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_UPGRADE_PLATFORM_CHANGE_NOT_SUPPORTED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0917";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_HOST_NAME_NOT_FOUND == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0918";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FAILED_OUT_OF_MEMORY == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0967";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INSTALLATION_DIRECTORY_ABS_PATH_NEEDED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0919";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_PARTITION_SPACE_NOT_AVAILABLE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0920";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[installPath] = m_jobdefinition.installation_path;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_OPERATING_SYSTEM_NOT_SUPPORTED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0921";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_THIRDPARTY_NOTICES_NOT_AVAILABLE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0922";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_LOCALE_NOT_AVAILABLE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0923";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_RPM_COMMAND_NOT_AVAILABLE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0924";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_CONFIG_DIRECTORY_CREATION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0925";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_CONF_FILE_COPY_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0926";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_RPM_INSTALLATION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0927";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_TEMP_FOLDER_CREATION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0928";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_DRIVER_INSTALLATION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0929";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_CACHE_FOLDER_CREATION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0930";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_SNAPSHOT_DRIVER_IN_USE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0931";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_SNAPSHOT_DRIVER_UNLOAD_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0932";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INVALID_VX_INSTALLATION_OPTIONS == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0933";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_MUTEX_ACQUISITION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0934";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_LINUX_INSUFFICIENT_DISK_SPACE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0935";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_CERT_GEN_FAIL == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0936";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_DATABASE_LOCKED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0938";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_ALREADY_EXISTS == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0939";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_MARKED_FOR_DELETION == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0940";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_CSSCRIPT_ACCESS_DENIED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0941";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_PATH_NOT_FOUND == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0942";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_VSS_INSTALLATION_FAILED_OUT_OF_MEMORY == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0943";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_MSI_INSTALLATION_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0944";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FAILED_TO_SET_SERVICE_STARTUP_TYPE == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0945";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INITRD_BACKUP_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0946";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INITRD_RESTORE_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0947";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_INITRD_IMAGE_UPDATE_FAILED == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0948";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FAILED_TO_CREATE_SYMLINKS == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0953";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (AGENT_INSTALL_FAILED_TO_INSTALL_AND_CONFIGURE_SERVICES == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0954";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (PI_BIOS_ID_MISMATCH == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";

						placeHolders.clear();
						if (m_jobdefinition.vmType == VM_TYPE::VMWARE)
						{
							errorCode = "EP0966";
							placeHolders[vCenterIp] = m_jobdefinition.vCenterIP;
						}
						else
						{
							errorCode = "EP0970";
						}
						
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else if (PI_FQDN_MISMATCH == exitStatus)
					{
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP2005";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = "";
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
					else {
						m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
						runningStage = "agent installation failed\n";
						errorCode = "EP0870";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0870(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}
				}
				else {

					if (output.empty()){
						output += "\n";
					}

					if (0 == exitStatus) {
						m_jobdefinition.jobStatus = UNINSTALL_JOB_COMPLETED;
						runningStage = "agent un-installation step completed\n";
						DebugPrintf(SV_LOG_DEBUG, "%s %s\n", runningStage.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, runningStage, "UNKNOWN-HOST-ID", output);
					}
					else {
						m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;
						runningStage = "agent un-installation step failed\n";
						errorCode = "EP0868";
						placeHolders.clear();
						placeHolders[sourceIp] = m_jobdefinition.remoteip;
						placeHolders[sourceName] = m_jobdefinition.vmName;
						placeHolders[errCode] = errorCode;
						errMessageToDisplay = getErrorEP0868(placeHolders);
						DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStage.c_str(), errMessageToDisplay.c_str(), output.c_str());
						m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", output, errorCode, placeHolders);
					}

				}

			}
			catch (MobilityAgentNotFoundException & mae) {

				errorCode = "EP0968";
				placeHolders[errCode] = errorCode;
				std::string runningStageFailed = runningStage + "failed.";
				m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
				std::string log = "agent installation failed\n error: \n";
				log += mae.what();
				log += "\n";
				DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
				if ((enum remoteApiLib::os_idx)m_jobdefinition.os_id == remoteApiLib::os_idx::windows_idx)
				{
					placeHolders[operatingSystemName] = "Windows";
				}
				else
				{
					placeHolders[operatingSystemName] = m_jobdefinition.getOptionalParam(PI_OSNAME_KEY);
				}
				m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
			}
			catch (PushClientNotFoundException & pce) {

				errorCode = "EP0969";
				placeHolders[errCode] = errorCode;
				std::string runningStageFailed = runningStage + "failed.";
				m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
				std::string log = "agent installation failed\n error: \n";
				log += pce.what();
				log += "\n";
				DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
				if ((enum remoteApiLib::os_idx)m_jobdefinition.os_id == remoteApiLib::os_idx::windows_idx)
				{
					placeHolders[operatingSystemName] = "Windows";
				}
				else
				{
					placeHolders[operatingSystemName] = m_jobdefinition.getOptionalParam(PI_OSNAME_KEY);
				}
				m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
			}
			catch (NonRetryableErrorException & nre){

				std::string runningStageFailed = runningStage + "failed.";
				if (m_jobdefinition.IsInstallationRequest()){
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
					std::string log = "agent installation failed\n error: \n";
					log += nre.what();
					log += "\n";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
				else {
					m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;
					std::string log = "agent un-installation failed\n error: \n";
					log += nre.what();
					log += "\n";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
			}
			catch (CredentialErrorException & ce){

				std::string runningStageFailed = runningStage + "failed.";
				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				if (remoteApiLib::windows_idx == m_jobdefinition.os_id){
					errorCode = "EP0858";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0858(placeHolders);
				}
				else{
					errorCode = "EP0859";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0859(placeHolders);
				}
				if (m_jobdefinition.IsInstallationRequest()){
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;
					std::string log = "agent installation failed\n error: \n";
					log += ce.what();
					log += "\n";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
				else {
					m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;
					std::string log = "agent un-installation failed\n error: \n";
					log += ce.what();
					log += "\n";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
			}
			catch (WmiErrorException & we) {

				std::string runningStageFailed = runningStage + "failed.";

				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[csIp] = m_proxy->csip();
				placeHolders[psIp] = m_config->PushServerIp();
				placeHolders[psName] = m_config->PushServerName();

				if (m_jobdefinition.IsInstallationRequest()) {
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;

					switch (we.getWmiErrorCode())
					{
					case WmiErrorCode::AccessDenied:
						errorCode = "EP0955";
						break;
					case WmiErrorCode::DomainTrustRelationshipFailed:
						errorCode = "EP0956";
						break;
					case WmiErrorCode::LoginAccountDisabled:
						errorCode = "EP0957";
						break;
					case WmiErrorCode::LoginAccountLockedOut:
						errorCode = "EP0958";
						break;
					case WmiErrorCode::LogonServersNotAvailable:
						errorCode = "EP0959";
						break;
					case WmiErrorCode::LogonServiceNotStarted:
						errorCode = "EP0960";
						break;
					case WmiErrorCode::NetworkNotFound:
						errorCode = "EP0961";
						break;
					case WmiErrorCode::SpaceNotAvailableOnTarget:
						errorCode = "EP0962";
						break;
					default:
						break;
					}

					placeHolders[errCode] = errorCode;
					std::string log = "agent installation failed\n error: \n";
					log += we.what();
					log += "\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
				else {
					m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;

					std::string log = "agent un-installation failed\n error: \n";
					log += we.what();
					log += "\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
			}
			catch (RemoteApiErrorException & remoteApiErrorException) {
				std::string runningStageFailed = runningStage + "failed.";

				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[csIp] = m_proxy->csip();
				placeHolders[psIp] = m_config->PushServerIp();
				placeHolders[psName] = m_config->PushServerName();

				if (m_jobdefinition.IsInstallationRequest()) {
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;

					switch (remoteApiErrorException.getRemoteApiErrorCode()) {
					case RemoteApiErrorCode::StorageNotAvailableOnTarget:
						errorCode = "EP0972";

						break;
					case RemoteApiErrorCode::CopyFailed:
						if (remoteApiLib::windows_idx == m_jobdefinition.os_id){
							errorCode = "EP0856";
						}
						else{
							errorCode = "EP0857";
						}
						break;

					case RemoteApiErrorCode::RunFailed:
						if (remoteApiLib::windows_idx == m_jobdefinition.os_id) {
							errorCode = "EP0854";
						}
						else {
							errorCode = "EP0855";
						}
						break;

					default:
						DebugPrintf(SV_LOG_ERROR, "Un-handled remote api error code encountered: %s\n", remoteApiErrorException.getRemoteApiErrorCode());
						break;
					}
				}

				placeHolders[errCode] = errorCode;
				std::string log = "Execution in remote api failed\n error: \n";
				log += remoteApiErrorException.what();
				log += "\n";
				if (!failureMessage.empty())
					errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
				DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
				m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
			}
			catch (ErrorException & ee){

				std::string runningStageFailed = runningStage + "failed.";

				if (m_jobdefinition.IsInstallationRequest()){
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;

					std::string log = "agent installation failed\n error: \n";
					log += ee.what();
					log += "\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
				else {
					m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;

					std::string log = "agent un-installation failed\n error: \n";
					log += ee.what();
					log += "\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
			}
			catch (std::exception & e){

				std::string runningStageFailed = runningStage + "failed.";

				if (m_jobdefinition.IsInstallationRequest()){
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;

					std::string log = "agent installation failed\n error: \n";
					log += e.what();
					log += "\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
				else {

					m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;

					std::string log = "agent un-installation failed\n error: \n";
					log += e.what();
					log += "\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
			}
			catch (...){

				std::string runningStageFailed = runningStage + "failed.";

				if (m_jobdefinition.IsInstallationRequest()){
					m_jobdefinition.jobStatus = INSTALL_JOB_FAILED;

					std::string log = "agent installation failed with unhandled error\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
				else {
					m_jobdefinition.jobStatus = UNINSTALL_JOB_FAILED;

					std::string log = "agent un-installation failed with unhandled error\n";
					if (!failureMessage.empty())
						errMessageToDisplay += "\n" + failureMessage + " Please resolve issues in the job log and retry the operation.";
					DebugPrintf(SV_LOG_ERROR, "%s\n%s\n%s\n", runningStageFailed.c_str(), errMessageToDisplay.c_str(), log.c_str());
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, errMessageToDisplay, "UNKNOWN-HOST-ID", log, errorCode, placeHolders);
				}
			}

		JobExecutionCompletionLabel:
			if (m_jobdefinition.IsJobSucceeded())
			{
				pushinstallTelemetry.RecordJobSucceeded();
			}
			else
			{
				PushInstallException pushinstallException(
					errorCode,
					GetErrorNameForErrorcode(errorCode),
					errMessageToDisplay,
					placeHolders);
				pushinstallTelemetry.RecordJobFailed(pushinstallException);
			}

			PullAgentTelemetryFiles();
			pushinstallTelemetry.SendTelemetryData(m_jobdefinition);
		}
		else
		{
			// throw exception instead ???
			DebugPrintf(SV_LOG_ERROR, "Thread Received non pending items for ip: %s, job id: %s \n", m_jobdefinition.remoteip.c_str(), m_jobdefinition.jobid.c_str());
			return;
		}
	}

	std::string PushJob::osName(std::string &errorCode, std::map<std::string, std::string> &placeHolders, std::string &errMessageToDisplay)
	{
		int attempt = 1;
		bool done = false;
		std::string val;
		std::string build_os;
		std::string actual_os;
		int supported;

		while (!done)
		{

			try {

				DebugPrintf(SV_LOG_DEBUG, "Attempt:%d identifying remote machine %s\n", attempt, m_jobdefinition.remoteip.c_str());

				remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
				RemoteConnection remoteConnection(remoteApiLib::platform(os_id),
					m_jobdefinition.remoteip,
					m_jobdefinition.userName(),
					m_jobdefinition.password,
					m_jobdefinition.remoteFolder());

				val = remoteConnection.impl()->osName(m_config->osScript(), m_jobdefinition.remoteFolder());
				if (val.empty()){
					throw ERROR_EXCEPTION << "OS version of remote machine is not supported." << m_jobdefinition.remoteip;
				}
				if (os_id != remoteApiLib::windows_idx) {
					std::string::size_type osx = val.find_first_of(":");
					if (osx == std::string::npos){
						throw NON_RETRYABLE_ERROR_EXCEPTION << "failed to identify remote machine return value " << val.c_str();
					}

					build_os = val.substr(0, osx);
					if (build_os.empty()){
						throw NON_RETRYABLE_ERROR_EXCEPTION << "failed to identify remote machine return value " << val.c_str();
					}
					std::string remaining = val.substr(osx + 1);
					boost::algorithm::trim(remaining);

					osx = remaining.find_first_of(":");
					if (osx == std::string::npos){
						throw NON_RETRYABLE_ERROR_EXCEPTION << "failed to identify remote machine return value " << val.c_str();
					}

					actual_os = remaining.substr(0, osx);
					remaining = remaining.substr(osx + 1);
					boost::algorithm::trim(remaining);

					supported = boost::lexical_cast<int>(remaining);
				}
				else {
					build_os = val;
					actual_os = "Windows";
					supported = 0;
				}

				if (supported == 1)
				{
					std::string pushServerIp = Host::GetInstance().GetIPAddress();
					std::string pushServerName = Host::GetInstance().GetHostName();

					errorCode = "EP0881";
					placeHolders.clear();
					placeHolders[sourceIp] = m_jobdefinition.remoteip;
					placeHolders[sourceName] = m_jobdefinition.vmName;
					placeHolders[sourceOs] = actual_os;
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0881(placeHolders);

					DebugPrintf(SV_LOG_ERROR, " unsupported version : %d\n", supported);
					throw NON_RETRYABLE_ERROR_EXCEPTION << "unsupported linux platform " << actual_os.c_str();					
				}
				done = true;

			}
			catch (NonRetryableErrorException &nre){ //No retries
				throw;
			}
			catch (CredentialErrorException){ //No retries for credential errors
				throw;
			}
			catch (RemoteApiErrorException &remoteApiErrorException) {
				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d identifying remote machine %s failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), remoteApiErrorException.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (ErrorException & ee){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d identifying remote machine %s failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), ee.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (std::exception &e){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d identifying remote machine %s failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), e.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (...) {

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d identifying remote machine %s failed with unhandled exception\n", attempt, m_jobdefinition.remoteip.c_str());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
		}

		return build_os;
	}

	void PushJob::downloadSW()
	{
		std::string uaOrPatchLocalPath, pushclientLocalPath;

		std::string osname = m_jobdefinition.osname();

		if ((m_jobdefinition.jobtype == pushFreshInstall) || (m_jobdefinition.jobtype == pushUpgrade) || (m_jobdefinition.jobtype == pushConfigure)) {
			uaOrPatchLocalPath = m_swmgr->downloadUA(osname, m_jobdefinition.UAdownloadLocation, false);
		}
		else  if (m_jobdefinition.jobtype == pushPatchInstall){
			uaOrPatchLocalPath = m_swmgr->downloadPatch(osname, m_jobdefinition.UAdownloadLocation, false);
		}

		pushclientLocalPath = m_swmgr->downloadPushClient(osname, m_jobdefinition.pushclientDownloadLocation, false);
		m_jobdefinition.setOptionalParam(PI_UAORPATCHLOCALPATH_KEY, uaOrPatchLocalPath);
		m_jobdefinition.setOptionalParam(PI_PUSHCLIENTLOCALPATH_KEY, pushclientLocalPath);
	}

	void PushJob::verifySW()
	{

		if (!m_config->SignVerificationEnabled())
			return;

		std::string osname = m_jobdefinition.osname();
		if ((m_jobdefinition.jobtype == pushFreshInstall) || (m_jobdefinition.jobtype == pushUpgrade) || (m_jobdefinition.jobtype == pushConfigure)) {
			m_swmgr->verifyUA(osname, m_jobdefinition.UAdownloadLocation);
		}
		else  if (m_jobdefinition.jobtype == pushPatchInstall){
			m_swmgr->verifyPatch(osname, m_jobdefinition.UAdownloadLocation);
		}

		m_swmgr->verifyPushClient(osname, m_jobdefinition.pushclientDownloadLocation);
	}

	std::string PushJob::preparePushClientJobSpec()
	{
		std::string jobSpec;

		remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
		jobSpec += "# A line will be identified as comment if the line begins with a \'#\'";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += "[" + PI::SECTION_VER + "]";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::SPEC_VERSION;
		jobSpec += " = 1.0";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += "[" + PI::SECTION_GLOBAL + "]";
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory.As soon as the push client starts, it writes pid to the file passed. If the file already exists, it is truncated.";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::PID_FILEPATH;
		jobSpec += " = ";
		jobSpec += m_jobdefinition.remotePidPath();
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory.The text output from the push client / installer is written to the file passed.If the file already exists, it is truncated.";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::STD_OUT_FILEPATH;
		jobSpec += " = ";
		jobSpec += m_jobdefinition.remoteLogPath();
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory.The errors from the push client / installer is written to the file passed.If the file already exists, it is truncated.";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::STD_ERR_FILEPATH;
		jobSpec += "= ";
		jobSpec += m_jobdefinition.remoteErrorLogPath();
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "#Mandatory, this is where the job exit status is written";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::EXIT_STATUS_FILEPATH;
		jobSpec += " = ";
		jobSpec += m_jobdefinition.remoteExitStatusPath();
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory.cs ip.";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::CS_IP;
		jobSpec += " = ";
		jobSpec += m_proxy->csip();
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory.cs port";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::CS_PORT;
		jobSpec += " = ";
		jobSpec += boost::lexical_cast<std::string>(m_proxy->csport());
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "#Mandatory, secure mode";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::SECURE_COMMUNICATION;
		jobSpec += " = ";
		jobSpec += boost::lexical_cast<std::string>(m_config->secure());
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "[" + SECTION_JOBDEFINITION + "]";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory entry, This uniquely identifies the push install job";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::JOB_ID;
		jobSpec += " = ";
		jobSpec += m_jobdefinition.jobId();
		jobSpec += remoteApiLib::NewLine(os_id);

		jobSpec += "# Mandatory entry, job type";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::JOB_TYPE;
		jobSpec += " = ";
		jobSpec += m_jobdefinition.jobType();
		jobSpec += remoteApiLib::NewLine(os_id);

		if (m_config->stackType() == StackType::RCM)
		{
			jobSpec += "# Not a Mandatory entry, bios id of the source";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::BIOS_ID;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.sourceMachineBiosId;
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# Not a Mandatory entry, host name of the source";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::FQDN;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.sourceMachineFqdn;
			jobSpec += remoteApiLib::NewLine(os_id);
		}

		jobSpec += "# Mandatory entry, command to execute for install/uninstall";
		jobSpec += remoteApiLib::NewLine(os_id);
		jobSpec += PI::JOB_COMMAND;
		jobSpec += " = ";
		jobSpec += m_jobdefinition.remoteCommandToRun();
		jobSpec += remoteApiLib::NewLine(os_id);

		if (m_jobdefinition.IsInstallationRequest()) {

			jobSpec += "# Mandatory entry for installation, if signature verification is enabled";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::INSTALLER_PATH;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.remoteUaPath();
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# verify signature";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::VERIFY_SIGNATURE;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.SignVerificationEnabledOnRemoteMachine();
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# Mandatory entry, installation path";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::INSTALLATION_PATH;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.remoteInstallationPath();
			jobSpec += remoteApiLib::NewLine(os_id);

			if (m_jobdefinition.jobtype == PushJobType::pushConfigure)
			{
				jobSpec += "# Skip installation. Set in case of appliance switch";
				jobSpec += remoteApiLib::NewLine(os_id);
				jobSpec += PI::SKIP_INSTALL;
				jobSpec += " = ";
				jobSpec += "true";
				jobSpec += remoteApiLib::NewLine(os_id);
			}

			//jobSpec += "# reboot machine after job is complete";
			//jobSpec += remoteApiLib::NewLine(os_id);
			//jobSpec += "reboot = ";
			//jobSpec += m_jobdefinition.isrebootRemoteMachine();
			//jobSpec += remoteApiLib::NewLine(os_id);

			if (m_jobdefinition.m_clientAuthenticationType == ClientAuthenticationType::Passphrase)
			{
				jobSpec += "# Mandatory entry, connection passphrase file";
				jobSpec += remoteApiLib::NewLine(os_id);
				jobSpec += PI::PASSPHRASE_FILEPATH;
				jobSpec += " = ";
				jobSpec += m_jobdefinition.remoteConnectionPassPhrasePath();
				jobSpec += remoteApiLib::NewLine(os_id);
			}
			else if (m_jobdefinition.m_clientAuthenticationType == ClientAuthenticationType::Certificate)
			{
				jobSpec += "# Mandatory entry, certificate config file";
				jobSpec += remoteApiLib::NewLine(os_id);
				jobSpec += PI::CERTCONFIG_FILEPATH;
				jobSpec += " = ";
				jobSpec += m_jobdefinition.remoteMobilityAgentConfigurationFilePath();
				jobSpec += remoteApiLib::NewLine(os_id);
			}

			// Construct postcommand only in case of fresh install scenario.
			if (m_jobdefinition.jobtype == pushFreshInstall || m_jobdefinition.jobtype == pushConfigure) {
				jobSpec += "# command to execute for configuration";
				jobSpec += remoteApiLib::NewLine(os_id);
				jobSpec += PI::JOB_POST_COMMAND;
				jobSpec += " = ";
				jobSpec += m_jobdefinition.remoteCommandToConfigure();
				jobSpec += remoteApiLib::NewLine(os_id);
			}
		}
		
		if (m_jobdefinition.IsInstallationRequest() && (m_jobdefinition.os_id == remoteApiLib::windows_idx)) {

			jobSpec += "# command to execute for extraction";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::JOB_PRE_COMMAND;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.remoteCommandToExtract();
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# domain, currently windows specific";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::DOMAIN_NAME;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.domainName();
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# username, currently windows specific";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::USER_NAME;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.userName();
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# encryption password file, currently windows specific";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::ENCRYPTED_PASSWORD_FILE;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.remoteEncryptionPasswordFile();
			jobSpec += remoteApiLib::NewLine(os_id);

			jobSpec += "# encryption key file, currently windows specific";
			jobSpec += remoteApiLib::NewLine(os_id);
			jobSpec += PI::ENCRYPTION_KEY_FILE;
			jobSpec += " = ";
			jobSpec += m_jobdefinition.remoteEncryptionKeyFile();
			jobSpec += remoteApiLib::NewLine(os_id);

		}

		jobSpec += remoteApiLib::NewLine(os_id);
		return jobSpec;
	}


	void PushJob::fillMissingFields(std::string &failureMessage)
	{
		PUSH_INSTALL_JOB jobdetail;

		std::string osname = m_jobdefinition.osname();

		try{
			jobdetail = m_proxy->GetInstallDetails(m_jobdefinition.jobid, osname);
		}
		catch (ContextualException & ce) {
			failureMessage = "Failed to get installation details from configuration server.";
			throw ERROR_EXCEPTION << "cs api call GetInstallDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
				<< " with error " << ce.what();
		}
		catch (ErrorException & ee)	{
			failureMessage = "Failed to get installation details from configuration server.";
			throw ERROR_EXCEPTION << "cs api call GetInstallDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
				<< " with error " << ee.what();
		}
		catch (std::exception & e) {
			failureMessage = "Failed to get installation details from configuration server.";
			throw ERROR_EXCEPTION << "cs api call GetInstallDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
				<< " with error " << e.what();
		}
		catch (...){
			failureMessage = "Failed to get installation details from configuration server.";
			throw ERROR_EXCEPTION << "cs api call GetInstallDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
				<< " with unhandled error ";
		}

		INSTALL_TASK_LIST::iterator iter = jobdetail.m_installTasks.begin();

		if (iter == jobdetail.m_installTasks.end()){
			failureMessage = "Configuration server returned empty installation details";
			throw ERROR_EXCEPTION << "internal error, cs api GetInstallDetails returned empty set for jobid " << m_jobdefinition.jobid << " os : " << osname;
		}

		if (jobdetail.m_installTasks.size() > 1) {
			failureMessage = "Configuration server returned more than one installation details for single job";
			throw ERROR_EXCEPTION << "internal error, cs api GetInstallDetails returned more than one install request for jobid " << m_jobdefinition.jobid << " os : " << osname;
		}

		// todo: remove this block once cs fills in the jobtype
		if (m_jobdefinition.IsUninstallRequest()) {
			m_jobdefinition.jobtype = pushUninstall;
		}
		else {
			INSTALL_TYPE type_of_install = (*iter).type_of_install;

			if (m_jobdefinition.jobtype != PushJobType::pushConfigure)
			{
				if (type_of_install == FRESH_INSTALL) {
					m_jobdefinition.jobtype = pushFreshInstall;
				}
				else if (type_of_install == UPGRADE_INSTALL) {
					m_jobdefinition.jobtype = pushUpgrade;
				}
				else if (type_of_install == PATCH_INSTALL) {
					m_jobdefinition.jobtype = pushPatchInstall;
				}
				else {
					failureMessage = "Configuration server returned invalid installation type";
					throw ERROR_EXCEPTION << "internal error, cs api GetInstallDetails returned invalid value (type_of_install = " << type_of_install
						<< " ) for jobid " << m_jobdefinition.jobid << " os : " << osname;
				}
			}
		}

		if (m_jobdefinition.jobtype == pushFreshInstall && m_jobdefinition.installation_path.empty()){
			failureMessage = "Configuration server returned empty for installation path";
			throw ERROR_EXCEPTION << "internal_error, cs api returned empty for installation folder for jobid " << m_jobdefinition.jobid << " os : " << osname;
		}


		if (m_jobdefinition.m_commandLineOptions.empty()){
			m_jobdefinition.m_commandLineOptions = (*iter).m_commandLineOptions;
		}

		if (m_jobdefinition.IsInstallationRequest() && m_jobdefinition.UAdownloadLocation.empty()) {
			try {

				std::string uaOrPatchLocalPath;
				uaOrPatchLocalPath = m_swmgr->defaultUAPath(osname);
				if (uaOrPatchLocalPath.empty()) {
					m_jobdefinition.UAdownloadLocation = m_proxy->GetBuildPath(m_jobdefinition.jobid, osname);
				}
			}
			catch (MobilityAgentNotFoundException) {
				throw;
			}
			catch (ContextualException & ce) {
				failureMessage = "Failed to get mobility agent download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetBuildPath failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with error " << ce.what();
			}
			catch (ErrorException & ee)	{
				failureMessage = "Failed to get mobility agent download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetBuildPath failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with error " << ee.what();
			}
			catch (std::exception & e) {
				failureMessage = "Failed to get mobility agent download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetBuildPath failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with error " << e.what();
			}
			catch (...){
				failureMessage = "Failed to get mobility agent download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetBuildPath failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with unhandled error ";
			}
		}

		if (m_jobdefinition.pushclientDownloadLocation.empty()) {

			try {

				std::string pushclientLocalPath;
				//  skip cs call if the push client is already available in the repository
				pushclientLocalPath = m_swmgr->defaultPushClientPath(osname);

				if (pushclientLocalPath.empty()) {
					m_jobdefinition.pushclientDownloadLocation = m_proxy->GetPushClientDetails(m_jobdefinition.jobid, osname);
				}

			}
			catch (PushClientNotFoundException) {
				throw;
			}
			catch (ContextualException & ce) {
				failureMessage = "Failed to get push client download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetPushClientDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with error " << ce.what();
			}
			catch (ErrorException & ee)	{
				failureMessage = "Failed to get push client download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetPushClientDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with error " << ee.what();
			}
			catch (std::exception & e) {
				failureMessage = "Failed to get push client download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetPushClientDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with error " << e.what();
			}
			catch (...){
				failureMessage = "Failed to get push client download path from configuration server.";
				throw ERROR_EXCEPTION << "cs api call GetPushClientDetails failed for jobid " << m_jobdefinition.jobid << " os : " << osname
					<< " with unhandled error ";
			}
		}

	}

	// why am i generating this script in code rather than packaging it ?
	std::string PushJob::preparePushJobScriptForUnix()
	{
		std::string script;

		std::string os = m_jobdefinition.osname();
		std::string remoteLog = m_jobdefinition.remoteLogPath();
		std::string remoteErrorLog = m_jobdefinition.remoteErrorLogPath();
		std::string exitStatusFile = m_jobdefinition.remoteExitStatusPath();

		remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
		script = "#!/bin/sh";
		script += remoteApiLib::NewLine(os_id);

		script += "#untar and invoke push client for installation";
		script += remoteApiLib::NewLine(os_id);

		script += ". /etc/profile";
		script += remoteApiLib::NewLine(os_id);

		script += "cd ";
		script += m_jobdefinition.remoteFolder();
		script += remoteApiLib::NewLine(os_id);

		script += "echo $$ > ";
		script += m_jobdefinition.remotePidPath();
		script += remoteApiLib::NewLine(os_id);

		script += "echo >";
		script += remoteLog;
		script += remoteApiLib::NewLine(os_id);

		script += "echo >";
		script += remoteErrorLog;
		script += remoteApiLib::NewLine(os_id);

		script += "echo >";
		script += exitStatusFile;
		script += remoteApiLib::NewLine(os_id);

		script += "echo \"extracting push client binary\" >>";
		script += remoteLog;
		script += remoteApiLib::NewLine(os_id);

		if (os.find("Solaris") != std::string::npos ||
			os.find("HP-UX") != std::string::npos ||
			os.find("AIX") != std::string::npos) {

			/*
			script += "gunzip ";
			script += m_jobdefinition.remotePushClientPath();
			script += " >> " + remoteLog;
			script += " 2>> " + remoteErrorLog;
			script += remoteApiLib::NewLine(os_id);

			script += "if [ $? -eq 0 ]; then";
			script += remoteApiLib::NewLine(os_id);
			*/

			script += " tar -xvf ";
			script += m_jobdefinition.remotePushClientPath();
			script += " >> " + remoteLog;
			script += " 2>> " + remoteErrorLog;
			script += remoteApiLib::NewLine(os_id);

			script += "if [ $? -eq 0 ]; then";
			script += remoteApiLib::NewLine(os_id);

			script += "echo \"push client binary extraction completed\" >>";
			script += remoteLog;
			script += remoteApiLib::NewLine(os_id);

			script += "else";
			script += remoteApiLib::NewLine(os_id);

			script += "rc=$?";
			script += remoteApiLib::NewLine(os_id);
			script += "echo \"push client binary extraction failed with exit status $rc \"  >> ";
			script += remoteErrorLog;
			script += remoteApiLib::NewLine(os_id);

			script += "echo ";
			script += m_jobdefinition.jobId();
			script += ":0:$rc:0";
			script += " > ";
			script += exitStatusFile;
			script += remoteApiLib::NewLine(os_id);

			script += "exit 1";
			script += remoteApiLib::NewLine(os_id);

			script += "fi";
			script += remoteApiLib::NewLine(os_id);

			/*
			script += "else";
			script += remoteApiLib::NewLine(os_id);

			script += "rc=$?";
			script += remoteApiLib::NewLine(os_id);
			script += "echo \"push client binary extraction (gunzip) failed with exit status $rc\" >>";
			script += remoteErrorLog;
			script += remoteApiLib::NewLine(os_id);

			script += "echo ";
			script += m_jobdefinition.jobId();
			script += ":0:$rc:0";
			script += " > ";
			script += exitStatusFile;
			script += remoteApiLib::NewLine(os_id);

			script += "exit 1";
			script += remoteApiLib::NewLine(os_id);

			script += "fi";
			*/
			script += remoteApiLib::NewLine(os_id);

		}
		else {

			script += " tar -xvf ";
			script += m_jobdefinition.remotePushClientPath();
			script += " >> " + remoteLog;
			script += " 2>> " + remoteErrorLog;
			script += remoteApiLib::NewLine(os_id);

			script += "if [ $? -eq 0 ]; then";
			script += remoteApiLib::NewLine(os_id);

			script += "echo \"push client binary extraction completed\" >>";
			script += remoteLog;
			script += remoteApiLib::NewLine(os_id);

			script += "else";
			script += remoteApiLib::NewLine(os_id);

			script += "rc=$?";
			script += remoteApiLib::NewLine(os_id);
			script += "echo \"push client binary extraction failed with exit status $rc \"  >> ";
			script += remoteErrorLog;
			script += remoteApiLib::NewLine(os_id);

			script += "echo ";
			script += m_jobdefinition.jobId();
			script += ":0:$rc:0";
			script += " > ";
			script += exitStatusFile;
			script += remoteApiLib::NewLine(os_id);

			script += "exit 1";
			script += remoteApiLib::NewLine(os_id);

			script += "fi";
			script += remoteApiLib::NewLine(os_id);
		}

		if (m_jobdefinition.jobtype == PushJobType::pushFreshInstall || m_jobdefinition.jobtype == PushJobType::pushUpgrade)
		{

			script += "echo \"extracting unified agent binary\" >";
			script += remoteLog;
			script += remoteApiLib::NewLine(os_id);

			if (os.find("Solaris") != std::string::npos ||
				os.find("HP-UX") != std::string::npos ||
				os.find("AIX") != std::string::npos) {
				/*
				script += "gunzip ";
				script += m_jobdefinition.remoteUaPath();
				script += " >> " + remoteLog;
				script += " 2>> " + remoteErrorLog;
				script += remoteApiLib::NewLine(os_id);

				script += "if [ $? -eq 0 ]; then";
				script += remoteApiLib::NewLine(os_id);
				*/
				script += " tar -xvf ";
				script += m_jobdefinition.remoteUaPath();
				script += " >> " + remoteLog;
				script += " 2>> " + remoteErrorLog;
				script += remoteApiLib::NewLine(os_id);

				script += "if [ $? -eq 0 ]; then";
				script += remoteApiLib::NewLine(os_id);

				script += "echo \"unified agent binary extraction completed\" >>";
				script += remoteLog;
				script += remoteApiLib::NewLine(os_id);

				script += "else";
				script += remoteApiLib::NewLine(os_id);

				script += "rc=$?";
				script += remoteApiLib::NewLine(os_id);
				script += "echo \"unified agent binary extraction failed with exit status $rc \"  >> ";
				script += remoteErrorLog;
				script += remoteApiLib::NewLine(os_id);

				script += "echo ";
				script += m_jobdefinition.jobId();
				script += ":0:$rc:0";
				script += " > ";
				script += exitStatusFile;
				script += remoteApiLib::NewLine(os_id);

				script += "exit 1";
				script += remoteApiLib::NewLine(os_id);

				script += "fi";
				script += remoteApiLib::NewLine(os_id);
				/*
				script += "else";
				script += remoteApiLib::NewLine(os_id);

				script += "rc=$?";
				script += remoteApiLib::NewLine(os_id);
				script += "echo \"unified agent binary extraction (gunzip) failed with exit status $rc \" >>";
				script += remoteErrorLog;
				script += remoteApiLib::NewLine(os_id);

				script += "echo ";
				script += m_jobdefinition.jobId();
				script += ":0:$rc:0";
				script += " > ";
				script += exitStatusFile;
				script += remoteApiLib::NewLine(os_id);

				script += "exit 1";
				script += remoteApiLib::NewLine(os_id);

				script += "fi";
				*/
				script += remoteApiLib::NewLine(os_id);

			}
			else {

				script += " tar -xvf ";
				script += m_jobdefinition.remoteUaPath();
				script += " >> " + remoteLog;
				script += " 2>> " + remoteErrorLog;
				script += remoteApiLib::NewLine(os_id);

				script += "if [ $? -eq 0 ]; then";
				script += remoteApiLib::NewLine(os_id);

				script += "echo \"unified agent binary extraction completed\" >>";
				script += remoteLog;
				script += remoteApiLib::NewLine(os_id);

				script += "else";
				script += remoteApiLib::NewLine(os_id);


				script += "rc=$?";
				script += remoteApiLib::NewLine(os_id);
				script += "echo \"unified agent binary extraction failed with exit status $rc \"  >> ";
				script += remoteErrorLog;
				script += remoteApiLib::NewLine(os_id);

				script += "echo ";
				script += m_jobdefinition.jobId();
				script += ":0:$rc:0";
				script += " > ";
				script += exitStatusFile;
				script += remoteApiLib::NewLine(os_id);

				script += "exit 1";
				script += remoteApiLib::NewLine(os_id);

				script += "fi";
				script += remoteApiLib::NewLine(os_id);
			}
		}
		std::string libpathvar;
		if (os.find("Solaris") != std::string::npos) {
			libpathvar = "LD_LIBRARY_PATH";
		}

		if (os.find("HP-UX") != std::string::npos) {
			libpathvar = "SHLIB_PATH";
		}

		if (os.find("AIX") != std::string::npos) {
			libpathvar = "LIBPATH";
		}


		if (os.find("Solaris") != std::string::npos ||
			os.find("HP-UX") != std::string::npos ||
			os.find("AIX") != std::string::npos) {

			script += libpathvar;
			script += "=";
			script += m_jobdefinition.remoteFolder();
			script += remoteApiLib::pathSeperator(os_id);
			script += os;
			script += "/lib:/lib:/usr/lib:";
			script += "$";
			script += libpathvar;
			script += remoteApiLib::NewLine(os_id);
			script += "export ";
			script += libpathvar;
		}

		script += "echo \"changing permission to r-xr-x- for push client exectuable\" >>";
		script += remoteLog;
		script += remoteApiLib::NewLine(os_id);

		script += "chmod 550 ";
		script += m_jobdefinition.remotePushClientExecutableAfterExtraction();
		script += " >> " + remoteLog;
		script += " 2>> " + remoteErrorLog;
		script += remoteApiLib::NewLine(os_id);

		script += "if [ $? -eq 0 ]; then";
		script += remoteApiLib::NewLine(os_id);

		script += "echo \"permissions changed to  r-xr-x- \" >>";
		script += remoteLog;
		script += remoteApiLib::NewLine(os_id);

		script += "else";
		script += remoteApiLib::NewLine(os_id);


		script += "rc=$?";
		script += remoteApiLib::NewLine(os_id);
		script += "echo \"chmod failed with exit status $rc \" >>";
		script += remoteErrorLog;
		script += remoteApiLib::NewLine(os_id);


		script += "echo ";
		script += m_jobdefinition.jobId();
		script += ":0:$rc:0";
		script += " > ";
		script += exitStatusFile;
		script += remoteApiLib::NewLine(os_id);

		script += "exit 1";
		script += remoteApiLib::NewLine(os_id);

		script += "fi";
		script += remoteApiLib::NewLine(os_id);


		script += "echo \"invoking push client\" >> ";
		script += remoteLog;
		script += remoteApiLib::NewLine(os_id);

		script += m_jobdefinition.remotePushClientInvocation();
		script += remoteApiLib::NewLine(os_id);

		script += "rc=$?";
		script += remoteApiLib::NewLine(os_id);
		script += "echo \"push client exited with status $rc \" >>";
		script += remoteErrorLog;
		script += remoteApiLib::NewLine(os_id);

		script += "exit $rc";
		script += remoteApiLib::NewLine(os_id);

		script += "fi";
		script += remoteApiLib::NewLine(os_id);

		return script;
	}


	void PushJob::copyFilesToRemoteMachine()
	{
		remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
		RemoteConnection remoteConnection(remoteApiLib::platform(os_id),
			m_jobdefinition.remoteip,
			m_jobdefinition.userName(),
			m_jobdefinition.password);

		// copy push client
		remoteConnection.impl()->copyFile(m_jobdefinition.pushclientLocalPath(), m_jobdefinition.remotePushClientPath());


		if (m_jobdefinition.IsInstallationRequest()) {

			if (m_jobdefinition.jobtype == PushJobType::pushFreshInstall || m_jobdefinition.jobtype == PushJobType::pushUpgrade)
			{
				// copy ua
				remoteConnection.impl()->copyFile(m_jobdefinition.UAOrPatchLocalPath(), m_jobdefinition.remoteUaPath());
			}

			if (m_jobdefinition.m_clientAuthenticationType == ClientAuthenticationType::Passphrase)
			{
				// copy connection passphrase
				std::string passphrase = m_proxy->GetPassphrase();
				remoteConnection.impl()->writeFile(passphrase.c_str(), passphrase.length(), m_jobdefinition.remoteConnectionPassPhrasePath());
			}
			else
			{
				remoteConnection.impl()->copyFile(m_jobdefinition.mobilityAgentConfigurationFilePath, m_jobdefinition.remoteMobilityAgentConfigurationFilePath());
			}

#ifdef WIN32
			// copy encryption key and encrypted password
			std::string encrypted_passwd, encryption_key;
			encryptcredentials(encrypted_passwd, encryption_key);

			remoteConnection.impl()->writeFile(encrypted_passwd.c_str(), encrypted_passwd.length(), m_jobdefinition.remoteEncryptionPasswordFile());
			remoteConnection.impl()->writeFile(encryption_key.c_str(), encryption_key.length(), m_jobdefinition.remoteEncryptionKeyFile());
#endif
		}

		// copy push job spec
		std::string jobSpec = preparePushClientJobSpec();
		remoteConnection.impl()->writeFile(jobSpec.c_str(), jobSpec.length(), m_jobdefinition.remoteSpecFile());

		if ((os_id == remoteApiLib::unix_idx)) {
			std::string unixInstallScript = preparePushJobScriptForUnix();
			remoteConnection.impl()->writeFile(unixInstallScript.c_str(), unixInstallScript.length(), m_jobdefinition.remoteInstallerScriptPathOnUnix());
		}
	}

	void PushJob::executeJobOnRemoteMachine()
	{
		remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
		RemoteConnection remoteConnection(remoteApiLib::platform(os_id),
			m_jobdefinition.remoteip,
			m_jobdefinition.userName(),
			m_jobdefinition.password);

		if (os_id == remoteApiLib::windows_idx) {
			remoteConnection.impl()->run(m_jobdefinition.remotePushClientInvocation(), m_config->jobTimeOutInSecs());
		}
		else {
			remoteConnection.impl()->run(m_jobdefinition.remoteInstallerScriptPathOnUnix(), m_config->jobTimeOutInSecs());
		}
	}

	void PushJob::consumeExitStatus(std::string & output, int & exitStatus, std::string & installerExtendedErrorsJson)
	{
		int attempt = 1;
		bool done = false;

		while (!done)
		{
			try {

				DebugPrintf(SV_LOG_DEBUG, "Attempt:%d fetching job execution status on remote machine %s\n", attempt, m_jobdefinition.remoteip.c_str());
				remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
				RemoteConnection remoteConnection(remoteApiLib::platform(os_id),
					m_jobdefinition.remoteip,
					m_jobdefinition.userName(),
					m_jobdefinition.password);

				bool fileNotAvailable = false;
				std::string pidVal = remoteConnection.impl()->readFile(m_jobdefinition.remotePidPath(), fileNotAvailable);
				if (fileNotAvailable){
					DebugPrintf(SV_LOG_DEBUG, "%s file is not available on the remote machine with IP %s\n", m_jobdefinition.remotePidPath().c_str(), m_jobdefinition.remoteip.c_str());
				}
				else {
					DebugPrintf(SV_LOG_DEBUG, "Read %s file from the remote machine with IP %s successfully.\n", m_jobdefinition.remotePidPath().c_str(), m_jobdefinition.remoteip.c_str());
				}

				std::string log = remoteConnection.impl()->readFile(m_jobdefinition.remoteLogPath(), fileNotAvailable);
				if (fileNotAvailable){
					DebugPrintf(SV_LOG_DEBUG, "%s file is not available on the remote machine with IP %s\n", m_jobdefinition.remoteLogPath().c_str(), m_jobdefinition.remoteip.c_str());
				}
				else {
					DebugPrintf(SV_LOG_DEBUG, "Read %s file from the remote machine with IP %s successfully.\n", m_jobdefinition.remoteLogPath().c_str(), m_jobdefinition.remoteip.c_str());
				}

				std::string errors = remoteConnection.impl()->readFile(m_jobdefinition.remoteErrorLogPath(), fileNotAvailable);
				if (fileNotAvailable){
					DebugPrintf(SV_LOG_DEBUG, "%s file is not available on the remote machine with IP %s\n", m_jobdefinition.remoteErrorLogPath().c_str(), m_jobdefinition.remoteip.c_str());
				}
				else {
					DebugPrintf(SV_LOG_DEBUG, "Read %s file from the remote machine with IP %s successfully.\n", m_jobdefinition.remoteErrorLogPath().c_str(), m_jobdefinition.remoteip.c_str());
				}

				installerExtendedErrorsJson = remoteConnection.impl()->readFile(m_jobdefinition.remoteInstallerExtendedErrorsJsonPath(), fileNotAvailable);
				if (fileNotAvailable){
					DebugPrintf(SV_LOG_DEBUG, "%s file is not available on the remote machine with IP %s\n", m_jobdefinition.remoteInstallerExtendedErrorsJsonPath().c_str(), m_jobdefinition.remoteip.c_str());
				}
				else {
					DebugPrintf(SV_LOG_DEBUG, "Read %s file from the remote machine with IP %s successfully.\n", m_jobdefinition.remoteInstallerExtendedErrorsJsonPath().c_str(), m_jobdefinition.remoteip.c_str());
				}

				DebugPrintf(SV_LOG_DEBUG, "InstallerExtendedErrors.json content: %s", installerExtendedErrorsJson.c_str());

				if (!log.empty()){
					output += "push client log:\n";
					output += "----------------\n";
					output += log;
					output += "----------------\n";
				}

				if (!errors.empty()){
					output += "push client errors:\n";
					output += "----------------\n";
					output += errors;
					output += "----------------\n";
				}

				std::string exitStatusString = remoteConnection.impl()->readFile(m_jobdefinition.remoteExitStatusPath(), fileNotAvailable);

				// If push client terminated abruptly, remoteExitStatusPath will not present
				// consider this as failure.
				if (fileNotAvailable) {
					output += "push client terminated without returning proper exit code.\n";
					DebugPrintf(SV_LOG_ERROR, "%s for job id: %s remote ip: %s\n", output.c_str(), m_jobdefinition.jobid.c_str(), m_jobdefinition.remoteip.c_str());
					exitStatus = 1;
					return;
				}

				DebugPrintf(SV_LOG_DEBUG, "exit status: %s\n", exitStatusString.c_str());
				if (!exitStatusString.empty()){
					std::string jobid;
					int prescriptExitCode = 0;
					int postscriptExitCode = 0;

					std::string::size_type idx = exitStatusString.find_first_of(":");
					if (idx == std::string::npos){
						throw ERROR_EXCEPTION << "internal error, push client did not return proper exit status (" << exitStatusString << ")";
					}

					jobid = exitStatusString.substr(0, idx);
					std::string remaining = exitStatusString.substr(idx + 1);

					idx = remaining.find_first_of(":");
					if (idx == std::string::npos){
						throw ERROR_EXCEPTION << "internal error, push client did not return proper exit status (" << exitStatusString << ")";
					}

					prescriptExitCode = boost::lexical_cast<int>(remaining.substr(0, idx));
					remaining = remaining.substr(idx + 1);

					idx = remaining.find_first_of(":");
					if (idx == std::string::npos){
						throw ERROR_EXCEPTION << "internal error, push client did not return proper exit status (" << exitStatusString << ")";
					}

					exitStatus = boost::lexical_cast<int>(remaining.substr(0, idx));
					remaining = remaining.substr(idx + 1);

					boost::algorithm::trim(remaining);
					postscriptExitCode = boost::lexical_cast<int>(remaining);
				}
				else {
					throw ERROR_EXCEPTION << "internal error, push client did not return any exit status";
				}

				done = true;
			}
			catch (CredentialErrorException){//No retries for credential errors
				throw;
			}			
			catch (ErrorException & ee){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d fetching job execution status on remote machine %s failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), ee.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (std::exception &e){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d fetching job execution status on remote machine %s  failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), e.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (...) {

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d fetching job execution status on remote machine %s failed with unhandled exception\n", attempt, m_jobdefinition.remoteip.c_str());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
		}
	}

	std::string PushJob::GetSourceMachineIpv4AddressesAndFqdn(std::string& ipAddresses)
	{
		// First attempt connection with IPv4 address. Skip IPv6 addresses.
		// If none of them is reachable, then use hostname to connect to the source machine.
		std::vector<std::string> ipAddressList, ipv4Addresses, fqdns, sortedIpAddresses;
		boost::split(ipAddressList, ipAddresses, boost::is_any_of(","));
		for (int i = 0; i < ipAddressList.size(); i++)
		{
			boost::system::error_code ec;
			auto address = boost::asio::ip::address_v4::from_string(ipAddressList[i], ec);
			if (!ec)
			{
				ipv4Addresses.push_back(ipAddressList[i]);
			}
			else {
				auto address = boost::asio::ip::address::from_string(ipAddressList[i], ec);
				if (ec)
				{
					// this string is not a IPv4 or IPv6 address. So, considering this as FQDN.
					fqdns.push_back(ipAddressList[i]);
				}
			}
		}

		sortedIpAddresses.insert(sortedIpAddresses.end(), ipv4Addresses.begin(), ipv4Addresses.end());

		// Use fqdn for DNS resolution only when IPv4 addresses are not provided by discovery.
		// Since SDS provides the IP address, DNS resolution should not be performed as it might lead
		// to installation on unintended machine.
		// DNS resolution  should be attempted only when SDS is unable to report IP addresses.
		// Hence add FQDN only when no IPv4 address is reported by SDS.
		// This is only applicable to RCM stack where SDS reports IP addresses. 
		// In CS stack, FQDN is not available as job input and hence will not be used for DNS resolution.
		if (ipv4Addresses.size() == 0)
		{
			sortedIpAddresses.insert(sortedIpAddresses.end(), fqdns.begin(), fqdns.end());
		}

		// if filtered IP addresses are present then return a comma separated list of IPs/FQDNs else return an empty string.
		return sortedIpAddresses.size() > 0 ? boost::algorithm::join(sortedIpAddresses, ",") : std::string();
	}

	void PushJob::getRemoteMachineIp(const std::string &ipAddresses, std::string &reachableIpAddress)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", __FUNCTION__);

		std::string ipAddress;
		int attempt = 1;
		bool done = false;

		std::vector<std::string> ipAddressList;
		// ipAddresses will contain only comma separated IPV4 addresses or FQDN(when no IPV4 addresses are discovered).
		boost::split(ipAddressList, ipAddresses, boost::is_any_of(","));

		DebugPrintf(SV_LOG_ALWAYS, "Received remote ip address = %s\n", ipAddresses.c_str());
		while (!done){
			try{
				for (int i = 0; i < ipAddressList.size(); i++)
				{
					ipAddress = ipAddressList[i];
					boost::algorithm::trim(ipAddress);
					DebugPrintf(SV_LOG_ALWAYS, "Ping remote machine %s\n", ipAddress.c_str());
					if (isRemoteMachineReachable(ipAddress)){
						DebugPrintf(SV_LOG_ALWAYS, "Ping success for %s\n", ipAddress.c_str());
						reachableIpAddress = ipAddress;
						done = true;
						break;
					}
					else{
						DebugPrintf(SV_LOG_ALWAYS, "Ping failed for %s\n", ipAddress.c_str());
						reachableIpAddress = "";
					}
				}
				if (reachableIpAddress.empty()){
					DebugPrintf(SV_LOG_ALWAYS, "None of the remote machine ip is reachable. IP = %s\n", ipAddresses.c_str());
					throw ERROR_EXCEPTION << "None of the remote machine ip is reachable. IP : " << ipAddresses.c_str();
				}
			}
			catch (ErrorException & ee){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d verifying connection to remote machine %s failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), ee.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (std::exception &e){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d verifying connection to remote machine %s failed with error %s\n", attempt, m_jobdefinition.remoteip.c_str(), e.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (...) {

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d verifying connection to remote machine %s failed with unhandled exception\n", attempt, m_jobdefinition.remoteip.c_str());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", __FUNCTION__);
	}

	bool PushJob::isRemoteMachineReachable(std::string &ipAddress)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", __FUNCTION__);
		bool bRet = true;
		int rc;

		int sock = socket(AF_INET, SOCK_STREAM, 0);

		if (-1 == sock){
			std::string err;
			convertErrorToString(lastKnownError(), err);
			DebugPrintf(SV_LOG_ALWAYS, "socket creation failed with error : %lu, description: %s\n", lastKnownError(), err.c_str());
			throw ERROR_EXCEPTION << "socket creation failed with error : " << lastKnownError() << "(" << err.c_str() << ")";
		}

		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		if (1 == m_jobdefinition.os_id){
			sin.sin_port = htons(135);
		}else{
			sin.sin_port = htons(22);
		}

		ULONG sourceAddress = inet_addr(ipAddress.c_str());

		if (sourceAddress == 0 || sourceAddress == 4294967295)
		{
			// When host name is used to connect to the source machine, this value would be 
			// 4294967295 (maximum value of ULONG) since host name is not a valid IP format address.
			// Hence, getting the source machine Ip addresses using getaddrinfo()
			struct addrinfo hints;
			struct addrinfo *result, *it;
			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_ALL;
			hints.ai_protocol = 0;
			hints.ai_canonname = NULL;
			hints.ai_addr = NULL;
			hints.ai_next = NULL;

			// Looking up Fqdn for the IP addresses.
			int s = getaddrinfo(ipAddress.c_str(), NULL, &hints, &result);
			if (s != 0) {
				DebugPrintf(
					SV_LOG_ALWAYS,
					"getaddrinfo for %s failed with error: %s\n",
					ipAddress.c_str(),
					gai_strerror(s));

#ifdef WIN32
				closesocket(sock);
#else
				close(sock);
#endif
				sock = -1;
				bRet = false;
				return bRet;
			}

			// Alternate way to get the ip addresses is to use boost library.
			/*boost::asio::io_service io_service;
			boost::asio::ip::tcp::resolver resolver(io_service);
			boost::asio::ip::tcp::resolver::query query(ipAddress, "");
			for (boost::asio::ip::tcp::resolver::iterator i = resolver.resolve(query);
				i != boost::asio::ip::tcp::resolver::iterator();
				++i)
			{
				boost::asio::ip::tcp::endpoint end = *i;
				DebugPrintf(
					SV_LOG_ALWAYS,
					"endpoint from boost: %s\n",
					end.address().to_string().c_str());
			}*/

			// Looping through all the IP addresses to check connectivity.
			it = result;
			while (it != NULL)
			{
				struct sockaddr_in *sockaddr_ip = (struct sockaddr_in *) it->ai_addr;
				std::string ipOfFqdn = std::string(inet_ntoa(sockaddr_ip->sin_addr));
				DebugPrintf(
					SV_LOG_ALWAYS,
					"getaddrinfo for %s returned: %s\n",
					ipAddress.c_str(),
					ipOfFqdn.c_str());

				DebugPrintf(
					SV_LOG_DEBUG,
					"Checking connectivity to %s obtained by resolving DNS for %s.\n",
					ipOfFqdn.c_str(),
					ipAddress.c_str());

				boost::system::error_code ec;
				auto address = boost::asio::ip::address_v4::from_string(ipOfFqdn, ec);
				if (!ec)
				{
					if (isRemoteMachineReachable(ipOfFqdn))
					{
						DebugPrintf(
							SV_LOG_DEBUG,
							"Connected to %s obtained by resolving DNS for %s.\n",
							ipOfFqdn.c_str(),
							ipAddress.c_str());
						ipAddress = ipOfFqdn;
#ifdef WIN32
						closesocket(sock);
#else
						close(sock);
#endif
						sock = -1;
						freeaddrinfo(result);
						bRet = true;
						return bRet;
					}
					else
					{
						DebugPrintf(
							SV_LOG_DEBUG,
							"Failed to connect to %s obtained by resolving DNS for %s.\n",
							ipOfFqdn.c_str(),
							ipAddress.c_str());
					}
				}

				it = it->ai_next;
			}
			
#ifdef WIN32
			closesocket(sock);
#else
			close(sock);
#endif
			freeaddrinfo(result);
			bRet = false;
		}
		else
		{
			sin.sin_addr.s_addr = sourceAddress;
			if (connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
				std::string err;
				convertErrorToString(lastKnownError(), err);
				DebugPrintf(SV_LOG_ALWAYS, "connect to %s failed with error : %lu, description: %s\n", ipAddress.c_str(), lastKnownError(), err.c_str());
				bRet = false;
			}
#ifdef WIN32
			closesocket(sock);
#else
			close(sock);
#endif
			sock = -1;
		}
		
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", __FUNCTION__);
		return bRet;
	}

	void PushJob::installMobilityService(std::string & errorCode, std::map<std::string, std::string> &placeHolders, std::string &errMessageToDisplay)
	{
		int attempt = 1;
		bool done = false;
		std::string debugMsg;
		std::string runningStage;

		while (!done){
			try{

				std::string pushServerIp = Host::GetInstance().GetIPAddress();
				std::string pushServerName = Host::GetInstance().GetHostName();

				debugMsg = "copying files to remote machine";
				DebugPrintf(SV_LOG_DEBUG, "Attempt:%d %s %s\n", attempt, debugMsg.c_str(), m_jobdefinition.remoteip.c_str());

				// copy push client and ua/patch (for install job) and rest of files to remote machine
				runningStage = "copying agent to remote machine...";
				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psIp] = pushServerIp;
				placeHolders[psName] = pushServerName;
				if (remoteApiLib::windows_idx == m_jobdefinition.os_id){
					errorCode = "EP0856";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0856(placeHolders);
				}
				else{
					errorCode = "EP0857";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0857(placeHolders);
				}
				m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, runningStage);
				copyFilesToRemoteMachine();

				// execute the job on remote machine
				debugMsg = "executing job on remote machine";
				DebugPrintf(SV_LOG_DEBUG, "Attempt:%d %s %s\n", attempt, debugMsg.c_str(), m_jobdefinition.remoteip.c_str());

				placeHolders.clear();
				placeHolders[sourceIp] = m_jobdefinition.remoteip;
				placeHolders[sourceName] = m_jobdefinition.vmName;
				placeHolders[psIp] = pushServerIp;
				placeHolders[psName] = pushServerName;
				if (remoteApiLib::windows_idx == m_jobdefinition.os_id){
					errorCode = "EP0854";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0854(placeHolders);
				}
				else{
					errorCode = "EP0855";
					placeHolders[errCode] = errorCode;
					errMessageToDisplay = getErrorEP0855(placeHolders);
				}

				if (m_jobdefinition.IsInstallationRequest()){
					runningStage = "starting agent installation on remote machine...";
					m_jobdefinition.jobStatus = INSTALL_JOB_PROGRESS;
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, runningStage);
				}
				else {
					runningStage = "starting agent un-installation on remote machine...";
					m_jobdefinition.jobStatus = UNINSTALL_JOB_PROGRESS;
					m_proxy->UpdateAgentInstallationStatus(m_jobdefinition.jobId(), m_jobdefinition.jobStatus, runningStage);
				}
				executeJobOnRemoteMachine();
				done = true;

				//// TODO (rovemula) - delete the mobility agent configuration file on the source machine
				//// in case of failure and success.
			}
			catch (NonRetryableErrorException){//No retries for timeout errors
				throw;
			}
			catch (CredentialErrorException){ //No retries for credential errors
				throw;
			}
			catch (RemoteApiErrorException& ree) {
				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d %s %s failed with error %s\n", attempt, debugMsg.c_str(), m_jobdefinition.remoteip.c_str(), ree.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (ErrorException & ee){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d %s %s failed with error %s\n", attempt, debugMsg.c_str(), m_jobdefinition.remoteip.c_str(), ee.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (std::exception &e){

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d %s %s failed with error %s\n", attempt, debugMsg.c_str(), m_jobdefinition.remoteip.c_str(), e.what());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
			catch (...) {

				if (attempt > m_config->jobRetries()) {
					throw;
				}

				DebugPrintf(SV_LOG_ERROR, "Attempt:%d %s %s failed with unhandled exception\n", attempt, debugMsg.c_str(), m_jobdefinition.remoteip.c_str());
				++attempt;
				CDPUtil::QuitRequested(m_config->jobRetryIntervalInSecs());
			}
		}
	}

	void PushJob::PullAgentTelemetryFile(const std::string & remoteFilepath, const std::string & localFilepath)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", __FUNCTION__);

		try
		{
			bool fileNotAvailable = false;
			remoteApiLib::os_idx os_id = (enum remoteApiLib::os_idx)m_jobdefinition.os_id;
			RemoteConnection remoteConnection(remoteApiLib::platform(os_id),
				m_jobdefinition.remoteip,
				m_jobdefinition.userName(),
				m_jobdefinition.password,
				m_jobdefinition.remoteFolder());

			std::string filecontent = remoteConnection.impl()->readFile(remoteFilepath, fileNotAvailable);
			if (fileNotAvailable)
			{
				DebugPrintf(SV_LOG_ERROR, "Agent Setup Telemetry file %s doesn't exist", remoteFilepath.c_str());
				return;
			}

			ofstream filehandle(localFilepath, ios::out | ios::binary);
			if (filehandle.good()) 
			{
				filehandle.write(filecontent.c_str(), filecontent.length());
				filehandle.close();
				DebugPrintf(
					SV_LOG_DEBUG,
					"Written agent setup telemetry file %s.",
					localFilepath.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Error writing to file %s", localFilepath.c_str());
			}

		}
		catch (std::exception &e) 
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Fetching telemetry file %s from %s failed with error %s\n",
				remoteFilepath.c_str(),
				m_jobdefinition.remoteip.c_str(), 
				e.what());
		}
		catch (...) 
		{
			DebugPrintf(SV_LOG_ERROR, "%s: unhandled exception", FUNCTION_NAME);
		}
	}

	void PushJob::PullAgentTelemetryFiles()
	{
		try
		{
			PullAgentTelemetryFile(
				m_jobdefinition.remoteInstallerJsonPath(),
				m_jobdefinition.LocalInstallerJsonPath());

			PullAgentTelemetryFile(
				m_jobdefinition.remoteConfiguratorJsonPath(),
				m_jobdefinition.LocalConfiguratorJsonPath());

			PullAgentTelemetryFile(
				m_jobdefinition.remoteLogPath(),
				m_jobdefinition.LocalInstallerLogPath());

			PullAgentTelemetryFile(
				m_jobdefinition.remoteErrorLogPath(),
				m_jobdefinition.LocalInstallerErrorLogPath());

			PullAgentTelemetryFile(
				m_jobdefinition.remoteExitStatusPath(),
				m_jobdefinition.LocalInstallerExitStatusPath());
		}
		catch (std::exception &e) 
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Fetching telemetry files for remote ip %s failed with error %s.\n",
				m_jobdefinition.remoteip.c_str(),
				e.what());
		}
		catch (...) {
			DebugPrintf(SV_LOG_ERROR, "%s: Unhandled exception", __FUNCTION__);
		}
	}

}
