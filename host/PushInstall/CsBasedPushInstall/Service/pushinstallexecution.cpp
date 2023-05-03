#include "pushinstallexecution.h"
#include <ace/Process_Manager.h>
#include <boost/filesystem.hpp>

#include "file.h"
#include "inmsafecapis.h"
#include "genpassphrase.h"

#include "portablehelpers.h"
#include "pushjob.h"
#include "pushjobdefinition.h"
#include "supportedplatforms.h"
#include "remoteapi.h"
#include "remoteconnection.h"
#include "logger.h"
#include "CsCommunicationImpl.h"
#include "CsPushInstallSWMgr.h"

using namespace remoteApiLib;
using namespace PI;

SVERROR PushInstallerThread::Start()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bRetStatus = SVS_OK;

	if (m_ThreadManager->spawn((ACE_THR_FUNC)PushInstallerThread::EntryPoint,
		this,
		THR_NEW_LWP | THR_JOINABLE,
		&m_threadid,
		0,
		ACE_DEFAULT_THREAD_PRIORITY) == -1)  {
		bRetStatus = SVE_FAIL;
		m_bIsActive = false;
	}

	m_bIsActive = true;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRetStatus;
}



void* PushInstallerThread::EntryPoint(void* pThis)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    PushInstallerThread* piThread = NULL;
    piThread = (PushInstallerThread *)pThis;
    if(piThread != NULL)
    {
        piThread->doWork();
        piThread->m_bIsActive = false;
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }
    return 0;
}

SVERROR PushInstallerThread::doWork()
{
	std::string tempLogFilepath =
		m_jobInfo.PITemporaryFolder() + 
		remoteApiLib::pathSeperator() +
		m_jobInfo.PIVerboseLogFileName();

	if (SVMakeSureDirectoryPathExists(CsPushConfig::Instance()->PushInstallTelemetryLogsPath().c_str()).failed())
	{
		DebugPrintf(
			SV_LOG_ERROR,
			"Could not create the directory = %s\n",
			CsPushConfig::Instance()->PushInstallTelemetryLogsPath().c_str());
	}

	if (SVMakeSureDirectoryPathExists(CsPushConfig::Instance()->AgentTelemetryLogsPath().c_str()).failed())
	{
		DebugPrintf(
			SV_LOG_ERROR,
			"Could not create the directory = %s\n",
			CsPushConfig::Instance()->AgentTelemetryLogsPath().c_str());
	}

	if (SVMakeSureDirectoryPathExists(m_jobInfo.PITemporaryFolder().c_str()).failed())
	{
		DebugPrintf(
			SV_LOG_ERROR,
			"Could not create the directory = %s\n",
			m_jobInfo.PITemporaryFolder().c_str());
	}

	if (SVMakeSureDirectoryPathExists(m_jobInfo.AgentTemporaryFolder().c_str()).failed())
	{
		DebugPrintf(
			SV_LOG_ERROR,
			"Could not create the directory = %s\n",
			m_jobInfo.AgentTemporaryFolder().c_str());
	}

	SetThreadSpecificLogFileName(tempLogFilepath.c_str());

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	SVERROR bRetStatus = SVS_OK;
	std::vector<std::string> listFiles;
	std::stringstream errorStream;
	std::stringstream statusStream;

	PushJob pushjob(m_jobInfo, (PushInstallCommunicationBasePtr)(CsCommunicationImpl::Instance()), (PushConfigPtr)(CsPushConfig::Instance()), (PushInstallSWMgrBasePtr)(CsPushInstallSWMgr::Instance()));
	pushjob.execute();

	DebugPrintf(SV_LOG_DEBUG, "Deleting unnecessary temporary push install files from telemetry folder.\n");
	try
	{
		m_jobInfo.DeleteTemporaryPushInstallFiles();
	}
	catch (std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s %s.\n", FUNCTION_NAME, e.what());
	}

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	CloseDebugThreadSpecific();

	boost::system::error_code ec;
	DebugPrintf(SV_LOG_DEBUG, "Renaming '%s' to '%s'.\n", m_jobInfo.PITemporaryFolder().c_str(), m_jobInfo.PITelemetryFolder().c_str());
	try
	{
		if (SVMakeSureDirectoryPathExists(m_jobInfo.PITemporaryUploadFolder().c_str()).failed())
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Could not create the directory = %s. So, not sending files to telemetry.\n",
				m_jobInfo.PITemporaryUploadFolder().c_str());
		}
		else {
			m_jobInfo.MoveTelemetryFilesToTemporaryTelemetryUploadFolder();

			boost::filesystem::rename(m_jobInfo.PITemporaryUploadFolder().c_str(), m_jobInfo.PITelemetryFolder().c_str(), ec);
			if (ec.value())
			{
				DebugPrintf(
					SV_LOG_ERROR,
					"Failed to rename push install telemetry upload folder %s with error %d (%s).\n",
					m_jobInfo.PITemporaryUploadFolder().c_str(),
					ec.value(),
					ec.message().c_str());
			}

			boost::filesystem::remove_all(m_jobInfo.PITemporaryFolder(), ec);
			if (ec.value())
			{
				DebugPrintf(
					SV_LOG_ERROR,
					"Failed to remove push install telemetry folder %s with error %d (%s).\n",
					m_jobInfo.PITemporaryFolder().c_str(),
					ec.value(),
					ec.message().c_str());
			}
		}
	}
	catch (std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s %s.\n", FUNCTION_NAME, e.what());
	}

	DebugPrintf(SV_LOG_DEBUG, "Renaming '%s' to '%s'.\n", m_jobInfo.AgentTemporaryFolder().c_str(), m_jobInfo.AgentTelemetryFolder().c_str());
	try
	{
		boost::filesystem::rename(m_jobInfo.AgentTemporaryFolder().c_str(), m_jobInfo.AgentTelemetryFolder().c_str(), ec);
		if (ec.value())
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Failed to rename agent telemetry folder %s with error %d (%s).\n",
				m_jobInfo.AgentTemporaryFolder().c_str(),
				ec.value(),
				ec.message().c_str());
		}
	}
	catch (std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s %s.\n", FUNCTION_NAME, e.what());
	}

	DebugPrintf(SV_LOG_DEBUG, "Deleting folder '%s'.\n", m_jobInfo.TemporaryFolder().c_str());
	try
	{
		boost::filesystem::remove(m_jobInfo.TemporaryFolder().c_str());
		if (ec.value())
		{
			DebugPrintf(
				SV_LOG_ERROR,
				"Failed to delete telemetry folder %s with error %d (%s).\n",
				m_jobInfo.TemporaryFolder().c_str(),
				ec.value(),
				ec.message().c_str());
		}
	}
	catch (std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s %s.\n", FUNCTION_NAME, e.what());
	}

    return bRetStatus;
}

