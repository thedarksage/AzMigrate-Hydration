#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <atlbase.h>
#include "logger.h"
#include "confsettings.h"
#include "servicemgr.h"
#include "DrRegister.h"
#include "passphraseprocessor.h"
#include "confpropertysettings.h"
#include "initialconfigupdater.h"
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmsafecapis.h"

#include <boost/thread.hpp>
boost::mutex g_MutexFortheConfPathInformer;

ConfPathsInformer* theConfPathInformer = NULL;

bool GetProgramDataDir(std::string& programDataDir);
bool GetCSPSInstallPath(std::string& cspsInstallPath, std::stringstream& errmsg, bool isLoggerSet = true);
bool GetPushInstallPath(std::string& pushInstallPath, char** err, bool isLoggerSet = true);
UINT GetScoutAgentInstallCode();
bool IsAgentInstalled();
bool GetAgentInstallPath(std::string& agentInstallPath, std::stringstream& errmsg, bool isLoggerSet = true);
void EndLogging();
void Init(std::string& amethystConfFilePath, std::string& piConfFilePath, std::string& psCXPSConfFilePath, std::string& psCXPSConfPath,
	std::string& drscoutConfFilePath, std::string& agentCXPSConfFilePath, std::string& cspsInstallPath, std::string& agentInstallPath, bool& isMTInstalled);

ConfPathsInformer::ConfPathsInformer()
{
	std::string amethystConfFilePath, piConfFilePath, psCXPSConfFilePath, psCXPSConfPath, drscoutConfFilePath, agentCXPSConfFilePath, cspsInstallPath, agentInstallPath;
	bool isMTInstalled;

	try{
		Init(amethystConfFilePath, piConfFilePath, psCXPSConfFilePath, psCXPSConfPath, drscoutConfFilePath, agentCXPSConfFilePath, cspsInstallPath, agentInstallPath, isMTInstalled);
	}
	catch (std::exception& e)
	{
		throw e;
	}

	m_amethyst_conf_file_path = amethystConfFilePath;

	m_pushinstall_conf_file_path = piConfFilePath;

	m_ps_cxps_conf_file_path = psCXPSConfFilePath;

	m_ps_cxps_conf_path = psCXPSConfPath;

	m_drscout_conf_file_path = drscoutConfFilePath;

	m_agent_cxps_conf_file_path = agentCXPSConfFilePath;

	m_csps_install_path = cspsInstallPath;

	m_agent_install_path = agentInstallPath;

	m_is_mt_installed = isMTInstalled;
}

std::string ConfPathsInformer::AmethystConfPath(){ return m_amethyst_conf_file_path; }

std::string ConfPathsInformer::PIConfPath(){ return m_pushinstall_conf_file_path; }

std::string ConfPathsInformer::CXPSConfPathOnPS(){ return m_ps_cxps_conf_file_path; }

std::string ConfPathsInformer::CXPSInstallPathOnPS(){ return m_ps_cxps_conf_path; }

std::string ConfPathsInformer::DrScoutConfPath(){ return m_drscout_conf_file_path; }

std::string ConfPathsInformer::CXPSConfPathOnMT(){ return m_agent_cxps_conf_file_path; }

std::string ConfPathsInformer::CSPSInstallPath(){ return m_csps_install_path; }

std::string ConfPathsInformer::MTInstallPath(){ return m_agent_install_path; }

bool ConfPathsInformer::IsMTInstalled(){ return m_is_mt_installed; }

void ConfPathsInformer::InitializeInstance()
{
	boost::mutex::scoped_lock guard(g_MutexFortheConfPathInformer);
	if (NULL == theConfPathInformer)
	{
		theConfPathInformer = new ConfPathsInformer();
	}
}

bool GetProgramDataDir(std::string& programDataDir)
{
	bool haveProgramData = true;

	const char* programData = "%ProgramData%";
	std::vector<char> buffer(260);
	size_t size = ExpandEnvironmentStrings(programData, &buffer[0], (DWORD)buffer.size());
	if (size > buffer.size()) {
		buffer.resize(size + 1);
		size = ExpandEnvironmentStrings(programData, &buffer[0], (DWORD)buffer.size());
		if (size > buffer.size()) {
			haveProgramData = false;
		}
	}
	buffer[size] = '\0';
	if (haveProgramData) {
		programDataDir.assign(&buffer[0]);
	}
	return haveProgramData;
}

