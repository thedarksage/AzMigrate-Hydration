#ifndef CONFPROPERTYSETTINGS__H
#define CONFPROPERTYSETTINGS__H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include <ace/File_Lock.h>
#include <ace/Recursive_Thread_Mutex.h>
#include <ace/Configuration.h>
#include <ace/Configuration_Import_Export.h>


#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <exception>

//#include "portablehelpers.h""
#include "portable.h"
#include "globs.h"
#include "ProcessMgr.h"

#define SV_AGENT_CXPSCONF "transport\\cxps.conf"
#define SV_AGENT_DRSCOUTCONF "Application Data\\etc\\drscout.conf"
#define INM_SVSYSTEMS_REGKEY		"SOFTWARE\\SV Systems"

int const SV_MAX_ELEMENT_ON_KEY_VALUE_PAIR = 2;
typedef std::map<std::string, std::string> KeyVal_t;
typedef KeyVal_t::const_iterator KeyValIter_t;

ACE_Recursive_Thread_Mutex g_confRThreadMutex;

std::string GetWinApiErrMsg(DWORD winApiLastError)
{
	std::string errStr = "";
	LPVOID lpHostConfigErrMsg = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, winApiLastError, 0, (LPTSTR)&lpHostConfigErrMsg, 0, NULL))
	{
		errStr = reinterpret_cast<char*>(&lpHostConfigErrMsg);
		LocalFree(lpHostConfigErrMsg);
	}
	return errStr;
}

bool GetAgentRole(std::string& agentRole, char** err)
{
	unsigned long bufferSize = 0;
	bool ret = true;
	try{
		LocalConfigurator lc;
		agentRole = lc.getAgentRole();
	}
	catch (std::exception& e)
	{
		std::stringstream errmsg;
		errmsg << "Could not find AgentRole in drscout.conf file. Exception: " << e.what();
		bufferSize = errmsg.str().length();
		size_t buflen;
		INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(bufferSize) + 1, INMAGE_EX(bufferSize))
			*err = new char[buflen];
		//inm_strcpy_s(szServerkey, ARRAYSIZE(szServerkey), "ServerName");
		inm_strcpy_s(*err, (bufferSize + 1), errmsg.str().c_str());
		ret = false;
	}

	return ret;
}

