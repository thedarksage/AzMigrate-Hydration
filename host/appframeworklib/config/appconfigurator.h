#ifndef APPCONFIGURATOR_H
#define APPCONFIGURATOR_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>
#include <string>

#include "talwrapper.h"
#include "proxy.h"
#include "applocalconfigurator.h"
#include "applicationsettings.h"
#include "configurator2.h"
#include "xmlizeapplicationsettings.h"
#include "serializeapplicationsettings.h"
#include "applicationsettings.h"
#include "cxupdatecache.h"
#include "task.h"
#include "serializationtype.h"
#include "apiwrapper.h"
#include "serializer.h"
#include "appconfchangemediator.h"

class ApplicationAgentInfo ;

#define APP_HASH_LENGTH	16

//maximum value of policyId sent by CS is INT_MAX
//Internal policies
#define RESUME_REPLICATION_POLICY_INTERNAL (1000000)

class ConfigureApplicationAgent
{
public:
    virtual void test() = 0 ;
    virtual void UpdateDiscoveryInfo(const DiscoveryInfo& discoveryInfo,SV_ULONG policyId,SV_ULONGLONG instanceId) = 0 ;
    virtual bool postMirrorInfo(const std::string& , const std::string& ) = 0;
    virtual bool postMirrorStatusInfo( const std::string& ) = 0;
    virtual bool postForceMirrorDeleteInfo( const std::string& ) = 0;
    virtual bool postLunMapInfo(const std::string&) = 0;
    virtual void GetApplicationSettings(ApplicationSettings&) = 0 ;
    virtual void persistAppSettings(const ApplicationSettings&) = 0 ;
    virtual void UpdateReadinessCheckInfo(const std::list<ReadinessCheckInfo>& readinessCheckInfo,SV_ULONG policyId,SV_ULONGLONG instanceId) = 0 ;
    virtual void UpdateUnregisterApplicationInfo(SV_ULONG policyId,SV_ULONGLONG instanceId,std::string strErrorCode,std::string strLog) = 0;
	virtual void PolicyUpdate(SV_ULONG policyId, SV_ULONGLONG instanceId, std::string strErrorCode, std::string strLog, std::string postMsg = "", bool noLogUpload = false) = 0;
    virtual SVERROR PostUpdate(const std::string& str,std::string& responseString,SV_ULONG policyId,SV_ULONGLONG instanceId)=0;
    virtual void UpdateFailoverJobs(const FailoverJob& failoverJob,SV_ULONG policyId,SV_ULONGLONG instanceId) = 0;
	virtual SVERROR uploadFileToCX( const SV_ULONG& policyId, int groupId, const SV_ULONGLONG& instanceId, unsigned long& bytesSent ) = 0;
	virtual SVERROR transportToCX( const SV_ULONG& policyId, const int& groupId, const SV_ULONGLONG& instanceId, const int& offset, const std::string& Data, unsigned long& retValue ) = 0;
    virtual std::string getTgtOracleInfo(std::string &instanceHome) = 0;
    virtual bool registerApplicationAgent(const std::string& hostId,
		const std::string& Agentversion,
		const std::string& HostName,
		const std::string& HostIP,
		const std::string& InstallPath,
		SV_ULONG compatabilityNum,
		const std::string& timeZone,
		const std::string& PatchHistory,
		const std::string& ProdVersion,
		const SV_ULONG& osval,
		const Object& osinfo,
		const SV_ULONG& endianness) = 0;
} ;


class ConfigureApplicationAgentProxy : public ConfigureApplicationAgent
{
public:
    ConfigureApplicationAgentProxy(ConfiguratorDeferredProcedureCall dpc,
                                    ConfiguratorMediator t,
                                    AppLocalConfigurator& cfg,
                                    ACE_Recursive_Thread_Mutex& m,
                                    const SerializeType_t serializetype) 
    :m_serializeType(serializetype),
    m_dpc(dpc),
    m_localConfigurator(cfg),    
    m_transport(t),
    m_lock(m)
    {
    }
    void test() ;
    void UpdateDiscoveryInfo(const DiscoveryInfo& discoveryInfo,SV_ULONG policyId,SV_ULONGLONG instanceId) ;
    bool postMirrorInfo(const std::string& , const std::string& );
    bool postMirrorStatusInfo(const std::string& );
    bool postForceMirrorDeleteInfo( const std::string& );
    bool postLunMapInfo(const std::string&);
    void persistAppSettings(const ApplicationSettings&) ;
    void GetApplicationSettings(ApplicationSettings&) ;
    void UpdateReadinessCheckInfo(const std::list<ReadinessCheckInfo>& readinessCheckInfo,SV_ULONG policyId,SV_ULONGLONG instanceId) ;
    void UpdateUnregisterApplicationInfo(SV_ULONG policyId,SV_ULONGLONG instanceId,std::string strErrorCode,std::string strLog);
	void PolicyUpdate (SV_ULONG policyId,SV_ULONGLONG instanceId, std::string strErrorCode,std::string strLog, std::string updateMsg = "", bool noLogUpload = false);

