#ifndef EXCHANGE_DISCOVERY_H
#define EXCHANGE_DISCOVERY_H
#include "ldap/exchangeldaphelper.h"
#include "service.h"

struct ExchAppVersionDiscInfo
{    
    InmProtectedAppType m_appType ;
    std::string m_cn ;
    std::string m_dn ;
    std::string m_version ;
    std::string m_installPath ;
    std::string m_dataPath ;
    SV_UINT m_serverCurrentRoles ;
    SV_UINT m_clusterStorageType ;
    SV_UINT m_serverRole ;
    bool m_isMta; 
    bool m_isClustered ;
    std::string m_adminstrativeGrpName ;
    std::string m_organizationName ;
    std::string m_transportSendProtocolLogPath ;
    std::string m_transportRecvProtocolLogPath ;
    std::string m_transportPickupDirPath ;
    std::string m_transportMsgTrackingPath ;
    std::string m_transportReplyDirPath ;
    std::string m_transportConnectivityLogPath ;
    std::string m_auditLogPath ;
    std::string m_pipelineTracingPath ;
    std::string m_transportRoutingLogPath ;
    std::list<InmService> m_services ;
    std::list<InmProcess> m_processes ;
    std::list<std::pair<std::string, std::string> > getPropertyList() ;
    std::string summary();
    std::string ServerCurrentRoleToString() const ;
    std::string ServerRoleToString()  const ;
    std::string ClusterStorageTypeToStr() const ;
    std::string m_errString;
	std::string m_nMaxMailStores;
	std::string m_edition;
	long m_errCode;
    ExchAppVersionDiscInfo()
    {
        m_errCode = 0 ;
        m_serverCurrentRoles = 0 ;
        m_isMta = false ;
    }
} ;

struct ExchApplDiscoveryInfo
{
    std::list<ExchAppVersionDiscInfo> m_exchAppVerDiscInfoList ;
    //std::list<InmService> m_services ;
    //std::list<InmProcess> m_processes ;
    std::string summary() ;
} ;

class ExchangeDiscoveryImpl
{
    std::string m_hostname ;
    std::string m_dnsname ;
    boost::shared_ptr<ExchangeLdapHelper> m_ldapHelper ;
    //void prepareSupportedExchangeVersionsList(std::list<InmProtectedAppType>& list) ;
    SVERROR discoverExchangeVersionInfo(ExchAppVersionDiscInfo&) ;
    //SVERROR discoverExchVersionServices(std::list<InmService>& serviceList) ;
    SVERROR discoverExchAppLevelServices(std::list<InmService>& serviceList, InmProtectedAppType ) ;
public:
    SVERROR init(const std::string& dnsname) ;    
    void Display(const ExchAppVersionDiscInfo& appInfo) ;
    SVERROR discoverExchangeApplication(const std::string& exchHost, ExchAppVersionDiscInfo&, bool bDiscSvs = true) ;
    bool isInstalled(const std::string& exchHost) ;
    bool isInstalled(std::list<std::string>& exchHosts) ;
    void SetHostName(const std::string& hostname) ;
    static boost::shared_ptr<ExchangeDiscoveryImpl> m_instancePtr ;
    static boost::shared_ptr<ExchangeDiscoveryImpl> getInstance() ;
} ;
#endif
