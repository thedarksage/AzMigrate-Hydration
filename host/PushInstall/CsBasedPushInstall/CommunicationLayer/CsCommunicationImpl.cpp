//---------------------------------------------------------------
//  <copyright file="CsCommunicationImpl.cpp" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements proxy for communication with Cs.
//  </summary>
//
//  History:     04-Sep-2018    rovemula    Created
//----------------------------------------------------------------

#ifndef __CS_PROXY_CPP__
#define __CS_PROXY_CPP__

#include <boost/thread/mutex.hpp>

#include "initialsettings.h"
#include "host.h"
#include "CsCommunicationImpl.h"

#include "serializepushInstallationSettings.h"
#include "serializeinitialsettings.h"
#include "configurevxagent.h"
#include "configurator2.h"

#include "portable.h"
#include "logger.h"
#include "converterrortostringminor.h"
#include "errorexception.h"
#include "errorexceptionmajor.h"
#include "securityutils.h"
#include "getpassphrase.h"
#include "cdputil.h"
#include "CsPushConfig.h"
#include "MobilityAgentNotFoundException.h"

namespace PI
{

	const char GET_INSTALL_TASKS_PUSH_CLIENT[] = "GetInstallDetails";
	const char SET_INSTALL_STATUS_PUSH_CLIENT[] = "UpdateAgentInstallationStatus";
	const char GET_PUSHINSTALLER_CLIENT_PATH[] = "getPushClientPath";
	const char GET_BUILD_PATH[] = "GetBuildPath";

	CsCommunicationImplPtr g_csproxy;
	bool g_csproxycreated = false;
	boost::mutex g_csproxyMutex;

	CsCommunicationImplPtr CsCommunicationImpl::Instance()
	{
		if (!g_csproxycreated) {
			boost::mutex::scoped_lock guard(g_csproxyMutex);
			if (!g_csproxycreated) {
				g_csproxy.reset(new CsCommunicationImpl());
				g_csproxycreated = true;
			}
		}

		return g_csproxy;
	}

	void CsCommunicationImpl::Initialize()
	{
		if (!g_csproxycreated) {
			boost::mutex::scoped_lock guard(g_csproxyMutex);
			if (!g_csproxycreated) {
				g_csproxy.reset(new CsCommunicationImpl());
				g_csproxycreated = true;
			}
		}
	}

	CsCommunicationImpl::CsCommunicationImpl()
		: m_csip(CsPushConfig::Instance()->csip()),
		m_csport(CsPushConfig::Instance()->csport()),
		m_hostid(CsPushConfig::Instance()->hostid()),
		m_secure(CsPushConfig::Instance()->secure()),
		m_tal(m_csip, m_csport, m_secure)
	{
		if (m_hostid.empty()) {
			m_hostid = "NEW HOST ID";
		}

		m_csRetries = CsPushConfig::Instance()->CSRetries();
		m_csRetryInterval = CsPushConfig::Instance()->CSRetryIntervalInSecs();

		std::string biosId;
		m_dpc =
			GetBiosId(biosId) ?
			marshalCxCall("::getVxAgent", m_hostid, biosId) :
			marshalCxCall("::getVxAgent", m_hostid);
	}

	PushInstallationSettings CsCommunicationImpl::GetSettings()
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		PushInstallationSettings currentSettings;