    SVERROR PostUpdate(const std::string& str,std::string& responseString,SV_ULONG policyId,SV_ULONGLONG instanceId);
    void UpdateFailoverJobs(const FailoverJob& failoverJob,SV_ULONG policyId,SV_ULONGLONG instanceId) ;
	SVERROR uploadFileToCX( const SV_ULONG& policyId, int groupId, const SV_ULONGLONG& instanceId, unsigned long& bytesSent );
    std::string getTgtOracleInfo(std::string &instanceName);
private:
    SerializeType_t m_serializeType;
    AppLocalConfigurator& m_localConfigurator;
    ConfiguratorDeferredProcedureCall const m_dpc;
    ConfiguratorMediator const m_transport;
    ACE_Recursive_Thread_Mutex& m_lock;
	unsigned char m_defaultDiscoveryHashp[16];
	unsigned char m_registerApplicationAgentHashp[APP_HASH_LENGTH];
	SVERROR transportToCX( const SV_ULONG& policyId, const int& groupId, const SV_ULONGLONG& instanceId, const int& offset, const std::string& Data, unsigned long& retValue );
     void setApiAndFirstArg(const char *apiname,
                                const char *dpc, 
                                const char **apitocall,
                                const char **firstargtoapi) const;
     bool registerApplicationAgent(const std::string& hostId,
		const std::string& Agentversion,
		const std::string& HostName,
		const std::string& HostIP,
		const std::string& InstallPath,
		SV_ULONG compatabilityNum,
		const std::string& timeZone,
		const std::string& PatchHistory,
		const std::string& ProdVersion,
		const SV_ULONG& osval,
		const Object& osinfo,
		const SV_ULONG& endianness);
	 // 
	 // Description: 
	 // 	  Verifies the pending updates for a give policyId.
	 // Perameters:
	 // 	  policyId         - policy Id
	 // 	  pendingUpdates   - list of pending updates available in cache
	 // 	  policyProperties - policy properties map.
	 // Return values: 
	 //    If the policyId found in pending updates then it returns true. Otheswise false.
	 // 
	 bool VerifyPendingUpdates(SV_ULONG policyId, std::list<CxRequestCache>& pendingUpdates, std::map<std::string, std::string>& policyProperties);
} ;


class CxAppProxy
{
public:
    void RegisterApplicationAgent(ApplicationAgentInfo&) ;
    void UnregisterApplicationAgent() ;
    void GetArrayInfos(std::string &info) const;
    void GetRegisteredArrays(std::string &info) const;
    bool PostStorageArrayInfoToReg(const std::string &info) const;
    bool PostLunInfoToReg(const std::string &info) const;
    bool PostLunMapInfo(const std::string &info) const;
    bool PostMirrorConfigInfo(const std::string &info) const;
} ;


class ApplicationCxProxy : CxAppProxy
{
public:
    ApplicationCxProxy(std::string hostId,ConfiguratorMediator t,AppLocalConfigurator& cfg,
                ACE_Recursive_Thread_Mutex& mutex,const SerializeType_t serializetype) :
                m_serializeType(serializetype),
                m_transport( t ),
                m_hostId( hostId ),
                m_localConfigurator( cfg )
                
                
    {
        std::string     biosId("");
        GetBiosId(biosId);

        m_dpc =  marshalCxCall("::getVxAgent", m_hostId, biosId);
        m_appAgentPtr.reset(new ConfigureApplicationAgentProxy(m_dpc,t,cfg,mutex,m_serializeType)) ;
    }
    void RegisterApplicationAgent(ApplicationAgentInfo&) ;
    void UnregisterApplicationAgent() ;
    boost::shared_ptr<ConfigureApplicationAgentProxy> m_appAgentPtr ;

    void GetArrayInfos(std::string &info) const
    {
        // info =  unmarshal<std::string> (m_transport( marshalCxCall( "::getArrayInfo", m_localConfigurator.getHostId())));
         const char *api = "::getArrayInfo";
         Serializer sr(m_serializeType);
         sr.Serialize( api, m_localConfigurator.getHostId());
         m_transport( sr );
         info = sr.UnSerialize<std::string> ();
    } 

    bool PostStorageArrayInfoToReg(const std::string &info) const
    {
       // (void) m_transport( marshalCxCall( "::postArrayInfoToRegister", m_localConfigurator.getHostId(),info));
         const char *api = "::postArrayInfoToRegister";
         Serializer sr(m_serializeType);
         sr.Serialize( api, m_localConfigurator.getHostId(),info);
         m_transport( sr );
         return true;    
    }

