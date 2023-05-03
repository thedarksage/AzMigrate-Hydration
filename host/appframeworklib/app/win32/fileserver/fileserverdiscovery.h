#ifndef FILESERVER_DISCOVERY_H
#define FILESERVER_DISCOVERY_H
#include "appglobals.h"
#include <list>
#include <set>
#include "clusterutil.h"
#include "service.h"
#include <Sddl.h>
#include <Aclapi.h>
#include <atlconv.h>

static const char LANMANSERVER_SHARE_KEY[] = "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\Shares";
static const char  SHARES_KEY[] = "[HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\Shares]";
static const char SECURITY_KEY[] = "[HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\Shares\\Security]";

struct FileShareInfo
{
	//For W2K8 Cluster
	SV_ULONG             m_shi503_type;
	std::string          m_shi503_remark;
	SV_ULONG             m_shi503_max_uses;
	std::string          m_shi503_passwd;

	std::string m_shareName ;
	std::string m_absolutePath ;
	std::string m_UNCPath ;
	std::string m_mountPoint ;
	std::string m_sharesAscii;
	std::string m_securityAscii;
    std::list<std::pair<std::string, std::string> > getProperties() ;
};

struct FileServerInstanceDiscoveryData
{
    std::list<InmService> m_services ;
    std::list<std::string> m_ips ;
	int m_ErrorCode;
	std::string m_ErrorString;
	FileServerInstanceDiscoveryData()
	{
		m_ErrorCode = 0;
		m_ErrorString = "SUCCESS";
	}
} ;

struct FileServerInstanceMetaData
{
	bool m_isClustered;
    std::list<std::string> m_volumes ;
    std::map<std::string, FileShareInfo> m_shareInfo ;
	int m_ErrorCode;
	std::string m_ErrorString;
	FileServerInstanceMetaData()
	{
		m_isClustered = true;
		m_ErrorCode = 0;
		m_ErrorString = "ACTIVE_INSTANCE";
	}
};

struct w2k8networkDetails
{
	std::string networkName;
	std::list<std::string> ipAddress;
};

class FileServerDiscoveryImpl
{

private:
	std::string m_stdOut;
	SVERROR exportRegistry( const std::string& filePath );
	SVERROR getRegistryVersionData( const std::string& totalData, std::string& regVersion );
	SVERROR getShareData( const std::string& shareName, const std::string& totalData, std::string& sharesAscii );
	SVERROR getSecurityData( const std::string& shareName, const std::string& totalData, std::string& securityAscii );
	SVERROR fillSharesAndSecurityDataFromReg( FileShareInfo& );
	bool isAlreadyDiscovered(std::string shareName, std::map<std::string, FileServerInstanceMetaData>&);
	void trimCharacters(std::string &);
	SVERROR DiscoverAllW2k8FileShares(std::list<std::string>&, std::map<std::string, w2k8networkDetails>&, std::map<std::string, FileServerInstanceDiscoveryData>&, std::map<std::string, FileServerInstanceMetaData>&, std::map<std::string,bool>& );
	void getBaseVolumeName(const std::string &,std::string &);
	BOOL getSecurityDescriptorString(const std::string&, std::string&);
	BOOL setSecurityEnablePriv();
	SVERROR discoverFileServerVolumes(std::list<std::string>&, std::map<std::string, w2k8networkDetails>&, std::map<std::string, w2k8networkDetails>&);
	SVERROR executeVbsScriptForClusterDisk(const std::string&);
	SVERROR createResourceDiskFile(const std::string&, const std::string&);
	SVERROR prepareClusterDiskNameIdMap(std::map<std::string,std::string>&);
	void stringParser(std::list<std::string>&);
	void stringParser(std::list<std::string>,std::map<std::string,std::string>&);
	void stringParser (std::list<std::string>,std::map<std::string,std::list<std::string> >&);
	SVERROR getListOfvolumesOnClusterDisk(const std::string&, const std::string&, std::list<std::string>&);
	void FormatFsVol(std::string&);
public:
	SVERROR discoverFileServer(std::map<std::string, FileServerInstanceDiscoveryData>&, std::map<std::string, FileServerInstanceMetaData>& );
	SVERROR discoverClusterShareInfo(std::map<std::string, FileServerInstanceDiscoveryData>&, std::map<std::string, FileServerInstanceMetaData>&);
	SVERROR discoverW2k8ClusterShareInfo(std::map<std::string, FileServerInstanceDiscoveryData>&, std::map<std::string, FileServerInstanceMetaData>&);
	SVERROR discoverNonClusterShareInfo(std::map<std::string, FileServerInstanceDiscoveryData>&, std::map<std::string, FileServerInstanceMetaData>&);
	SVERROR generateRegistryFile( const std::string& filePath, const std::string& registryVersion, const std::map<std::string, FileShareInfo>& regInfoMap );

};
#endif
