#ifndef SERVICEMGR__H
#define SERVICEMGR__H

#include <list>
#include <sstream>
#include <map>

#define CS_INSTALLATION_TYPE "\"3\""

class ServiceMgr
{

public:
    ServiceMgr()
    {
        m_ServicesFullNames.insert(std::make_pair("ProcessServerMonitor", "Process Server Monitor"));
        m_ServicesFullNames.insert(std::make_pair("ProcessServer", "Process Server"));
		m_ServicesFullNames.insert(std::make_pair("tmansvc", "tmansvc"));
		m_ServicesFullNames.insert(std::make_pair("cxprocessserver", "cxprocessserver"));
		m_ServicesFullNames.insert(std::make_pair("InMage PushInstall", "InMage PushInstall"));
		m_ServicesFullNames.insert(std::make_pair("svagents", "InMage Scout VX Agent - Sentinel/Outpost"));
		m_ServicesFullNames.insert(std::make_pair("obengine", "Microsoft Azure Recovery Services Agent"));
		m_ServicesFullNames.insert(std::make_pair("InMage Scout Application Service", "InMage Scout Application Service"));
		//m_ServicesFullNames.insert(std::make_pair("frsvc", "InMage Scout FX Agent"));
    }
    bool getServiceList(const std::string& installationtype, std::list<std::string>& services, bool isMTInstalled);
    int stopservices(std::list<std::string>& services,std::stringstream& errmsg);
    int startservices(std::list<std::string>& services, std::stringstream& errmsg);
	bool RestartServices(std::list<std::string>& services, std::list<std::string>& failedToRestartServices, std::stringstream& err);

private:
	std::map<std::string, std::string> m_ServicesFullNames;

};

#endif /* SERVICEMGR__H */