    bool GetCxInfoToRegisterWithArray(std::string &info) const
    {
       //  info =  unmarshal<std::string> (m_transport( marshalCxCall( "::getCxInfo", m_localConfigurator.getHostId())));
         
         const char *api = "::getCxInfoToRegisterWithArray";
         Serializer sr(m_serializeType);
         sr.Serialize( api, m_localConfigurator.getHostId());
         m_transport( sr );
         info = sr.UnSerialize<std::string> ();
         return true;
    }

    bool PostCxRegWithArrayResponse(const std::string &info) const
    {
        //(void) m_transport( marshalCxCall( "::postCxRegWithArrayResponse", m_localConfigurator.getHostId(),info));
         const char *api = "::postCxInfoToRegisterWithArray";
         Serializer sr(m_serializeType);
         sr.Serialize( api, m_localConfigurator.getHostId(),info);
         m_transport( sr );
         return true;
    }

    bool GetCxInfoToDeregisterWithArray(std::string &info) const
    {
         const char* api = "::getCxInfoToDeregisterWithArray" ;
         std::string marshalledstr = marshalCxCall( api, m_localConfigurator.getHostId()) ;
         Serializer sr(m_serializeType) ;
         sr.Serialize( api, m_localConfigurator.getHostId()) ;
         m_transport( sr ) ;
         info = sr.UnSerialize<std::string>() ;
         //info =  unmarshal<std::string> (m_transport( marshalCxCall( "::getCxInfoToDeregisterWithArray", m_localConfigurator.getHostId())));
         return true;
    }

    bool PostCxDeregWithArrayResponse(const std::string &info) const
    {
         const char* api = "::postCxInfoToDeregisterWithArray" ;
         Serializer sr(m_serializeType);
         sr.Serialize( api, m_localConfigurator.getHostId(),info);
         m_transport( sr ) ;
        //(void) m_transport( marshalCxCall( "::postCxInfoToDeregisterWithArray", m_localConfigurator.getHostId(),info));
         return true;
    } 

    void GetRegisteredArrays(std::string &info) const
    {
         //info =  unmarshal<std::string> (m_transport( marshalCxCall( "::getRegisteredArrayInfo", m_localConfigurator.getHostId())));
         const char *api = "::getRegisteredArrayInfo";
         Serializer sr(m_serializeType);
         sr.Serialize( api, m_localConfigurator.getHostId());
         m_transport( sr );
         info = sr.UnSerialize<std::string> ();
    }

    bool PostLunInfoToReg(const std::string &info) const
    {
        //return unmarshal<bool>  (m_transport( marshalCxCall( "::postLunInfoToRegister", m_localConfigurator.getHostId(),info)));
        const char *api = "::postLunInfoToRegister";
        Serializer sr(m_serializeType);
        sr.Serialize( api, m_localConfigurator.getHostId(),info);
        m_transport( sr );
        return sr.UnSerialize<bool> ();
    }
private:
    SerializeType_t m_serializeType;
    ConfiguratorMediator m_transport;
    ApplicationCxProxy();
    std::string m_hostId;
    AppLocalConfigurator& m_localConfigurator;
    ConfiguratorDeferredProcedureCall m_dpc;
} ;
typedef boost::shared_ptr<ApplicationCxProxy> ApplicationCxProxyPtr ;


class AppAgentConfigurator ;
typedef boost::shared_ptr<AppAgentConfigurator> AppAgentConfiguratorPtr ;

class AppAgentConfigurator
{
private:
    SerializeType_t m_serializeType;
    ApiWrapper m_apiWrapper;
    AppLocalConfigurator m_localConfigurator ;
    ApplicationCxProxyPtr m_cx ;
    ConfigSource m_configSource ;
    ApplicationSettings m_settings ;
    /*
        TODO : 
        We need to use the below times 
        m_lastTimeSettingsFetchedFromCx to issue fail over signal
        m_lastTimeSettingsFetchedFromCache to avoid reading the cache if cache timestamp is not changed
    */
    ACE_Time_Value m_lastTimeSettingsFetchedFromCx ;
    ACE_Time_Value m_lastTimeSettingsFetchedFromCache ;

protected:
    AppAgentConfigurator(const SerializeType_t &serializetype,const std::string& ip, 
                    int port,
                    const std::string& hostId) ;