bool UpdateRegistrySettings(std::string& csip, std::string& port, std::stringstream& errmsg, bool isHttps = true)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL rv = TRUE;
	HKEY svSysKey;
	HKEY fileReplKey;

	LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ | KEY_WRITE, &svSysKey);
	if (svSysRet != ERROR_SUCCESS)
	{
		errmsg << "Failed to open registry key to update global settings.\nErrorCode = " << svSysRet << "  . WinApiErrMsg: " << GetWinApiErrMsg(GetLastError());
		DebugPrintf(SV_LOG_DEBUG, "%s\n", errmsg.str().c_str());
		return FALSE;
	}

	//Update ServerHttpPort
	DWORD cxPort_Dw = atoi(port.c_str());
	svSysRet = RegSetValueEx(svSysKey, SV_SERVER_HTTP_PORT_VALUE_NAME, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&cxPort_Dw), sizeof(DWORD));
	if (svSysRet != ERROR_SUCCESS)
	{
		errmsg << "Failed to update Http port.\nErrorCode = " << svSysRet << "  . WinApiErrMsg: " << GetWinApiErrMsg(GetLastError());
		DebugPrintf(SV_LOG_DEBUG, "%s\n", errmsg.str().c_str());
		RegCloseKey(svSysKey);
		return FALSE;
	}

	//Update ServerName
	svSysRet = RegSetValueEx(svSysKey, SV_SERVER_NAME_VALUE_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*> (csip.c_str()), (lstrlen(csip.c_str()) + 1)*sizeof(TCHAR));
	if (svSysRet != ERROR_SUCCESS)
	{
		errmsg << "Failed to update CX ip.\nErrorCode = " << svSysRet << "  . WinApiErrMsg: " << GetWinApiErrMsg(GetLastError());
		DebugPrintf(SV_LOG_DEBUG, "%s\n", errmsg.str().c_str());
		RegCloseKey(svSysKey);
		return FALSE;
	}

	////Update Https
	svSysRet = RegSetValueEx(svSysKey, SV_HTTPS, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&isHttps), sizeof(DWORD));
	if (svSysRet != ERROR_SUCCESS)
	{
		errmsg << "Failed to update Https state.\nErrorCode = " << svSysRet << "  . WinApiErrMsg: " << GetWinApiErrMsg(GetLastError());
		DebugPrintf(SV_LOG_DEBUG, "%s\n", errmsg.str().c_str());
		RegCloseKey(svSysKey);
		return FALSE;
	}

	RegCloseKey(svSysKey);
	return rv;
}
bool UpdateMTAgentSettings(std::string& csip, std::string& port, std::string& sslport, std::stringstream& errmsg, bool isHttps = true)
{
	bool ret = true;
	std::string agentInstallPath = "";
	try
	{
		LocalConfigurator localConfig;
		//CX_SETTINGS
		HTTP_CONNECTION_SETTINGS httpConSettings;
		strcpy_s(httpConSettings.ipAddress, _countof(httpConSettings.ipAddress), csip.c_str());
		httpConSettings.port = atoi(port.c_str());
		localConfig.setHttp(httpConSettings);
		localConfig.setHttps(isHttps);

		agentInstallPath = localConfig.getInstallPath();
	}
	catch (std::exception const& e)
	{
		ret = FALSE;
		errmsg << "Could not set agent settings in drscout.conf file. Exception: " << e.what();
		DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", e.what());
	}
	if (ret)
	{
		std::string tmpPath = agentInstallPath + std::string("\\") + "transport";

		//std::string tmpFile = tmpPath;
		//tmpFile += ACE_DIRECTORY_SEPARATOR_CHAR;
		//tmpFile += "tmpLog.log";

		std::string conffile = tmpPath + std::string("\\") + "cxps.conf";

		std::string cmd = std::string("\"") + agentInstallPath + std::string("\\") + "transport" + std::string("\\") + "cxpscli.exe" + std::string("\"");
		cmd += std::string(" ") + "--conf" + std::string(" ") + std::string("\"") + conffile + std::string("\"");
		cmd += std::string(" ") + "--csip" + std::string(" ") + csip;
		cmd += std::string(" ") + "--sslport" + std::string(" ") + sslport;

		if (isHttps)
		{
			cmd += std::string(" ") + "--cssecure" + std::string(" ") + "yes";
			cmd += std::string(" ") + "--cssslport" + std::string(" ") + port;
		}
		if (!isHttps)
		{
			cmd += std::string(" ") + "--cssecure" + std::string(" ") + "no";
			cmd += std::string(" ") + "--csport" + std::string(" ") + port;
		}


		DebugPrintf(SV_LOG_DEBUG, "ENTERED Command : %s\n", cmd.c_str());


		std::string emsg;

		if (!execProcUsingPipe(cmd, emsg))
		{
			std::string resp = std::string("Successfully updated ip and port in ") + agentInstallPath + std::string("\\") + SV_AGENT_DRSCOUTCONF + std::string(".But failed to update in ")
				+ agentInstallPath + std::string("\\") + SV_AGENT_CXPSCONF + std::string(". Error: ") + emsg;
			errmsg << resp;
			ret = false;
			DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
		}
		else
		{
			std::stringstream err;
			if (!UpdateRegistrySettings(csip, port, err, isHttps))
			{
				std::string resp = std::string("Successfully updated ip and port in ") + agentInstallPath + std::string("\\") + SV_AGENT_CXPSCONF + std::string(" and ") 
					+ agentInstallPath + std::string("\\") + SV_AGENT_CXPSCONF + std::string(".But failed to update in registry.")
					+ std::string(". Error: ") + err.str().c_str();
				errmsg << resp;
				DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
				ret = false;
			}
		}
	}

	return ret;
}

class ConfLinesObject
{
public:
    ConfLinesObject() :emptyKey(false), isKeyValPair(true){}
    bool operator < (const ConfLinesObject& str) const
    {
        return (lineNum < str.lineNum);
    }

    void Tokenize(const std::string& input, std::vector<std::string>& outtoks, const std::string& separators)
    {
        std::string::size_type pp, cp, np;

        outtoks.clear();
        pp = input.find_first_not_of(separators, 0);
        cp = input.find_first_of(separators, pp);
        if ((std::string::npos != cp) || (std::string::npos != pp)) {
            outtoks.push_back(input.substr(pp, cp - pp));
        }

        pp = input.find_first_not_of(separators, cp);
        np = input.find_first_not_of(' ', pp);//to check if the key contains only spaces, then dont push into the vector
        if ((std::string::npos != pp) && (std::string::npos != np))
            outtoks.push_back(input.substr(pp));
    }

    bool emptyKey;
    bool isKeyValPair;
    unsigned int lineNum;
    std::vector<std::string> tokens;
};
class ConfValueObject
{
public:

    ConfValueObject(const std::string& confFile) :m_lockInifile(ACE_TEXT_CHAR_TO_TCHAR((confFile + ".lck").c_str()), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS, 0)
    {
        m_confFile = confFile;
    }

