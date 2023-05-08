#ifndef __APP__CONTROLLER__H
#define __APP__CONTROLLER__H

#include <boost/shared_ptr.hpp>
#include <ace/Task.h>
#include <ace/Thread_Manager.h>
#include<ace/Message_Queue.h>
#include <sigslot.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "applicationsettings.h"
#include "serializeapplicationsettings.h"

#include "appglobals.h"

class AppController ; //forward declaration

typedef boost::shared_ptr<AppController> AppControllerPtr ;
typedef ACE_Message_Queue<ACE_MT_SYNCH> ACE_SHARED_MQ ;

struct FailoverInfo
{
    std::string m_ScenarioId;
    std::string m_AppType;
    std::string m_SolutionType;
    std::string m_InstanceName;
    std::string m_PolicyType;
    std::string m_FailoverType;
    std::string m_ProductionServerName;
    std::string m_DRServerName;
    std::string m_TagName;
    std::string m_TagType;
    std::string m_DNSFailover;
    std::string m_ADFailover;
    std::string m_MovePublicFolders;
    std::string m_RepointAllStores;
    std::string m_PrevPublicFolderServer;
    std::string m_CurrentPublicFolderServer;
    std::string m_ProductionVirtualServerName;
    std::string m_DRVirtualServerName;
    std::string m_RecoveryExecutor;
    std::string m_Prescript;
    std::string m_Postscript;
    std::string m_Customscript;
    std::string m_VacpCmdLine;
    std::string m_RecoveryPointType;
    std::string m_IsOracleRac;
    std::string m_AppConfFilePath;
    std::string m_ConvertToCluster;
    std::string m_SrcHostIp;
    std::string m_SrcVirtualServerIp;
    std::string m_ProductionServerIp;
    std::string m_DRServerIp;
    std::string m_ProductionVirtualServerIp;
    std::string m_DRVirtualServerIp;
    std::string m_ProtecionDirection;
    std::string m_startApplicationService;
    std::string m_ExtraOptions;
  
};



class AppController : public sigslot::has_slots<>,public ACE_Task<ACE_MT_SYNCH>
{
	std::string m_appType ;
    ACE_SHARED_MQ m_MsgQueue ;
public:

    FailoverInfo m_FailoverInfo;
    static FailoverJobs m_FailoverJobs;
    bool bIsClusterNodeActive;
    bool  m_ProtectionSettingsChanged ;
    std::map<std::string,AppProtectionSettings> m_AppProtectionsettingsMap;
    FailoverPolicyData m_FailoverData;
    SV_ULONG m_policyId; 
    ApplicationPolicy m_policy ;
    AppProtectionSettings m_AppProtectionSettings;
    SourceSQLDiscoveryInformation m_SrcDiscInfo;
    static ACE_Recursive_Thread_Mutex m_lock;
    typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuard;
    AppController(ACE_Thread_Manager* thrMgr) 
    : ACE_Task<ACE_MT_SYNCH>(thrMgr)
    {

    }
    //Thread specific pure virtual functions.
    virtual int open(void *args = 0) = 0 ;
    virtual int close(u_long flags = 0) = 0 ;
    virtual int svc() = 0 ;

    //Application Logic processing
    virtual void ProcessMsg(SV_ULONG policyId) = 0;
    
	//virtual 	SVERROR Process(std::list<ApplicationPolicy>&);
    
	void ProcessRequests() ;
    void PostMsg(int priority, SV_ULONG policyId) ;
    ACE_SHARED_MQ& Queue() ;
	SVERROR prepareTarget ( );
 
    void Init() ;
    void GetFailoverInformation();
    SVERROR GetProtectionSettings(const std::string& scenarioId="");
	enum PAIR_STATE { 
		PAIR_STATE_RESYNC_I,
		PAIR_STATE_RESYNC_II,
		PAIR_STATE_DIFF_SYNC 
	};
	SVERROR GetPairsOfState(std::string scenarioId,PAIR_STATE pairState,std::map<std::string, std::string> & pairs);
    SVERROR GetFailoverData();
    SVERROR GetSourceDiscoveryInformation();
    void BuildFailoverCommand(std::string& failoverCmd);
    virtual void PrePareFailoverJobs() = 0;
    virtual void UpdateConfigFiles() = 0 ;
    virtual bool IsItActiveNode() = 0;
    
    void UpdateFailoverJobsToCx();
    void BuildDNSCommand(std::string& dnsCmd);	
	void BuildClusReConstructionCommand(std::string& clusConstructionCmdStr);
    void  CreateSectionAndKeysOfConfFile(std::string const & confFile);
    void SetCustomVolumeInfoToConfigValueObj();
    void SetAppVolumeInfoToConfigValueObj();
    void SetRecoveryPointInfoToConfigValueObj();
	void SetReplPairStatusInfoToConfigValueObj();
    void CreateAppConfFile();
    SVERROR RunFailoverJob();
    //static bool getFailoverJob(SV_ULONG policyId, FailoverJob&) ;
	static void AddFailoverJob(SV_ULONG policyId, const FailoverJob&) ;
    static FailoverJobs getFailoverJobsMap();
    void SetSQLInstanceInfoToConfigValueObj();
    std::string GetTagType(const std::string& appType);
    bool RunClusterReconstructionSteps(FailoverJob& failoverJob);
    void RemoveFailoverJobFromCache(SV_ULONG policyId) ;
    virtual void MakeAppServicesAutoStart() = 0 ;
    virtual void PreClusterReconstruction() = 0 ;
    virtual bool StartAppServices() = 0;
    void getSourceHostNameForFileCreation(std::string& srcFileName);
    bool isFailoverInformationRequired(SV_ULONG policyId);
    bool checkPairsInDiffsync(const std::string& scenarioId,std::string& outputFileName);
    bool getDiffSyncVolumesList(const std::string& scenarioId, std::string& outputFileName, std::list<std::string>& diffSyncVolumeList);
    bool isPairRemoved();
    void BuildWinOpCommand(std::string& winOpCmd, bool bConsiderAllZones = true);
} ;
#endif