    AppAgentConfigurator(const SerializeType_t & serializetype,enum ConfigSource confSource) ;
public:
    SerializeType_t getSerializationType();
    ~AppAgentConfigurator() ;
    ApplicationSettings GetCachedApplicationSettings() ;
    ConfigureApplicationAgent& GetAppAgent() ;
    ApplicationCxProxy& GetCXAgent() ;    
    ApplicationSettings GetApplicationSettings();
    void CacheConsistencySettings(ApplicationSettings& appSettings);
    bool postMirrorInfo(const std::string& , const std::string& );
    bool postMirrorStatusInfo(const std::string& );
    bool postForceMirrorDeleteInfo( const std::string& );
    bool postLunMapInfo(const std::string&);
    bool isCxReachable(const std::string& ip, const SV_UINT& port) ;
    
    ACE_Time_Value lastTimeSettingsGotFromCX() ;
    ACE_Time_Value lastTimeSettingsGotFromCache() ;
    ConfigSource getConfigSource() ;
    void UpdateDiscoveryInfo(const DiscoveryInfo&,SV_ULONG policyId,SV_ULONGLONG instanceId) ;
    void UpdateReadinessCheckInfo(const std::list<ReadinessCheckInfo>& readinessCheckInfo,SV_ULONG policyId,SV_ULONGLONG instanceId) ;
    void UpdateUnregisterApplicationInfo(SV_ULONG policyId,SV_ULONGLONG instanceId,std::string strErrorCode,std::string strLog);
	void PolicyUpdate(SV_ULONG policyId, SV_ULONGLONG instanceId, std::string strErrorCode, std::string strLog, std::string postMsg = "", bool noLogUpload = false);

	SVERROR perisitClusterResourceInfo(const std::list<RescoureInfo>& ) ;
    void UpdateReadinessCheckInfo(const ReadinessCheckInfo& readinessCheckInfo,SV_ULONG policyId,SV_ULONGLONG instanceId) ;
    void persistAppSettings(const ApplicationSettings&) ;
    void switchToStandbyCx(const SerializeType_t & serializetype,ReachableCx reachableCx) ;
    SVERROR SerializeSchedulingInfo(const AppSchedulingInfo&) ;
    SVERROR UnserializeSchedulingInfo(std::map<SV_ULONG, TaskInformation> &) ;
	SVERROR UnserializeClusterResourceInfo(std::list<RescoureInfo>& );
    void SerializeApplicationSettings(const ApplicationSettings& settings) ;
	bool getApplicationPolicies(SV_ULONG policyId,
								std::string appType,
								ApplicationPolicy & policy);
	void findInstancesForDefaultDiscovery(const std::string& appType, 
													      std::list<std::string>& instanceList,
                                                          std::map<std::string, std::list<std::string> > instancesMap) ;
    SVERROR getApplicationPolicyType(SV_ULONG policyId, std::string& policyType) ;
	SVERROR getApplicationPolicyPropVal(SV_ULONG policyId,const std::string & policy_prop, std::string & prop_value);
    static AppAgentConfiguratorPtr CreateAppAgentConfigurator(const SerializeType_t & serializetype,ConfigSource confSource) ;
    static AppAgentConfiguratorPtr CreateAppAgentConfigurator(const SerializeType_t & serializetype) ;
	static AppAgentConfiguratorPtr CreateAppAgentConfigurator( const std::string& ipAddress, const SV_INT& portNo, const std::string& hostId,const SerializeType_t & serializetype);
    static AppAgentConfiguratorPtr GetAppAgentConfigurator() ;
    static AppAgentConfiguratorPtr m_configuratorInstance ;
    void init() ;
    void start(ConfigChangeMediatorPtr configChangeMediatorPtr) ;
	void WriteUpdateIntoFile(std::string update);
	SVERROR getApplicationType(SV_ULONG policyId, std::string& AppType);
	SV_ULONG GetRequestStatus(SV_ULONG policyId);
    void UpdatePendingAndFailedRequests();
	bool isThisProtected(const std::string& scenarioId);
    void getTasksFromConfiguratorCache(std::map<SV_ULONG, TaskInformation> &);
    void getAppProtectionPolicies(std::map<std::string,AppProtectionSettings>& protectionsettings);
    std::string getCacheDirPath() const;
    SVERROR SerializeFailoverJobs(const FailoverJobs& failoverJobs );
    SVERROR UnserializeFailoverJobs(FailoverJobs& failoverJobs);
    void UpdateFailoverJobs(const FailoverJob& failoverJob,SV_ULONG policyId,SV_ULONGLONG instanceId);
	void RemovePolicyFromCache(SV_ULONG& policyId, bool force=false);
    void UpdatePolicyInCache(SV_ULONG& policyId);
    void ChangeTransport(const ReachableCx& reachableCx,const SerializeType_t serializeType) ;
    std::string getTgtOracleInfo(std::string &instanceName);
    bool registerApplicationAgent();
} ;

#endif