bool GetCSPSInstallPath(std::string& cspsInstallPath, std::stringstream& errmsg, bool isLoggerSet)
{
	bool ret = false;
	std::string installerConfPath;
	std::string programDataDir;
	if (GetProgramDataDir(programDataDir))
	{
		std::string confFile = programDataDir + "\\" + SV_CSPS_INSTALLER_CONF_PATH;

		fstream file(confFile.c_str());
		if (!file)
		{
			errmsg << " File Not Found : " << confFile.c_str() << "\n";
			return ret;
		}

		std::string line;

		while (getline(file, line))
		{
			if ('#' == line[0] || '[' == line[0] || line.find_first_not_of(' ') == line.npos)
			{
				continue;
			}
			else
			{
				std::string::size_type idx = line.find_first_of("=");
				if (std::string::npos != idx) {
					if (' ' == line[idx - 1]) {
						--idx;
					}
					std::string key = line.substr(0, idx);
					Trim(key, " ");
					if (boost::iequals(key," "), SV_CSPS_INSTALLDIRECTORY_KEY)
					{
						if (' ' == line[idx])
						{
							++idx;
						}
						cspsInstallPath = line.substr(idx + 1);
						Trim(cspsInstallPath, " \"");
						ret = true;
					}
				}
			}
		}
		if (!ret)
		{
			errmsg << "Could not find value for key: " << SV_CSPS_INSTALLDIRECTORY_KEY << " in " << confFile;
			if (isLoggerSet)
			{
				DebugPrintf(SV_LOG_ERROR, "%s.\n", errmsg.str().c_str());
			}
		}
	}
	else
	{
		if (isLoggerSet)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to determine ProgramData directory.\n");
		}
	}

	return ret;
}

bool GetPushInstallPath(std::string& pushInstallPath, char** err, bool isLoggerSet)
{
	bool ret = true;
	unsigned long bufferSize = 0;
	std::stringstream errmsg;
	CRegKey cregkey;
	DWORD result;
	result = cregkey.Open(HKEY_LOCAL_MACHINE, "SOFTWARE\\SV Systems\\PushInstaller");

	if (ERROR_SUCCESS != result)
	{
		errmsg << "Failed to open PushInstaller reg key";
		if (isLoggerSet)
		{
			DebugPrintf(SV_LOG_ERROR, "%s.\n", errmsg.str().c_str());
		}
		bufferSize = errmsg.str().length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
		return false;
	}

	char path[BUFSIZ];
	DWORD dwCount = sizeof(path);
	result = cregkey.QueryStringValue("InstallDirectory", path, &dwCount);
	cregkey.Close();
	if (ERROR_SUCCESS != result)
	{
		errmsg << "Failed to query InstallDirectory from PushInstaller reg key";
		if (isLoggerSet)
		{
			DebugPrintf(SV_LOG_ERROR, "%s.\n", errmsg.str().c_str());
		}
		bufferSize = errmsg.str().length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
		return false;
	}
	pushInstallPath = path;
	if (isLoggerSet)
	{
		DebugPrintf(SV_LOG_ERROR, "PushInstallDirectory: %s.\n", pushInstallPath.c_str());
	}

	if (pushInstallPath.empty())
	{
		errmsg << "InstallDirectory reg key's value is empty in pushinstall registry";
		if (isLoggerSet)
		{
			DebugPrintf(SV_LOG_ERROR, "%s.\n", errmsg.str().c_str());
		}
		ret = false;
	}

	if (!ret)
	{
		bufferSize = errmsg.str().length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
		return false;
	}
	return true;
}

UINT GetScoutAgentInstallCode()
{
	UINT scoutAgentInstallCode = 0;
	HKEY scoutAgentKey;

	//Check for VX agent installation
	LSTATUS scoutAgentKeyStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\InMage Systems\\Installed Products\\5", 0, KEY_READ, &scoutAgentKey);
	if (scoutAgentKey != NULL)
	{
		scoutAgentInstallCode = SCOUT_VX_ISTALLATION_REG_KEY;
		RegCloseKey(scoutAgentKey);
	}

	return scoutAgentInstallCode;
}

bool IsAgentInstalled()
{
	bool ret = false;
	if (GetScoutAgentInstallCode() == 5)
	{
		ret = true;
	}

	return ret;
}
bool GetAgentInstallPath(std::string& agentInstallPath, std::stringstream& errmsg, bool isLoggerSet)
{
	bool ret = true;
	try{
		LocalConfigurator lc;
		agentInstallPath = lc.getInstallPath();
	}
	catch (std::exception& e)
	{
		errmsg << "Could not find the agent installation path from drscout.conf file. Exception: " << e.what();
		if (isLoggerSet)
		{
			DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
		}
		ret = false;
	}
	return ret;
}

void EndLogging()
{
    CloseDebug();
}

