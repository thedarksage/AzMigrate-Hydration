#ifndef FILE_SERVER_CONTROLLER_H
#define FILE_SERVER_CONTROLLER_H
#include "app/appcontroller.h"
#include "config\connection.h"
#include "fileserverdiscovery.h"

#define APPLICATION_DATA_DIRECTORY    "Application Data"
#define FAILOVER_DATA_PATH "Failover"
#define FILESERVER_DATA_PATH "FileServer"
#define DATA "Data"

class FileServerController;
typedef boost::shared_ptr<FileServerController>FileServerControllerPtr;

class FileServerController : public AppController
{
	SVERROR  FileServerController::Process() ;
	FileServerController(ACE_Thread_Manager*)  ;
	SVERROR Consistency( );
	SVERROR prepareTarget ( );
    static FileServerControllerPtr m_FSinstancePtr ;
	SVERROR stopFSServices(const std::string &strApplication, bool isClusterNode);
	bool m_bCluster;
    SVERROR GetSourceDiscoveryInformation();
    void CreateFileServerDiscoveryInfoFiles();
    void CreateFileServerRegistryInfoFiles();
    void CreateFileServerFailoverServicesConfFile();
	void CreateFileServerDbFiles();
	SrcFileServerDiscoveryInfomation m_SrcFileServerDiscInfo;

public:
    void MakeAppServicesAutoStart() ;
    virtual int open(void *args = 0) ;
    virtual int close(u_long flags = 0) ;
    virtual int svc() ;
    void ProcessMsg(SV_ULONG policyId) ;
    static FileServerControllerPtr getInstance(ACE_Thread_Manager* tm) ;
    void UpdateConfigFiles();
    void PrePareFailoverJobs();
    void BuildFailoverCommand(std::string& failoverCmd);
	void BuildADReplicationCmd(std::string& adReplicationCmd);
    void BuildDRServerFastFailbackCmd(std::string& fastFailbackCmd);
    void BuildProductionServerFastFailbackCmd(std::string& fastFailbackCmd);
    bool IsItActiveNode();
    void PreClusterReconstruction() ;
    bool StartAppServices();
    bool isItW2k8R2ClusterNode();
} ;

class W2k8FileServerDB
{
	void CreateFileServerInfoTable();
public:
	SqliteConnection m_con;
	void InitFileServerDB(const std::string& dbPath);
	void AddFileServerInfoinDB(FileShareInfo &);
};
#endif