		std::string debug = m_tal(marshalCxCall(m_dpc, "getInstallHosts"));
		//DebugPrintf(SV_LOG_DEBUG, "before unmarshalling %s\n", debug.c_str()) ;
		currentSettings = unmarshal<PushInstallationSettings>(debug);

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return currentSettings;
	}

	PUSH_INSTALL_JOB CsCommunicationImpl::GetInstallDetails(
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

		int attempt = 1;
		bool csupdated = false;

		while (!csupdated)
		{

			try {

				std::string debug =
					m_tal(marshalCxCall(
						m_dpc,
						GET_INSTALL_TASKS_PUSH_CLIENT,
						jobId,
						"NEWHOSTID",
						osName));

				job = unmarshal<PUSH_INSTALL_JOB>(debug);

				for (INSTALL_TASK& task : job.m_installTasks)
				{
					if (osName == "Win")
					{
						if (task.type_of_install == INSTALL_TYPE::FRESH_INSTALL)
						{
							task.m_commandLineOptions += " /InstallationType Install /CSType CSLegacy ";
						}
						else if (task.type_of_install == INSTALL_TYPE::UPGRADE_INSTALL)
						{
							task.m_commandLineOptions += " /InstallationType Upgrade /CSType CSLegacy ";
						}
					}
					else
					{
						if (task.type_of_install == INSTALL_TYPE::FRESH_INSTALL)
						{
							task.m_commandLineOptions += " -a Install -c CSLegacy";
						}
						else if (task.type_of_install == INSTALL_TYPE::UPGRADE_INSTALL)
						{
							task.m_commandLineOptions += " -a Upgrade -c CSLegacy";
						}
					}
				}

				csupdated = true;
			}
			catch (ContextualException & ee) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION 
						<< "communication failure with configuration server - attempt :" << attempt
						<< " GetInstallDetails api for job " << jobId << "os: " << osName 
						<< " failed with exception " << ee.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  os: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					osName.c_str());

			}
			catch (std::exception & e) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION 
						<< "communication failure with configuration server - attempt :" << attempt
						<< " GetInstallDetails api for job " << jobId << "os: " << osName 
						<< " failed with exception " << e.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  os: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					osName.c_str());

			}
		}

		DebugPrintf(
			SV_LOG_DEBUG,
			"EXITED %s jobid: %s  os: %s\n",
			__FUNCTION__,
			jobId.c_str(),
			osName.c_str());
		return job;
	}

	bool CsCommunicationImpl::UpdateAgentInstallationStatus(
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
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		int retVal = -1;

		DebugPrintf(SV_LOG_DEBUG, "Status Updation Details \n");
		DebugPrintf(SV_LOG_DEBUG, "------------------------\n");
		DebugPrintf(SV_LOG_DEBUG, "JobId: %s \n", jobId.c_str());
		DebugPrintf(SV_LOG_DEBUG, "State: %d \n", state);
		DebugPrintf(SV_LOG_DEBUG, "Status Code: %s \n", errorCode.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Status Message: %s \n", statusString.c_str());
		DebugPrintf(SV_LOG_DEBUG, "------------------------\n");

		int attempt = 1;
		bool csupdated = false;
		std::string id = hostId;
		if (id.empty()) {
			id = m_hostid;
		}

		// note: not throwing exception, since we want this error to be ignored
		// we need to persist status on-disk and have a seperate thread to post the information to cs
		// when cs is reachable
		while (!csupdated) {

			try	{
				std::string debug = m_tal(marshalCxCall(m_dpc,
					SET_INSTALL_STATUS_PUSH_CLIENT,
					jobId,
					state,
					statusString,
					id, log,
					errorCode,
					placeHolders,
					installerExtendedErrorsJson));
				retVal = unmarshal<int>(debug);
				csupdated = true;
			}
			catch (ContextualException & ee) {

				if (attempt > m_csRetries) {
					DebugPrintf(
						SV_LOG_ERROR,
						"\ncommunication failure with configuration server - attempt:%d cs api %s for jobid: %s  status: %s encountered exception %s.\n",
						attempt,
						SET_INSTALL_STATUS_PUSH_CLIENT,
						jobId.c_str(),
						statusString.c_str(),
						ee.what());

					break;
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  status: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					statusString.c_str());

			}
			catch (std::exception & e) {

				if (attempt > m_csRetries) {
					DebugPrintf(
						SV_LOG_ERROR,
						"\ncommunication failure with configuration server - attempt:%d cs api %s for jobid: %s  status: %s encountered exception %s.\n",
						attempt,
						SET_INSTALL_STATUS_PUSH_CLIENT,
						jobId.c_str(),
						statusString.c_str(),
						e.what());

					break;
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  status: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					statusString.c_str());

			}
		}

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return csupdated;
	}

	std::string CsCommunicationImpl::GetPassphrase()
	{
		return securitylib::getPassphrase();
	}

	std::string CsCommunicationImpl::GetPassphraseFilePath()
	{
		return securitylib::getPassphraseFilePath();
	}

	bool CsCommunicationImpl::RegisterPushInstaller()
	{
		//DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		char szHostName[50];
		bool bRet = false;
		gethostname(szHostName, sizeof(szHostName));
		std::string hostname = szHostName;
		//DebugPrintf(SV_LOG_DEBUG, "Cx ip is %s\n", m_csip.c_str());
#ifdef SV_WINDOWS
		int osType = 1;
#else
		int osType = 2;
#endif

		// note: no  retries and ignoring failures, as this is periodic call
		try
		{
			std::string biosId;
			if (GetBiosId(biosId)) {
				(void)m_tal(marshalCxCall(
					"::registerPushProxy",
					m_hostid,
					Host::GetInstance().GetIPAddress(),
					hostname,
					osType,
					biosId));
			}
			else {
				(void)m_tal(marshalCxCall(
					"::registerPushProxy",
					m_hostid,
					Host::GetInstance().GetIPAddress(),
					hostname,
					osType));
			}
			bRet = true;
		}
		catch (ContextualException& ce)
		{
			DebugPrintf(SV_LOG_DEBUG, "Registering push Proxy failed. Error: %s\n", ce.what());
		}
		//DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return bRet;
	}

	bool CsCommunicationImpl::UnregisterPushInstaller()
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
		bool bRet = false;

		// note: no  retries and ignoring failures, as this is periodic call
		try
		{
			std::string biosId;
			if (GetBiosId(biosId)) {
				(void)m_tal(marshalCxCall("::unregisterPushProxy", m_hostid, biosId));
			}
			else {
				(void)m_tal(marshalCxCall("::unregisterPushProxy", m_hostid));
			}
			bRet = true;
		}
		catch (ContextualException& ce)
		{
			DebugPrintf(SV_LOG_ERROR, "Unregistering push Proxy failed. Eroor: %s\n", ce.what());
		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
		return bRet;
	}


	std::string CsCommunicationImpl::GetPushClientDetails(const std::string & jobId, const std::string& osName)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s jobid: %s  os: %s\n",
			__FUNCTION__,
			jobId.c_str(),
			osName.c_str());

		PUSH_INSTALL_CLIENT_DOWNLOAD_PATH piclient_path;
		std::string url;
		int attempt = 1;
		bool csupdated = false;

		while (!csupdated)
		{

			try {

				std::string debug = 
					m_tal(marshalCxCall(m_dpc, GET_PUSHINSTALLER_CLIENT_PATH, osName, jobId));
				//DebugPrintf(SV_LOG_DEBUG, "Before unmarshalling %s\n", debug.c_str()) ;
				piclient_path = unmarshal<PUSH_INSTALL_CLIENT_DOWNLOAD_PATH>(debug);
				url = piclient_path.m_PIClient_url;
				csupdated = true;
			}
			catch (ContextualException & ee) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION <<
						"communication failure with configuration server - attempt :" << attempt
						<< " " << GET_PUSHINSTALLER_CLIENT_PATH << " api failed with exception "
						<< ee.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  os: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					osName.c_str());

			}
			catch (std::exception & e) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION <<
						"communication failure with configuration server - attempt :" << attempt
						<< " " << GET_PUSHINSTALLER_CLIENT_PATH << "  api failed with exception "
						<< e.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  os: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					osName.c_str());

			}
		}

		DebugPrintf(
			SV_LOG_DEBUG,
			"EXITED %s jobid: %s  os: %s\n",
			__FUNCTION__,
			jobId.c_str(),
			osName.c_str());
		return url;
	}


	std::string CsCommunicationImpl::GetBuildPath(const std::string & jobId, const std::string& osName)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s jobid: %s  os: %s\n",
			__FUNCTION__,
			jobId.c_str(),
			osName.c_str());

		BUILD_DOWNLOAD_PATH build_path;
		std::string url;
		int attempt = 1;
		bool csupdated = false;

		while (!csupdated)
		{

			try {

				std::string debug = m_tal(marshalCxCall(m_dpc, GET_BUILD_PATH, osName, jobId));
				//DebugPrintf(SV_LOG_DEBUG, "Before unmarshalling %s\n", debug.c_str()) ;
				build_path = unmarshal<BUILD_DOWNLOAD_PATH>(debug);
				url = build_path.m_Build_url;

				if (url == "BuildIsNotAvailableAtCX")
				{
					throw MobilityAgentNotFoundException(osName);
				}

				csupdated = true;
			}
			catch (MobilityAgentNotFoundException) {
				throw;
			}
			catch (ContextualException & ee) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION
						<< "Either there is a communication failure with configuration server" 
						<< "or there is no suitable installer available at configuration server"
						<< "to download. - attempt :"
						<< attempt
						<< " " << GET_BUILD_PATH << " api failed with exception "
						<< ee.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  os: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					osName.c_str());

			}
			catch (std::exception & e) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION
						<< "Either there is a communication failure with configuration server or "
						<< "there is no suitable installer available at configuration server to "
						<< "download. - attempt :"
						<< attempt << " " << GET_BUILD_PATH << "  api failed with exception "
						<< e.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s jobid: %s  os: %s\n",
					attempt,
					__FUNCTION__,
					jobId.c_str(),
					osName.c_str());

			}
		}

		DebugPrintf(
			SV_LOG_DEBUG,
			"EXITED %s jobid: %s  os: %s\n",
			__FUNCTION__,
			jobId.c_str(),
			osName.c_str());
		return url;

	}

	struct httpFile
	{
		std::string m_fileName;
		FILE * stream;

		httpFile(const std::string& localFileName)
			:m_fileName(localFileName), stream(0)
		{

#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)
			stream = _wfopen(getLongPathName(localFileName.c_str()).c_str(), ACE_TEXT("wbN"));
#else
			stream = fopen(localFileName.c_str(), "wb");
#endif

		}

		~httpFile()
		{
			if (stream != NULL)
				fclose(stream);
		}
	};

	static size_t download_func(void *buffer, size_t size, size_t nmemb, void *stream)
	{
		FILE *lfstream = static_cast<FILE *>(stream);
		return fwrite(buffer, size, nmemb, lfstream);
	}

	void httpdownload(const std::string& url, const std::string& localFileName)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s %s %s \n",
			__FUNCTION__,
			url.c_str(),
			localFileName.c_str());
		struct httpFile http_file(localFileName);

		if (!http_file.stream) {
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION
				<< "open " << localFileName << " failed with error " << lastKnownError()
				<< "(" << err << ")";
		}

		CurlOptions options(url);
		options.writeCallback(http_file.stream, download_func);
		options.validResponseRange(200, 299);

		CurlWrapper cw;
		cw.post(options, std::string());

		DebugPrintf(SV_LOG_DEBUG, "Download of %s finished\n", url.c_str());
	}


	void CsCommunicationImpl::DownloadInstallerFile(const std::string& path, const std::string& localFilePath)
	{
		DebugPrintf(
			SV_LOG_DEBUG,
			"ENTERED %s %s %s \n",
			__FUNCTION__,
			path.c_str(),
			localFilePath.c_str());

		std::stringstream url;
		if (m_secure) {
			url << "https://" << m_csip << ":" << m_csport << "/" << path;
		}
		else {
			url << "http://" << m_csip << ":" << m_csport << "/" << path;
		}

		int attempt = 1;
		bool done = false;

		while (!done)
		{

			try {

				httpdownload(url.str(), localFilePath);
				done = true;
			}
			catch (ContextualException & ee) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION << "communication failure with configuration server - attempt :" 
						<< attempt
						<< " download api failed with exception "
						<< ee.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s url: %s  local path: %s\n",
					attempt,
					__FUNCTION__,
					url.str().c_str(),
					localFilePath.c_str());

			}
			catch (std::exception & e) {

				if (attempt > m_csRetries) {
					throw  ERROR_EXCEPTION << "communication failure with configuration server - attempt :" 
						<< attempt
						<< " download api failed with exception "
						<< e.what();
				}

				++attempt;
				CDPUtil::QuitRequested(m_csRetryInterval);
				DebugPrintf(
					SV_LOG_DEBUG,
					"Attempt:%d %s url: %s  local path: %s\n",
					attempt,
					__FUNCTION__,
					url.str().c_str(),
					localFilePath.c_str());

			}
		}

		DebugPrintf(
			SV_LOG_DEBUG,
			"EXITED %s %s %s \n",
			__FUNCTION__,
			path.c_str(),
			localFilePath.c_str());
	}

	std::string CsCommunicationImpl::csip() const
	{
		return m_csip;
	}

	int CsCommunicationImpl::csport() const
	{
		return m_csport;
	}
}



#endif