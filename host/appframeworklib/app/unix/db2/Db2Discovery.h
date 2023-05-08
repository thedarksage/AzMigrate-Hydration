#ifndef DB2DISCOVERY_H
#define DB2DISCOVERY_H

#include <ace/Guard_T.h>
#include "ace/Recursive_Thread_Mutex.h"
#include <stdlib.h>

struct Db2AppDatabaseDiscInfo
{
	std::string m_dbName;
	std::string m_dbLocation;
	std::map<std::string, std::string> m_filesMap;

	bool m_recoveryLogEnabled;

	std::list <std::string> m_diskView;
	std::list<std::string> m_filterDeviceList;

	Db2AppDatabaseDiscInfo()
	{
		m_recoveryLogEnabled = false;
	}
};

struct Db2AppInstanceDiscInfo
{
	std::string m_hostId;
	std::string m_nodeName;	
	
	std::string m_dbInstance;
	std::string m_dbInstallPath;
	std::string m_dbVersion;
    	
	//As each instance may have number of dbs.
	std::map<std::string, Db2AppDatabaseDiscInfo> m_db2DiscInfo;
		
    bool m_isInstRunning;
    
	Db2AppInstanceDiscInfo()
    {
        m_isInstRunning = false;
    }
};

struct Db2UnregisterDiscInfo 
{
    std::list<std::string> m_dbNames;
};

struct Db2AppDiscoveryInfo
{
    std::map<std::string, Db2AppInstanceDiscInfo> m_db2InstDiscInfo;
    std::map<std::string, Db2UnregisterDiscInfo> m_db2UnregisterDiscInfo;        
} ;

class Db2DiscoveryImpl
{
	std::string m_hostName;
	bool m_isInstalled;
	bool m_isMounted;
    static ACE_Recursive_Thread_Mutex m_mutex;

	SVERROR discoverDb2VersionDiscInfo(Db2AppDiscoveryInfo&);
	SVERROR getDb2DiscoveryData(Db2AppDiscoveryInfo&);	
	SVERROR fillDb2AppInstanceDiscInfo(std::map<std::string, std::string>&, Db2AppDiscoveryInfo&);
	SVERROR fillDb2UnregisterInfo(std::map<std::string, std::string>&, Db2AppDiscoveryInfo&);

public:
	SVERROR discoverDb2Application(Db2AppDiscoveryInfo&);

	void Display(const Db2AppInstanceDiscInfo& db2AppInfo) ;

	void setHostName(const std::string& hostName);
	bool isInstalled(const std::string& db2Host);
	bool isDBRunning(const std::string& db2Host);

	static boost::shared_ptr<Db2DiscoveryImpl> m_instancePtr;
	static boost::shared_ptr<Db2DiscoveryImpl> getInstance();

};

#endif
