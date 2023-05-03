#ifndef ORACLE_DISCOVERY_H
#define ORACLE_DISCOVERY_H

//#include <ace/Task.h>
//#include <ace/Process_Manager.h>
#include <ace/Guard_T.h>
#include "ace/Recursive_Thread_Mutex.h"
#include <stdlib.h>

struct OracleAppVersionDiscInfo
{

	std::string m_dbName;
	std::string m_dbVersion;
    std::string m_dbInstallPath;

	std::string m_isCluster;
	std::string m_clusterName;

    bool m_recoveryLogEnabled;

	std::string m_hostId;
	std::string m_nodeName;
	std::list<std::string> m_otherClusterNodes;

    std::map<std::string, std::string> m_filesMap;
	bool m_isDBRunning;
       std::list <std::string> m_diskView;
	//std::list<std::pair<std::string, std::string> > m_diskView; //  will have viewLevelName(mountPoint name/diskgroup name) and viewLevelType(mountpoint/DiskGroup/Disk)
	std::list<std::string> m_filterDeviceList;

   	//std::string summary();

    OracleAppVersionDiscInfo()
    {
        m_recoveryLogEnabled = false;
	    m_isDBRunning = false;
    }

};

struct OracleClusterDiscInfo
{
	std::string m_clusterName;
	std::string m_nodeName;
    std::list<std::string> m_otherNodes;
};

struct OracleUnregisterDiscInfo 
{
	std::string m_dbName;
};

struct OracleAppDevDiscInfo
{
	std::list<std::string> m_devNames;
};

struct OracleAppDiscoveryInfo
{
    std::map<std::string, OracleAppVersionDiscInfo> m_dbOracleDiscInfo;
    std::map<std::string, OracleClusterDiscInfo> m_dbOracleClusterDiscInfo;
    std::map<std::string, OracleUnregisterDiscInfo> m_dbUnregisterDiscInfo;
    std::map<std::string, OracleAppDevDiscInfo> m_dbOracleAppDevDiscInfo;
    //std::string summary() ;
} ;

class OracleDiscoveryImpl
{


	std::string m_hostName;
	bool m_isInstalled;
	bool m_isMounted;
    static ACE_Recursive_Thread_Mutex m_mutex;

	SVERROR discoverOracleVersionDiscInfo(OracleAppDiscoveryInfo&);
	SVERROR getOracleDiscoveryData(OracleAppDiscoveryInfo&);
	SVERROR getOracleAppDevices(OracleAppDiscoveryInfo&);
	SVERROR fillOracleAppVersionDiscInfo(std::map<std::string, std::string>&, OracleAppDiscoveryInfo&);
	SVERROR fillOracleClusterDiscInfo(std::map<std::string, std::string>&, OracleAppDiscoveryInfo&);
	SVERROR fillOracleUnregisterInfo(std::map<std::string, std::string>&, OracleAppDiscoveryInfo&);

public:
	SVERROR discoverOracleApplication(OracleAppDiscoveryInfo&);

	void Display(const OracleAppVersionDiscInfo& oracleAppInfo) ;

	void setHostName(const std::string& hostName);
	bool isInstalled(const std::string& oracleHost);
	bool isDBRunning(const std::string& oracleHost);

	static boost::shared_ptr<OracleDiscoveryImpl> m_instancePtr;
	static boost::shared_ptr<OracleDiscoveryImpl> getInstance();
	
	


};

#endif



