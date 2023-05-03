#ifndef MSEXCHANGE_CONTROLLER_H
#define MSEXCHANGE_CONTROLLER_H
#include "app/appcontroller.h"
#include "exchangeapplication.h"

#define RESOURCE_EXCHANGE_INFORMATON_STORE	        "Microsoft Exchange Information Store"
#define RESOURCE_EXCHANGE_SYSTEM_ATTENDANT	        "Microsoft Exchange System Attendant"
#define RESOURCE_EXCHANGE_MTA				                    "Microsoft Exchange Message Transfer Agent"

#define EXCHANGE		"Exchange"
#define EXCHANGE2003	"Exchange2003"
#define EXCHANGE2007	"Exchange2007"
#define EXCHANGE2010	"Exchange2010"

// Exchange Service Names
#define EXCHANGE_TRANSPORT_SERVICE		"MSExchangeTransport"
#define EXCHANGE_INFORMATION_STORE		"MSExchangeIS"
#define EXCHANGE_MANAGEMENT				    "MSExchangeMGMT"
#define EXCHANGE_MTA					                "MSExchangeMTA"
#define EXCHANGE_ROUTING_ENGINE			    "RESvc"
#define EXCHANGE_SYSTEM_ATTENDANT		"MSExchangeSA"

#define EXCHANGE_REPL					                "MSExchangeRepl"
#define ECHANGE_TRANS					            "MSExchangeTransportLogSearch"
#define FAILOVER_DATA_PATH "Failover"
#define DATA "Data"

class MSExchangeController ;
typedef boost::shared_ptr<MSExchangeController> MSExchangeControllerPtr ;


class MSExchangeController : public AppController
{
    ExchangeApplicationPtr m_exchApp ;
    SrcExchangeDiscInfo m_SrcDiscInfo;
    SrcExchange2010DiscInfo m_Exchange2010DiscInfo;
	SVERROR Process();
	void DiscoverApplication() ;
	SVERROR Consistency( );
	SVERROR persistSearchPath();
	SVERROR backupExchangeSearchDir();
    SVERROR restoreExchangeSearchDir() ;
	void getDataSearchPath(std::list<std::string>& );
	SVERROR prepareTarget ( );
	SVERROR stopExchangeServices(const std::string &strApplication,bool isClusterNode);
    static MSExchangeControllerPtr m_instancePtr ;
	MSExchangeController(ACE_Thread_Manager*) ;
	bool m_bCluster;
	
    void CreateExchangeDiscoveryInfoFiles();
    void CreateExchangeDiscoveryDBInfoFile();
    void CreateExchangeDiscoveryLogInfoFile();
    void CreateExchange2010DiscoveryInfoFiles();
    void CreateExchange2010DiscoveryDBInfoFile();
    void CreateExchange2010DiscoveryLogInfoFile();
    void CreateExchangeFailoverServicesConfFile();
    SVERROR GetSourceDiscoveryInformation();
	void updateJobStatus(InmTaskState ,SV_ULONG policyId,const std::string &jobResult,const std::string &LogMsg,bool updateCxStatus = true,bool updateSchedulerStatus = true);
public:
	SVERROR copyExchangeSearchFiles(const std::string& strSourcePath,const std::string strTargetPath);

   	void ProcessMsg(SV_ULONG policyId) ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc() ;
    static MSExchangeControllerPtr getInstance(ACE_Thread_Manager* tm) ;
    void BuildFailoverCommand(std::string& failoverCmd);
    void BuildExFailoverCommand(std::string& exFailoverCmd,const std::string& mailServerName);
    void PrePareFailoverJobs();
    void BuildProductionServerFastFailbackExFailoverCmd(std::string& exfailoverCmd);
    
    void UpdateConfigFiles();
    bool IsItActiveNode();
    void MakeAppServicesAutoStart() ;
	void PreClusterReconstruction() ;
	bool StartAppServices();
	void onlineDependentResources();
} ;
#endif
