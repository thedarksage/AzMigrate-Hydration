#include <windows.h>

#include <iostream>
#include <vector>

#include <ace/ACE.h>
#include <ace/Init_ACE.h>
#include <boost/algorithm/string.hpp>
#include <filesystem>

#include "version.h"
#include "HandlerCurl.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include <cdputil.h>
#include "inmsafecapis.h"
#include "logger.h"
#include "terminateonheapcorruption.h"
#include "securityutils.h"
#include "pushjob.h"
#include "RcmCommunicationImpl.h"
#include "RcmPushInstallSWMgr.h"
#include "RcmJob.h"
#include "RcmJobInputs.h"
#include "NotImplementedException.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "json_reader.h"
#include "json_writer.h"
#include "json_adapter.h"
#include "RcmJobInputValidationFailedException.h"

#include <chrono>
#include <thread>

using namespace PI;

void CreateRequiredLogFolders(PushJobDefinition& jobDefinition);

int main( int argc, char **argv )
{
	if (argc == 0)
	{
		return -1;
	}
	
	try
	{
		std::string exePath(argv[0]);
		std::string::size_type idx = exePath.find_last_of("\\");
		std::string exeDirectory = exePath.substr(0, idx);
		std::string logFilePath = exeDirectory + remoteApiLib::pathSeperator() + "PushInstallService.log";
		std::string errorMessage = "";

		bool runInDaemonMode = true;
		for (int i = 1; i < argc; i++)
		{
			std::string inputParam(argv[i]);

			if (boost::iequals(inputParam, "/interactive"))
			{
				runInDaemonMode = false;
			}
		}

		if (runInDaemonMode)
		{
			SetDaemonMode();
		}

		SetLogLevel(static_cast<SV_LOG_LEVEL>(7));
		SetLogFileName(logFilePath.c_str());

		std::string jobInputFilePath = "";
		std::string jobOutputFilePath = "";
		std::string jobLogsFolder = "";
		std::string jobTunablesFilePath = "";

		for (int i = 1; i < argc; i++)
		{
			std::string inputParam(argv[i]);

			if (boost::iequals(inputParam, "/jobInputFilePath"))
			{
				i++;
				if (i < argc)
				{
					jobInputFilePath = std::string(argv[i]);
					if (jobInputFilePath == "")
					{
						errorMessage = "Rcm job input file path passed is empty.";
						DebugPrintf(
							SV_LOG_ERROR,
							"%s.\n",
							errorMessage.c_str());
						throw RcmJobInputValidationFailedException("JobInputFilePath", errorMessage);
					}
				}
				else
				{
					errorMessage = 
						"Command line parameters passed are not complete. Job input file path is missing.";
					DebugPrintf(
						SV_LOG_ERROR,
						"%s.\n",
						errorMessage.c_str());
					throw RcmJobInputValidationFailedException("JobInputFilePath", errorMessage);
				}
			}
			else if (boost::iequals(inputParam, "/jobOutputFilePath"))
			{
				i++;
				if (i < argc)
				{
					jobOutputFilePath = std::string(argv[i]);
					if (jobOutputFilePath == "")
					{
						errorMessage =
							"Output file path passed is empty.";
						DebugPrintf(
							SV_LOG_ERROR,
							"%s.\n",
							errorMessage.c_str());
						throw RcmJobInputValidationFailedException("JobOutputFilePath", errorMessage);
					}
				}
				else
				{
					errorMessage =
						"Command line parameters passed are not complete. Job output file path is missing.";
					DebugPrintf(
						SV_LOG_ERROR,
						"%s.\n",
						errorMessage.c_str());
					throw RcmJobInputValidationFailedException("JobOutputFilePath", errorMessage);
				}
			}
			else if (boost::iequals(inputParam, "/kustoLogsFolder"))
			{
				i++;
				if (i < argc)
				{
					jobLogsFolder = std::string(argv[i]);
				}
				else
				{
					errorMessage =
						"Command line parameters passed are not complete. Job logs folder is missing.";
					DebugPrintf(
						SV_LOG_ERROR,
						"%s.\n",
						errorMessage.c_str());
					throw RcmJobInputValidationFailedException("JobLogsFolder", errorMessage);
				}
			}
			else if (boost::iequals(inputParam, "/jobConfigFilePath"))
			{
				i++;
				if (i < argc)
				{
					jobTunablesFilePath = std::string(argv[i]);
				}
				else
				{
					errorMessage =
						"Command line parameters passed are not complete. Job tunables file path is missing.";
					DebugPrintf(
						SV_LOG_ERROR,
						"%s.\n",
						errorMessage.c_str());
					throw RcmJobInputValidationFailedException("JobTunablesFilePath", errorMessage);
				}
			}
		}

		if (!boost::filesystem::exists(jobInputFilePath))
		{
			errorMessage =
				"Input push install job json file does not exist.";
			DebugPrintf(
				SV_LOG_ERROR,
				"Input push install job json file %s does not exist.\n",
				jobInputFilePath.c_str());
			throw RcmJobInputValidationFailedException("JobInputFilePath", errorMessage);
		}

		RcmPushConfig::Initialize(jobTunablesFilePath, jobLogsFolder);
		RcmCommunicationImpl::Initialize(jobInputFilePath, jobOutputFilePath);

		PushJobDefinition jobDefinition(
			std::string(),
			1,
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
			PUSH_JOB_STATUS::INSTALL_JOB_PROGRESS,
			InstallationMode::WMISSH,
			(PushInstallCommunicationBasePtr)NULL,
			(PushConfigPtr)NULL,
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string(),
			std::string());

		jobDefinition = RcmCommunicationImpl::Instance()->GetPushJobDefinition();

		CreateRequiredLogFolders(jobDefinition);

		SetThreadSpecificLogFileName(jobDefinition.PITemporaryVerboseLogFilePath().c_str());

		PushJob job(
			jobDefinition,
			(PushInstallCommunicationBasePtr)(RcmCommunicationImpl::Instance()),
			(PushConfigPtr)(RcmPushConfig::Instance()),
			(PushInstallSWMgrBasePtr)(RcmPushInstallSWMgr::Instance()));

		job.execute();

		try
		{
			jobDefinition.DeleteTemporaryPushInstallFiles();
		}
		catch (std::exception &e)
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Failed to delete temporary files for job %s, client request id %s. Exception: %s\n",
				jobDefinition.jobId().c_str(),
				jobDefinition.clientRequestId.c_str(),
				e.what());
		}

		CloseDebugThreadSpecific();
	}
	catch (std::exception & ex)
	{
		std::string exType = typeid(ex).name();
		DebugPrintf(
			SV_LOG_ERROR,
			"Caught exception of type %s in main: %s.\n",
			exType.c_str(),
			ex.what());

		// exType would be of format "class PI::CredStoreAccountNotFoundException". So parsing the 
		// exception name from the string.
		if (exType.empty() || exType.find("std::exception") != std::string::npos)
		{
			exType = "PI_INTERNAL_ERROR";
		}
		else
		{
			std::size_t index = exType.find_last_of(" ");
			if (index != std::string::npos)
			{
				exType = exType.substr(index + 1);
			}

			index = exType.find_last_of(":");
			if (index != std::string::npos)
			{
				exType = exType.substr(index + 1);
			}

			if (exType == "ErrorException")
			{
				exType = "PI_INTERNAL_ERROR";
			}
		}

		RcmCommunicationImpl::Instance()->UpdateAgentInstallationStatus(
			"",
			INSTALL_JOB_FAILED,
			ex.what(),
			"UNKNOWN-HOST-ID",
			"",
			exType);

		return -1;
	}

	return 0;
}

void CreateRequiredLogFolders(PushJobDefinition& jobDefinition)
{
	if (SVMakeSureDirectoryPathExists(jobDefinition.PITemporaryFolder().c_str()).failed())
	{
		DebugPrintf(
			SV_LOG_ERROR,
			"Could not create the directory = %s\n",
			jobDefinition.PITemporaryFolder().c_str());
	}

	if (SVMakeSureDirectoryPathExists(jobDefinition.AgentTemporaryFolder().c_str()).failed())
	{
		DebugPrintf(
			SV_LOG_ERROR,
			"Could not create the directory = %s\n",
			jobDefinition.AgentTemporaryFolder().c_str());
	}
}


