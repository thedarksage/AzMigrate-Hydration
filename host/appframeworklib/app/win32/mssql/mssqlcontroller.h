#ifndef MSSQL_CONTROLLER_H
#define MSSQL_CONTROLLER_H
#include "app/appcontroller.h"
#include "appcommand.h"

#define APPLICATION_DATA_DIRECTORY    "Application Data"
#define FAILOVER_DATA_PATH "Failover"
#define MSSQL_DATA_PATH "Mssql"
#define DATA "Data"

class MSSQLController;
typedef boost::shared_ptr<MSSQLController> MSSQLControllerPtr ;


class MSSQLController : public AppController
{
	SVERROR  MSSQLController::Process() ;
    MSSQLController(ACE_Thread_Manager*) ;
    static MSSQLControllerPtr m_SQLinstancePtr ;
	SVERROR Consistency( );
	SVERROR prepareTarget ( );
	SVERROR stopSQLServices(const std::string &strApplication,const std::string &strInstanceName,bool isClusterNode);
	bool m_bCluster;
    SVERROR CreateDiscoveryInfoFiles();
    void CreateSQLInstanceDiscoveryInfofile(const std::string& instanceName,SourceSQLInstanceInfo& srcDiscInfo);
    void CreateSQLFailoverServicesConfFile();
    void CreateSQLDiscoveryInfoFiles();
    SVERROR GetSourceDiscoveryInformation();
    SVERROR getSQLInstanceList(std::list<std::string>& sqlInstancesList);
public:
	void ProcessMsg(SV_ULONG policyId) ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
	 static MSSQLControllerPtr getInstance(ACE_Thread_Manager* tm) ;
    void BuildFailoverCommand(std::string& failoverCmd);
    void MakeAppServicesAutoStart() ;
    void PrePareFailoverJobs();
    void UpdateConfigFiles();
    bool IsItActiveNode();
    void PreClusterReconstruction() ;
    bool StartAppServices();

} ;
#endif