    void getKeyValuePairs(KeyVal_t& pairmap,std::stringstream& errmsg){
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        AutoGuardRThreadMutex mutex(g_confRThreadMutex);
        AutoGuardFileLock lock(m_lockInifile);
        bool rv = true;

        Parse(errmsg);
        if (errmsg.str().empty())
        {
            std::vector<ConfLinesObject>::iterator st = valueObject.begin();
            while (st != valueObject.end())
            {

                if (st->isKeyValPair)
                {
                    std::string key = st->tokens.front();
                    Trim(key);
                    std::string value = "";
                    if (!st->emptyKey)
                    {
                        value = st->tokens.back();
                        Trim(value, " ");
                    }
                    pairmap.insert(std::make_pair(key, value));
                }

                st++;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed: Parsing %s\n", m_confFile.c_str());
            rv = false;
        }
        
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    }

    bool getValue(const std::string& key, std::string& value,std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = false;
        KeyVal_t pairs;
        getKeyValuePairs(pairs,errmsg);
        valueObject.clear();

        KeyValIter_t it = pairs.begin();
        while (it != pairs.end())
        {
            if (stricmp(it->first.c_str(), key.c_str()) == 0)
            {
                value = it->second;
                rv = true;
                break;
            }

            it++;
        }
		if (value.empty())
		{
			errmsg << "key " << key << " does not exist in conf file " << m_confFile;
		}
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }

    bool update(KeyVal_t newKVmap,std::stringstream& errmsg){
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        AutoGuardRThreadMutex mutex(g_confRThreadMutex);
        AutoGuardFileLock lock(m_lockInifile);
        bool updated = true;

        Parse(errmsg);
        bool updatedReadValue = true;

        KeyValIter_t it = newKVmap.begin();
        while (it != newKVmap.end())
        {
            if (!updateValue(it->first, it->second, errmsg))
            {
                errmsg << "Failed to update tmp read value for the Key " << it->first << "with new value " << it->second << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                updatedReadValue = false;
                updated = false;
            }
            it++;
        }
        
        if (updatedReadValue)
        {
            if (!updateFile(errmsg))
            {
                errmsg << "Failed to update conffile " << m_confFile << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                updated = false;
            }
        }
        valueObject.clear();
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return updated;
    }

    void clear(){ valueObject.clear(); }
protected:
    void Trim(std::string& s, const std::string &trim_chars = " \n\b\t\a\r\xc")
    {
        size_t begIdx = s.find_first_not_of(trim_chars);
        if (begIdx == std::string::npos)
        {
            begIdx = 0;
        }
        size_t endIdx = s.find_last_not_of(trim_chars);
        if (endIdx == std::string::npos)
        {
            endIdx = s.size() - 1;
        }
        s = s.substr(begIdx, endIdx - begIdx + 1);
    }
    bool RenameConfFile(std::string const & confCopyFile, std::string const & conFile)const
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool rv = true;

		std::string backName(conFile);
		backName += ".";
		backName += boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time());
		backName += ".bak";
		try {
			boost::filesystem::rename(conFile, backName);
		}
		catch (std::exception const& e) {
			std::cout << "failed to back up " << conFile << ": " << e.what() << '\n';
			return false;
		}

