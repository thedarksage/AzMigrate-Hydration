#ifndef DB2_CONTROLLER_H
#define DB2_CONTROLLER_H
#include "app/appcontroller.h"
#include "Db2Application.h"

class Db2Controller ;
typedef boost::shared_ptr<Db2Controller> Db2ControllerPtr ;

class Db2Controller : public AppController
{
	
	Db2ApplicationPtr m_db2App ;
    SrcDb2DiscoveryInformation m_srcDiscInfo;
	SVERROR Process();	
	SVERROR Consistency( );
	SVERROR prepareTarget ( );
	SVERROR stopDb2Services(const std::string &strApplication);
	ApplicationPolicy m_policy ;
    static Db2ControllerPtr m_instancePtr ;
	Db2Controller(ACE_Thread_Manager*) ;   
    std::string srcDbConfFilePath;      
    std::string srcDbInstance;      

	bool IsItActiveNode();
public:
   	void ProcessMsg(SV_ULONG policyId) ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc() ;
    static Db2ControllerPtr getInstance(ACE_Thread_Manager* tm) ;

    void PrePareFailoverJobs();
    void UpdateConfigFiles();
    void BuildFailoverCommand(std::string& failoverCmd);
    SVERROR GetSourceDiscoveryInformation();
    void MakeAppServicesAutoStart() ;    
	void PreClusterReconstruction();
    bool StartAppServices();

};
#endif