void Init(std::string& amethystConfFilePath, std::string& piConfFilePath, std::string& psCXPSConfFilePath, std::string& psCXPSConfPath,
	std::string& drscoutConfFilePath, std::string& agentCXPSConfFilePath, std::string& cspsInstallPath, std::string& mt_agentInstallPath, bool& isMTInstalled)
{
	unsigned long bufferSize = 0;
	std::string cspsInstallationPath;
	std::string pushInstallPath;
	std::stringstream errmsg;
	if (GetCSPSInstallPath(cspsInstallationPath, errmsg, false))
	{
		if (!cspsInstallationPath.empty())
		{
			amethystConfFilePath = cspsInstallationPath + std::string("\\") + SV_AMETHYST_CONF_FILE;
			psCXPSConfFilePath = cspsInstallationPath + std::string("\\") + SV_PS_CXPS_CONF_FILE;
			cspsInstallPath = cspsInstallationPath;
			psCXPSConfPath = cspsInstallationPath + std::string("\\") + SV_PS_CXPS_CONF_DIR;

			char** err;

			if (!GetPushInstallPath(pushInstallPath, err, false))
			{
				std::stringstream emsg;
				emsg << err;
				throw std::exception(emsg.str().c_str());
			}
			else
			{
				piConfFilePath = pushInstallPath + std::string("\\") + SV_PUSH_CONF_FILE;
			}

			if (IsAgentInstalled())
			{
				std::string agentRole = "";
				if (!GetAgentRole(agentRole, err))
				{
					std::stringstream emsg;
					emsg << err;
					throw std::exception(emsg.str().c_str());
				}
				else
				{
					if (agentRole.compare(MASTER_TARGET_ROLE) == 0)
					{
						isMTInstalled = true;
					}
				}
				std::string agentInstallPath = "";
				if (GetAgentInstallPath(agentInstallPath, errmsg))
				{
					if (!agentInstallPath.empty())
					{
						mt_agentInstallPath = agentInstallPath;
						drscoutConfFilePath = agentInstallPath + std::string("\\") + SV_DRSCOUT_CONF_FILE;
						agentCXPSConfFilePath = agentInstallPath + std::string("\\") + SV_AGENT_CXPS_CONF_FILE;
					}
					else
					{
						errmsg << "Failed to determine agent installation path";
						throw std::exception(errmsg.str().c_str());
					}
				}
				else
				{
					throw std::exception(errmsg.str().c_str());
				}
			}
		}
		else
		{
			errmsg << "Failed to determine CSPS installation path. May be the value for the key " << SV_CSPS_INSTALLDIRECTORY_KEY << " is empty in <PragramData>\\" << SV_CSPS_INSTALLER_CONF_PATH;
			throw  std::exception(errmsg.str().c_str());
		}
	}
	else
	{
		errmsg << ". Hence failed to determine csps install path.";
		throw  std::exception(errmsg.str().c_str());
	}
}

void StartLogging(const std::string& cspsInstallationPath)
{
	std::string logFilePath = cspsInstallationPath + "\\home\\svsystems\\var\\configtool.log";
	SetLogFileName(logFilePath.c_str());
	SetLogLevel(7);
}
void Closing()
{
    EndLogging();
}

void freeAllocatedBuf(unsigned char** allocatedBuff) {
    if (allocatedBuff)
    {
        delete[](*allocatedBuff);
    }
}
int StopServices(char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
    
	boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()));
    std::string cxtype;
    std::list<std::string> services;
    std::stringstream errmsg;

    if (!cvo->getValue("CX_TYPE", cxtype,errmsg))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to determine installation type as key CX_TYPE. Error: %s.\n", errmsg.str().c_str());
        bufferSize = errmsg.str().length();
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
        DebugPrintf(SV_LOG_ERROR, "Failed to stop services : %s.\n", *err);
        rv = SV_FAIL;
    }
    else
    {
        boost::shared_ptr<ServiceMgr> cmgr(new ServiceMgr());
		cmgr->getServiceList(cxtype, services,/*SV_MTAGENT_INSTALLED*/theConfPathInformer->IsMTInstalled());
        rv = cmgr->stopservices(services, errmsg);
        if (rv != SVS_OK)
        {
            bufferSize = errmsg.str().length();
            size_t buflen;
            INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
            *err = new char[buflen];
            inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
            DebugPrintf(SV_LOG_ERROR, "Failed to stop services : %s.\n", *err);
        }
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully stopped all services\n");
		}
    }
   
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
    return rv;
}
int StartServices(char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	} 
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
	boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()));
    std::string cxtype;
    std::list<std::string> services;
    std::stringstream errmsg;

    Sleep(5000);
    if (!cvo->getValue("CX_TYPE", cxtype, errmsg))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to determine installation type as key CX_TYPE does not exist.\n");
        rv = SV_FAIL;
    }
    else
    {
        boost::shared_ptr<ServiceMgr> cmgr(new ServiceMgr());
		cmgr->getServiceList(cxtype, services,/*SV_MTAGENT_INSTALLED*/theConfPathInformer->IsMTInstalled());
        rv = cmgr->startservices(services, errmsg);
        if (rv != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to start services err : %s.\n", errmsg.str().c_str());
            bufferSize = errmsg.str().length();
            size_t buflen;
            INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
            *err = new char[buflen];
            inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
            DebugPrintf(SV_LOG_ERROR, "%s : Failed to start services.\n", *err);
        }
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully started all services\n");
		}
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
    return rv;
}