        if (ACE_OS::rename(confCopyFile.c_str(), conFile.c_str()) < 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "ace last error %d\n" ,ACE_OS::last_error());
            ACE_OS::unlink(confCopyFile.c_str());
            rv = false;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return rv;
    }

    void ParseStream(fstream& o)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        int i = 0;

        std::string line;

        while (getline(o, line))
        {
            boost::shared_ptr<ConfLinesObject> clo(new ConfLinesObject);

            if ('#' == line[0] || '[' == line[0] || line.find_first_not_of(' ') == line.npos)
            {
                clo->tokens.push_back(line);
                clo->isKeyValPair = false;
            }

            else
            {
                clo->Tokenize(line, clo->tokens, "=");
            }

            clo->lineNum = i++;
            if (clo->isKeyValPair && clo->tokens.size() == SV_MAX_ELEMENT_ON_KEY_VALUE_PAIR - 1)
                clo->emptyKey = true;

            valueObject.push_back(*clo);
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    }

    void Parse(std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        std::string configFilePath = m_confFile;
        fstream file(configFilePath.c_str());
        if (!file)
        {
            errmsg << " File Not Found : " << configFilePath.c_str() << "\n";
            DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
        }
           

        ParseStream(file);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    }
    bool getValuePosIfInDoubleQuote(const std::string& value, std::string::size_type& beginPos, std::string::size_type& endPos){
        beginPos = value.find_first_of('\"', 0);
        endPos = value.find_first_of('\"', beginPos + 1);
        if ((std::string::npos != beginPos) && (std::string::npos != endPos))
            return true;

        return false;

    }
    bool getVaulePos(const std::string& value, std::string::size_type& beginPos){
        beginPos = value.find_first_of(' ', 0);
        
        if ((std::string::npos != beginPos))
            return true;

        return false;
    }

    bool updateValue(const std::string& key, const std::string& newValue,std::stringstream& errmsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
        bool updated = false;
        if (!newValue.empty())
        {
            std::vector<ConfLinesObject>::iterator st = this->valueObject.begin();
            while (st != this->valueObject.end())
            {
                std::string trmEnteredKey = key;
                Trim(trmEnteredKey);
                std::string keyElement = st->tokens.front();
                Trim(keyElement);

                if (st->isKeyValPair && stricmp(keyElement.c_str(), trmEnteredKey.c_str()) == 0)
                {
                    std::string updateValue;
                    if (st->tokens.size() == SV_MAX_ELEMENT_ON_KEY_VALUE_PAIR)
                    {
                        std::string existValue = st->tokens.back();

                        std::string::size_type pp, cp;

                        if (getValuePosIfInDoubleQuote(existValue, pp, cp))// to store the value if the previous value was in double quote
                        {
                            updateValue = existValue;
                            updateValue.replace(pp + 1, cp - pp - 1, newValue);

                        }
                        else if (existValue.c_str()[0] == ' ' && getVaulePos(existValue, pp))//to store the value from exact position after blank spcace as the value was earlier
                        {
                            updateValue = existValue;
                            updateValue.replace(pp + 1, existValue.length() - 1, newValue);
                        }
                        else
                        {
                            updateValue += newValue;
                        }
                        st->tokens.pop_back();
                        st->tokens.push_back(updateValue);
                        updated = true;
                    }
                    else
                    {
                        updateValue = newValue;
                        st->tokens.push_back(updateValue);
                        st->emptyKey = false;
                        updated = true;
                    }
                }
                st++;
            }
            if (!updated)
            {
                errmsg << "Error: Entered key does not exist ."<< "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
            }
        }
        else
        {
            errmsg << "Error : Entered new value for the key " << key << "is empty." << "\n";
            DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
        return updated;
    }

    bool updateFile(std::stringstream& errmsg){
        bool updatedFile = true;
        std::string newConfFilePath = m_confFile;
        newConfFilePath += ".copy";

        FILE * pFile = fopen(newConfFilePath.c_str(), "w+");

        if (pFile)
        {
            std::vector<ConfLinesObject>::iterator st = valueObject.begin();
            while (st != valueObject.end())
            {

                int token = 0;

                while (token < st->tokens.size())
                {
                    if (token == SV_MAX_ELEMENT_ON_KEY_VALUE_PAIR - 1)
                    {
                        if (fputs("=", pFile) == EOF)
                        {
                            updatedFile = false;
                            errmsg << "failed to write = on file" << newConfFilePath;
                            DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                            break;
                        }
                    }
                    if (fputs((st->tokens[token]).c_str(), pFile) == EOF)
                    {
                        updatedFile = false;
                        errmsg << "failed to write tokens on file" << newConfFilePath;
                        DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                        break;
                    }
                    token++;
                }
                if (!updatedFile)
                    break;
                if (st->emptyKey)
                if (fputs("=", pFile) == EOF)
                {
                    updatedFile = false;
                    errmsg << "failed to write = on file" << newConfFilePath;
                    DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                    break;
                }

                if (fputs("\n", pFile) == EOF)
                {
                    updatedFile = false;
                    errmsg << "failed to write entered char on file" << newConfFilePath;
                    DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                    break;
                }
                st++;
            }
            fclose(pFile);

            if (!RenameConfFile(newConfFilePath, m_confFile)){
                errmsg << "Couldn't Rename file " << newConfFilePath << "\n";
                DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
                updatedFile = false;
            }
        }
        else
        {
            errmsg << "failed to create tmp update file " << newConfFilePath << "\n";
            DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());
            updatedFile = false;
        }

        return updatedFile;
    }

    mutable ACE_File_Lock m_lockInifile;
    typedef ACE_Guard<ACE_File_Lock> AutoGuardFileLock;
    typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuardRThreadMutex;

    std::string m_confFile;
    std::vector<ConfLinesObject> valueObject;

};



#endif // CONFPROPERTYSETTINGS__H