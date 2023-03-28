#ifndef BULK_PROTECTION_CONTROLLER_H
#define BULK_PROTECTION_CONTROLLER_H
#include "util/volpack.h"
#include "storagerepository/storagerepository.h"
#include "app/appcontroller.h"
#include "configwrapper.h"

class BulkProtectionController;
typedef boost::shared_ptr<BulkProtectionController>BulkProtectionControllerPtr;

class BulkProtectionController : public AppController
{
	//ApplicationPolicy m_policy; 
    //FailoverInfo m_FailoverInfo;
	SVERROR  Process() ;
	BulkProtectionController(ACE_Thread_Manager*)  ;
	SVERROR Consistency( );
      static BulkProtectionControllerPtr m_BPControllerPtr ;
	bool m_bCluster;  
    int m_volpackIndex ;
	//Use this conficurator carefully. Initialize it when required and
	//     uninitialze immmediatly once the task is done with configurator.
	Configurator * m_pConfig;
	bool InitializeConfig(ConfigSource configSrc = USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE);
	void UninitializeConfig();
	std::string GetRetentionDbPath(std::string volumeName);
public:

    virtual int open(void *args = 0) ;
    virtual int close(u_long flags = 0) ;
    virtual int svc() ;
    void ProcessMsg(SV_ULONG policyId) ;
    static BulkProtectionControllerPtr getInstance(ACE_Thread_Manager* tm) ;
    void UpdateConfigFiles()
	{	
		//
	}
    void PrePareFailoverJobs();
    bool IsItActiveNode();
    void MakeAppServicesAutoStart() ;
    void PreClusterReconstruction() ;
    bool StartAppServices();
    SVERROR createVolPack(SV_ULONG policyId, VolPack objVolPack, std::map<std::string,std::string>& mountPointToDeviceFileMap, SV_ULONG& exitCode);
    SVERROR deleteVolPack(SV_ULONG policyId, VolPack& objVolPack, SV_ULONG& exitCode);
    SVERROR resizeVolPack(SV_ULONG policyId, VolPack& objVolPack, SV_ULONG& exitCode);
    SVERROR ProcessVolPack(SV_ULONG policyId);
	SVERROR executeScript(const SV_ULONG& policyId );
	SVERROR setupRepository(SV_ULONG policyId);
	SVERROR setupRepositoryV2(SV_ULONG policyId) ;
	SVERROR createConfFile(ScriptPolicyDetails, std::string);
} ;
#endif