int UpdatePropertyValues(char* amethystReqStream, char* pushReqStream, char* cxpsRequestStream, char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
	std::string amethystreq = amethystReqStream;
	std::string pushreq = pushReqStream;
	std::string cxpsreq = cxpsRequestStream;
	std::string resp;
    std::stringstream errmsg;

	if (amethystReqStream && pushReqStream && cxpsRequestStream)
	{
		DebugPrintf(SV_LOG_DEBUG, "Amethyst Input key,value %s\n", amethystReqStream);
		DebugPrintf(SV_LOG_DEBUG, "Push Input key,value %s\n", pushReqStream);
		DebugPrintf(SV_LOG_DEBUG, "cxps Input key,value %s\n", cxpsRequestStream);

		std::vector<std::string> apairs, ppairs, cpairs;
		std::vector<std::string>::const_iterator apairIter, ppairIter, cpairIter;
		Tokenize(amethystreq, apairs, ";");

        KeyVal_t amap;
		apairIter = apairs.begin();
		while (apairIter != apairs.end())
        {
            std::vector<std::string> tokens;
            Tokenize(*apairIter, tokens, "=");
            amap.insert(std::make_pair(tokens.front(), tokens.back()));
            apairIter++;
        }

        boost::shared_ptr<ConfValueObject> acvo(new ConfValueObject(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()));

        if (!acvo->update(amap, errmsg))
        {
			resp = errmsg.str();
			bufferSize = resp.length();
			DebugPrintf(SV_LOG_ERROR, "Failed to Update in %s KEY Value : %s.\n", theConfPathInformer->AmethystConfPath().c_str(), resp.c_str());
			rv = SV_FAIL;
		}
		else
		{
            std::string csip, port;
			KeyVal_t pmap;
			Tokenize(pushreq, ppairs, ";");
			ppairIter = ppairs.begin();
			while (ppairIter != ppairs.end())
			{
				std::vector<std::string> tokens;
				Tokenize(*ppairIter, tokens, "=");
				pmap.insert(std::make_pair(tokens.front(), tokens.back()));
				ppairIter++;
			}
			KeyValIter_t ipit = pmap.find("Hostname");

			if (pmap.end() != ipit)
			{
				csip = ipit->second;
			}

			KeyValIter_t portit = pmap.find("Port");
			if (pmap.end() != portit)
			{
				port = portit->second;
			}

			boost::shared_ptr<ConfValueObject> pcvo(new ConfValueObject(/*SV_PUSHINSTALL_CONF_FILE_PATH*/theConfPathInformer->PIConfPath()));

			if (!pcvo->update(pmap, errmsg))
			{
				resp = std::string("Successfully updated ip and port in ") + /*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()
					+ std::string(". But failed to update in ") + /*SV_PUSHINSTALL_CONF_FILE_PATH */theConfPathInformer->PIConfPath()
								   + std::string(", Error: ") + errmsg.str();
				bufferSize = resp.length();
				DebugPrintf(SV_LOG_ERROR, "%s.\n",resp.c_str());
				rv = SV_FAIL;
			}
			else
			{
				Tokenize(cxpsreq, cpairs, "=");

				KeyVal_t cmap;
				cpairIter = cpairs.begin();
				while (cpairIter != cpairs.end())
				{
					cmap.insert(std::make_pair(cpairs.front(), cpairs.back()));
					DebugPrintf(SV_LOG_DEBUG, "cxps Input key %s,value %s\n", cpairs.front().c_str(), cpairs.back().c_str());
					cpairIter++;
				}

				std::string sslport = "";
				KeyValIter_t sslportit = cmap.find("sslport");

				if (cmap.end() != sslportit)
				{
					sslport = sslportit->second;
				}

				if (!csip.empty() && !port.empty() && !sslport.empty())
				{
					DebugPrintf(SV_LOG_DEBUG, "updating cxps.conf \n");
					


					std::string cmd = std::string("\"") + /*SV_PS_CXPS_CONF_PATH*/theConfPathInformer->CXPSInstallPathOnPS() + std::string("\\") + "cxpscli.exe" + std::string("\"");
					cmd += std::string(" ") + "--conf" + std::string(" ") + std::string("\"") + /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() + std::string("\"");
					cmd += std::string(" ") + "--csip" + std::string(" ") + csip;
					cmd += std::string(" ") + "--cssslport" + std::string(" ") + port;
					cmd += std::string(" ") + "--sslport" + std::string(" ") + sslport;

					DebugPrintf(SV_LOG_DEBUG, "Issuing Command : %s\n", cmd.c_str());

					std::string emsg;

					if (!execProcUsingPipe(cmd, emsg))
					{
						resp = std::string("Successfully updated ip and port in ") + /*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()
							+ std::string(" and ") + /*SV_PUSHINSTALL_CONF_FILE_PATH */theConfPathInformer->PIConfPath() + std::string(".But failed to update in ")
							+ /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() + std::string(", Error: ") + emsg;
						bufferSize = resp.length();
						rv = SV_FAIL;
						DebugPrintf(SV_LOG_ERROR, "%s\n", resp.c_str());
					}
					else if (/*SV_MTAGENT_INSTALLED*/theConfPathInformer->IsMTInstalled())
					{

						// Updating Agent settings for MT(drscout.conf and agent's cxps.conf) if installed
						if (!UpdateMTAgentSettings(csip, port, sslport, errmsg, true))
						{
							resp = std::string("Successfully updated ip , port and sslport in ") + /*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()
								+ std::string(",") + /*SV_PUSHINSTALL_CONF_FILE_PATH */theConfPathInformer->PIConfPath() + std::string("and") + /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() +
								std::string(".But failed to update in MTAgentSettings. Error: ") + errmsg.str().c_str();
							bufferSize = resp.length();
							rv = SV_FAIL;
							DebugPrintf(SV_LOG_ERROR, "%s\n", resp.c_str());
						}
					}

					DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
				}
				else
				{
					resp = std::string("Successfully updated ip and port in ") + /*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()
						+ std::string(" and ") + /*SV_PUSHINSTALL_CONF_FILE_PATH */theConfPathInformer->PIConfPath() + std::string(".But failed to update in ")
						+ /*SV_AGENT_CXPS_CONF_FILE_PATH */theConfPathInformer->CXPSConfPathOnMT() + " , " + /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() + " and " + /*SV_DRSCOUT_CONF_FILE_PATH*/theConfPathInformer->DrScoutConfPath()
						+ "Error: Either csip or cssslport or sslport input value to update on all those files is/are empty.";
					bufferSize = resp.length();
					rv = SV_FAIL;
					DebugPrintf(SV_LOG_ERROR, "%s\n", resp.c_str());
				}
				
			}
		}
    }
    else
    {
        errmsg << "NullCommand Input.";
		resp = errmsg.str();
		bufferSize = resp.length();
        DebugPrintf(SV_LOG_ERROR, "Failed to update. Error: %s.\n", resp.c_str());
        rv = SV_FAIL;
    }
	if (rv == SV_FAIL)
	{
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
		*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), resp.c_str());
		DebugPrintf(SV_LOG_ERROR, "Failed to update Property values. Error: %s.\n", *err);
	}

    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
    return rv;
}

