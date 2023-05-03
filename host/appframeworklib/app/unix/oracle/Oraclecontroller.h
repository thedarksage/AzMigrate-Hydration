#ifndef ORACLE_CONTROLLER_H
#define ORACLE_CONTROLLER_H
#include "app/appcontroller.h"
#include "Oracleapplication.h"

#define RESOURCE_ORACLE_INFORMATON_STORE	"Oracle Information Store"
#define RESOURCE_ORACLE_SYSTEM_ATTENDANT	"Oracle System Attendant"
#define RESOURCE_ORACLE_MTA				"Oracle Message Transfer Agent"

#define ORACLE		"Oracle"


// Oracle Service Names
#define ORACLE_INFORMATION_STORE		"OracleIS"
#define ORACLE_MANAGEMENT				"OracleMGMT"
#define ORACLE_MTA					"OracleMTA"
#define ORACLE_ROUTING_ENGINE			"RESvc"
#define ORACLE_SYSTEM_ATTENDANT		"OracleSA"

#define ORACLE_REPL					"OracleRepl"
#define ORACLE_TRANS					"OracleTransportLogSearch"

class OracleController ;
typedef boost::shared_ptr<OracleController> OracleControllerPtr ;

class OracleController : public AppController
{
    OracleApplicationPtr m_oracleApp ;
    SrcOracleDiscoveryInformation m_srcDiscInfo;
	SVERROR Process();
	void DiscoverApplication() ;
	SVERROR Consistency( );
	SVERROR prepareTarget ( );
	SVERROR stopOracleServices(const std::string &strApplication,bool isClusterNode);
	ApplicationPolicy m_policy ;
    static OracleControllerPtr m_instancePtr ;
	OracleController(ACE_Thread_Manager*) ;
	bool m_bCluster;
    //std::map<std::string, std::string > srcDbConfigFileMap;
    std::string srcDbConfFilePath;
    // TargetOracleInfo tgtInfo;
    std::string srcDbPFilePath;
    std::string srcDbGeneratedPFilePath;
    std::string srcDbInstance;

	bool IsItActiveNode();
public:
   	void ProcessMsg(SV_ULONG policyId) ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc() ;
    static OracleControllerPtr getInstance(ACE_Thread_Manager* tm) ;

    void PrePareFailoverJobs();
    void UpdateConfigFiles();
    void BuildFailoverCommand(std::string& failoverCmd);
    SVERROR GetSourceDiscoveryInformation();
    void MakeAppServicesAutoStart() ;
    void PreClusterReconstruction();
    bool StartAppServices();

} ;
#endif