void UpdateAmethyst(KeyVal_t amap)
{
	boost::shared_ptr<ConfValueObject> acvo(new ConfValueObject(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()));
	std::stringstream errmsg;
	if (!acvo->update(amap, errmsg))
	{
		std::string resp = "Failed to update input key value to amethyst.conf. Error: " + errmsg.str();
		DebugPrintf(SV_LOG_ERROR, "%s.\n", resp.c_str());
		throw std::exception(resp.c_str());
	}
}

void UpdateSSLPortInPSCSPXConf(const std::string& sslport)
{
	std::string cmd = std::string("\"") + /*SV_PS_CXPS_CONF_PATH*/theConfPathInformer->CXPSInstallPathOnPS() + std::string("\\") + "cxpscli.exe" + std::string("\"");
	cmd += std::string(" ") + "--conf" + std::string(" ") + std::string("\"") + /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() + std::string("\"");
	cmd += std::string(" ") + "--sslport" + std::string(" ") + sslport;

	DebugPrintf(SV_LOG_DEBUG, "Issuing Command : %s\n", cmd.c_str());

	std::string emsg;

	if (!execProcUsingPipe(cmd, emsg))
	{
		std::string resp = std::string("Successfully updated DVACP_PORT in ") + /*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()
			+ std::string(".But failed to update in ")
			+ /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() + std::string(", Error: ") + emsg;
		DebugPrintf(SV_LOG_ERROR, "%s\n", resp.c_str());
		throw std::exception(resp.c_str());
	}
}

int UpdateConfFilesInCS(char* amethystReqStream, char* cxpsReqStream, char* azureCsStream, char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	int rv = SV_OK;
	std::string amethystreq = amethystReqStream;
	std::string cxpsreq = cxpsReqStream;
	std::string azurecsipreq = azureCsStream;
	std::string resp;
	std::stringstream errmsg;

	if (amethystReqStream && cxpsReqStream && azureCsStream)
	{
		DebugPrintf(SV_LOG_DEBUG, "Amethyst Input key,value %s\n", amethystReqStream);
		DebugPrintf(SV_LOG_DEBUG, "cxps Input key,value %s\n", cxpsReqStream);
		DebugPrintf(SV_LOG_DEBUG, "Amethyst Input key,value %s\n", azureCsStream);
		std::vector<std::string> apairs, cpairs, azurecsippairs;
		std::vector<std::string>::const_iterator apairIter, cpairIter, azurecsippairsIter;

		//Creating key value pairs map to give input to ConfValueObject to update amethyst.conf
		KeyVal_t amap;
		Tokenize(amethystreq, apairs, "=");
		apairIter = apairs.begin();
		while (apairIter != apairs.end())
		{
			amap.insert(std::make_pair(apairs.front(), apairs.back()));
			DebugPrintf(SV_LOG_DEBUG, "cxps Input key %s,value %s\n", apairs.front().c_str(), apairs.back().c_str());
			apairIter++;
		}

		std::string dvacpport = "";
		KeyValIter_t sslportit = amap.find("DVACP_PORT");

		if (amap.end() != sslportit)
		{
			dvacpport = sslportit->second;
		}

		KeyVal_t azurecsmap;
		Tokenize(azurecsipreq, azurecsippairs, "=");
		azurecsippairsIter = azurecsippairs.begin();
		while (azurecsippairsIter != azurecsippairs.end())
		{
			azurecsmap.insert(std::make_pair(azurecsippairs.front(), azurecsippairs.back()));
			DebugPrintf(SV_LOG_DEBUG, "azurecsip Input key %s,value %s\n", azurecsippairs.front().c_str(), azurecsippairs.back().c_str());
			azurecsippairsIter++;
		}

		std::string azurecsip = "";
		KeyValIter_t azurecsipit = azurecsmap.find("IP_ADDRESS_FOR_AZURE_COMPONENTS");

		if (azurecsmap.end() != azurecsipit)
		{
			azurecsip = azurecsipit->second;
		}


		//getting sslport and value from cxps request stream
		std::string sslport = "";
		Tokenize(cxpsreq, cpairs, "=");

		cpairIter = cpairs.begin();
		if (cpairIter != cpairs.end())
		{
			if (cpairs.front().compare("sslport") == 0)
			{
				sslport = cpairs.back();
			}
		}

		if (!dvacpport.empty() && !sslport.empty() && !azurecsip.empty())
		{
			try
			{
				UpdateAmethyst(amap);
				UpdateAmethyst(azurecsmap);
				UpdateSSLPortInPSCSPXConf(sslport);
			}
			catch (std::exception& e)
			{
				resp = e.what();
				bufferSize = resp.length();
				rv = SV_FAIL;
			}
		}
		else
		{
			resp = "Error: Either DVACP_PORT or sslport or azurecs input value to update on all those files is / are empty.";
			bufferSize = resp.length();
			rv = SV_FAIL;
			DebugPrintf(SV_LOG_ERROR, "%s\n", resp.c_str());
		}
	}
	else
	{
		errmsg << "NullCommand Input.";
		resp = errmsg.str();
		bufferSize = resp.length();
		DebugPrintf(SV_LOG_ERROR, "Failed to update. Error: %s.\n", resp.c_str());
		rv = SV_FAIL;
	}
	if (rv == SV_FAIL)
	{
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), resp.c_str());
		DebugPrintf(SV_LOG_ERROR, "Failed to update Property value. Error: %s.\n", *err);
	}


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	Closing();
	return rv;
}

int UpdateHostGUID(char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);

    int rv = SV_OK;
    std::stringstream errmsg;
	boost::shared_ptr<InitialConfigUpdater> cvo(new InitialConfigUpdater(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath(), /*SV_PUSHINSTALL_CONF_FILE_PATH*/theConfPathInformer->PIConfPath(), /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS(), /*SV_CSPS_INSTALL_PATH*/theConfPathInformer->CSPSInstallPath()));
    if (!cvo->ReadAndUpdateHostId(errmsg))
    {
        bufferSize = errmsg.str().length();
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
        DebugPrintf(SV_LOG_ERROR, "Failed to Create and Update new HostId : %s\n",*err);
        rv = SV_FAIL;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
    return rv;
}

int UpdateIPs(char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
    std::stringstream errmsg;

	boost::shared_ptr<InitialConfigUpdater> ic(new InitialConfigUpdater(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath(), /*SV_PUSHINSTALL_CONF_FILE_PATH*/theConfPathInformer->PIConfPath(), /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS(), /*SV_CSPS_INSTALL_PATH*/theConfPathInformer->CSPSInstallPath()));
    if (!ic->ReadAndUpdateIPs(errmsg))
    {
        bufferSize = errmsg.str().length();
        rv = SV_FAIL;
    }

    if (rv == SV_FAIL)
    {
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
        DebugPrintf(SV_LOG_ERROR, "Failed to Update new IPs : %s\n", *err);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
    return rv;
}

int GetValue(const std::string& conffile,char* key,char** value,int& rv,char** err){
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    unsigned long bufferSize = 0;
    std::stringstream errmsg;

    if (key)
    {
        std::string cxtype;
        boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(conffile));
        if (!cvo->getValue(key, cxtype, errmsg))
        {
            bufferSize = errmsg.str().length();
            size_t buflen;
            INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
            *err = new char[buflen];
            inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
            DebugPrintf(SV_LOG_ERROR, "Failed to get value for key: %s. Error: %s.\n", key, *err);
            rv = SV_FAIL;
        }
        else
        {
            bufferSize = cxtype.length();
            size_t buflen;
            INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
            *value = new char[buflen];
            inm_strcpy_s(*value, (bufferSize + 1), cxtype.c_str());
            DebugPrintf(SV_LOG_DEBUG, "Successfully get value %s for the key %s\n", *value, key);
        }
    }
    else
    {
        errmsg << "NullCommand Input.";
        bufferSize = errmsg.str().length();
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
        DebugPrintf(SV_LOG_ERROR, "Failed to get value. Error: %s.\n", *err);
        rv = SV_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}
int GetValueFromAmethyst(char* key, char** value,char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			DebugPrintf(SV_LOG_DEBUG, "after INM_SAFE_ARITHMETIC\n");
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		DebugPrintf(SV_LOG_DEBUG, "after inm_strcpy_s\n");
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
    std::string conffile = /*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath();
    GetValue(conffile,key, value, rv, err);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
    return rv;
}

int GetHostName(char** hostname,char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
    std::stringstream errmsg;

    boost::shared_ptr<DRRegister> dr(new DRRegister());
    std::string host = dr->GetHostName();
    DebugPrintf(SV_LOG_DEBUG, "HostName : %s\n", host.c_str());
    if (!host.empty())
    {
        unsigned long hostbufferSize = host.length();
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(hostbufferSize) + 1, INMAGE_EX(hostbufferSize))
        *hostname = new char[buflen];
        inm_strcpy_s(*hostname, (hostbufferSize + 1), host.c_str());
        DebugPrintf(SV_LOG_DEBUG, "Returning HostName : %s to UI \n", *hostname);
    }
    else
    {
        errmsg << "failed to retrieve the host name.";
        bufferSize = errmsg.str().length();
        rv = SV_FAIL;
    }


    if (rv == SV_FAIL)
    {
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
        DebugPrintf(SV_LOG_ERROR, "Failed to Update new IPs : %s\n", *err);
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	Closing();
	return rv;
}

int GetValueFromCXPSConf(char* key, char** value, char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
	int rv = SV_OK;
	std::stringstream errmsg;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	std::string errOption;
	boost::shared_ptr<CXPSOptions> options(new CXPSOptions());
	std::string sslport = options->getOption(/*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS(), "ssl_port", errOption);
	if (!errOption.empty())
	{
		errmsg << "Failed to read ssl_port from" << /*SV_PS_CXPS_CONF_FILE_PATH*/theConfPathInformer->CXPSConfPathOnPS() << ".Error: " << errOption;
		DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
		bufferSize = errmsg.str().length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
		DebugPrintf(SV_LOG_ERROR, "%s\n", *err);
		rv = SV_FAIL;
	}
	else
	{
		if (sslport.empty())
		{
			sslport = EMPTY_STRING;//Just to retrun empty to UI if ssl_port is empty
		}
		bufferSize = sslport.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*value = new char[buflen];
		//strncpy(*value, cxtype.c_str(), (bufferSize + 1));
		inm_strcpy_s(*value, (bufferSize + 1), sslport.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Successfully get value %s for the key %s\n", *value, key);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	Closing();
	return rv;
}

int DoDRRegister(char* cmdinput,char** processexitcode,char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    DWORD exitcode;
    int rv = SV_OK;
    std::stringstream errmsg;
    unsigned long buffSize;
    std::string res;

    if (!cmdinput)
    {
        res = "NullCommand Input.";
        buffSize = res.length();
        rv = SV_FAIL;
    }
    else
    {
        std::vector<std::string> pairs;
        std::vector<std::string>::const_iterator pairIter;
        std::string req = cmdinput;
        Tokenize(req, pairs, ";");
		if (pairs.begin() == pairs.end())
        {
			res = "Invalid input from UI. Please check config logs.";
            buffSize = res.length();
            rv = SV_FAIL;
        }
        else
        {
            bool invalidarg = false;
            KeyVal_t map;
            pairIter = pairs.begin();
			DebugPrintf(SV_LOG_DEBUG, "Entered printing command input elements.\n");
            while (pairIter != pairs.end())
            {
                std::vector<std::string> tokens;
                Tokenize(*pairIter, tokens, "=");

                if (tokens.size() != 2)
                {
                    res = "Invalid input from UI. Please check config logs.";
					DebugPrintf(SV_LOG_ERROR, "Invalid key=value pair in command input.\n");
                    buffSize = res.length();
					rv = SV_FAIL;
                    invalidarg = true;
					break;
                }
                else
                {
					if ((tokens.front()).compare(PROXY_PASSWORD) == 0)
					{
						DebugPrintf(SV_LOG_DEBUG, "pairiter element %s=***\n", tokens.front().c_str());
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG, "pairiter element %s=%s\n", tokens.front().c_str(), tokens.back().c_str());
					}
                    map.insert(std::make_pair(tokens.front(), tokens.back()));
                }
                pairIter++;
            }

			DebugPrintf(SV_LOG_DEBUG, "Exited printing command input elements.\n");

            if (!invalidarg)
            {
                boost::shared_ptr<DRRegister> dr(new DRRegister());
                std::string proxytype, hostname, credentialsFile, proxyaddress, proxyport, proxyusername, proxypassword;
                KeyVal_t::iterator mapit;

				if ((mapit = map.find("proxytype")) != map.end())
					proxytype = mapit->second;
                if ((mapit = map.find("hostname")) != map.end())
                    hostname = mapit->second;
                if ((mapit = map.find("credentialsfile")) != map.end())
                    credentialsFile = mapit->second;
				if ((mapit = map.find("proxyaddress")) != map.end())
					proxyaddress = mapit->second;
				if ((mapit = map.find("proxyport")) != map.end())
					proxyport = mapit->second;
				if ((mapit = map.find("proxyusername")) != map.end())
					proxyusername = mapit->second;
				if ((mapit = map.find("proxypassword")) != map.end())
					proxypassword = mapit->second;

				if (!proxytype.empty() && !hostname.empty() && !credentialsFile.empty())
				{
					if (!dr->Register(proxytype, theConfPathInformer->CSPSInstallPath(), hostname, credentialsFile, exitcode, errmsg, proxyaddress, proxyport, proxyusername, proxypassword))
					{
						res = errmsg.str().c_str();
						buffSize = res.length();
						rv = SV_FAIL;
					}
					else
					{
						std::string st = boost::lexical_cast<std::string>(exitcode);
						buffSize = st.length();
						size_t buflen;
						INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(buffSize) + 1, INMAGE_EX(buffSize))
							*processexitcode = new char[buflen];
						inm_strcpy_s(*processexitcode, (buffSize + 1), st.c_str());
						DebugPrintf(SV_LOG_DEBUG, "Process Exit code :%s", *processexitcode);
					}
				}
                else
                {
					if (proxytype.empty())
					{
						res = "Error: proxytype did not find in command input from UI. Please check config logs.";
					}

					if (hostname.empty())
					{
						res = "Error: hostname did not find in command input from UI. Please check config logs.";
					}

					if (credentialsFile.empty())
					{
						res = "Error: credentialsFile did not find in command input from UI. Please check config logs.";
					}
                    
                    buffSize = res.length();
                    rv = SV_FAIL;
                }
            }
        }
    }
    if (rv == SV_FAIL)
    {
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(buffSize) + 1, INMAGE_EX(buffSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (buffSize + 1), res.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s", *err);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();

    return rv;
}

int GetPassphrase(char** passphrase, char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);

    std::string pphrase = "Empty";//Just to retrun empty if passphrase file does not exist

    GetPPhrase(pphrase);
    
    bufferSize = pphrase.length();
    size_t buflen;
    INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
    *passphrase = new char[buflen];
    inm_strcpy_s(*passphrase, (bufferSize + 1), pphrase.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();
	return SV_OK;
}

int ProcessPassphrase(char* req, char** err)
{
	unsigned long bufferSize = 0;
	try{
		ConfPathsInformer::InitializeInstance();
	}
	catch (std::exception& e)
	{
		std::string emsg = e.what();
		bufferSize = emsg.length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		inm_strcpy_s(*err, (bufferSize + 1), emsg.c_str());
		return SV_FAIL;
	}
	StartLogging(theConfPathInformer->CSPSInstallPath());
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int rv = SV_OK;
    std::string res;
	unsigned long buffSize = 0;
    std::stringstream errmsg;

    if (!req)
    {
        res = "NullCommand Input.";
        buffSize = res.length();
        rv = SV_FAIL;
    }
    else
    {
        std::vector<std::string> pairs;
        std::vector<std::string>::const_iterator pairIter;
        Tokenize(req, pairs, ";");
        if (pairs.size() != 3)
        {
            res = "Invalide Arg.More than 3 args with ; separation";
            buffSize = res.length();
            rv = SV_FAIL;
        }
        else
        {
            bool invalidarg = false;
            KeyVal_t map;
            pairIter = pairs.begin();
            while (pairIter != pairs.end())
            {
                std::vector<std::string> tokens;
                Tokenize(*pairIter, tokens, "=");

                if (tokens.size() != 2)
                {
                    res = "Invalid Arg. More than two args with = separation";
                    buffSize = res.length();
                    invalidarg = true;
                    break;
                }
                else
                {
                    map.insert(std::make_pair(tokens.front(), tokens.back()));
                }
                pairIter++;
            }
            if (!invalidarg)
            {
               
                std::string pscsip, pscsport,passphrase;
                KeyVal_t::iterator mapit;
                if ((mapit = map.find("PS_CS_IP")) != map.end())
                    pscsip = mapit->second;
                if ((mapit = map.find("PS_CS_PORT")) != map.end())
                    pscsport = mapit->second;
                if ((mapit = map.find("passphrase")) != map.end())
                    passphrase = mapit->second;

                if (!pscsip.empty() && !pscsport.empty() && !passphrase.empty())
                {
                    std::string hostid;
                    boost::shared_ptr<ConfValueObject> cvo(new ConfValueObject(/*SV_AMETHYST_CONF_FILE_PATH*/theConfPathInformer->AmethystConfPath()));
                    if (!cvo->getValue("HOST_GUID", hostid, errmsg))
                    {
                        res = errmsg.str().c_str();
                        buffSize = errmsg.str().length();
                        rv = SV_FAIL;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG, "successfully retrieved hostid %s from amethyst.conf to process passphrase.\n", hostid.c_str());

                        if (!ProcPassphrase(/*SV_CSPS_INSTALL_PATH*/theConfPathInformer->CSPSInstallPath(),pscsip, pscsport,hostid, passphrase, errmsg))
                        {
                            DebugPrintf(SV_LOG_DEBUG, "ProcPassphrase failed.\n");
                            res = errmsg.str().c_str();
                            buffSize = errmsg.str().length();
                            rv = SV_FAIL;
                        }
                    }
                }
                else
                {
                    res = "Error pscsip, pscsport or passphrase did not found in command input from UI Please check log.";
                    buffSize = res.length();
                    rv = SV_FAIL;
                }
            }
        }
    }
    if (rv == SV_FAIL)
    {
        DebugPrintf(SV_LOG_DEBUG, "return SV_FAIL.\n");
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(buffSize) + 1, INMAGE_EX(buffSize))
        *err = new char[buflen];
        inm_strcpy_s(*err, (buffSize + 1), res.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s", *err);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    Closing();

    return rv;

}


