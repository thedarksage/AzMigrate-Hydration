#ifndef EXCHANGE_APPLICATION_H
#define EXCHANGE_APPLICATION_H

#include <boost/shared_ptr.hpp>
#include <list>
#include <set>
#include "ruleengine/validator.h"
#include "app/application.h"
#include "exchangemetadata.h"
#include "exchangediscovery.h"
#include "exchangemetadatadiscovery.h"

class ExchangeApplication : public Application
{
    boost::shared_ptr<ExchangeDiscoveryImpl> m_exchaDiscoveryImplPtr ;
    boost::shared_ptr<ExchangeMetaDataDiscoveryImpl> m_exchametaDataDiscoveryImplPtr ;
    bool m_discoverDependencies ;
    std::list<std::string> m_evsList ;
    std::list<std::string> m_exchHosts ;
    std::list<std::string> m_allInstanceNameList ;
    bool m_isInstalled ;
    void prepareExchHostsList() ;
    SVERROR getEVSNamesList(std::list<std::string>& activeInstanceNameList, std::list<std::string>& passiveInstanceNameList);

public:
    ExchangeApplication(bool DiscoverDependetHosts=true) ;
    ExchangeApplication(std::list<std::string> evsNamesList,bool DiscoverDependetHosts=true ) ;    
    ExchangeApplication(const std::string& evsName, bool DiscoverDependetHosts=true) ;
    std::map<std::string, ExchangeMetaData> m_exchMetaDataMap ;
	std::map<std::string, Exchange2k10MetaData> m_exch2k10MetaDataMap ;
    std::map<std::string, ExchAppVersionDiscInfo> m_exchDiscoveryInfoMap ;

    InmProtectedAppType m_appType ;
    SVERROR init(const std::string& dnsname) ;
    virtual bool isInstalled() ;
    bool isInstalledInDomain(std::list<std::string>& exchHosts) ;
    virtual SVERROR discoverApplication() ;
    virtual SVERROR discoverMetaData() ;
    void clean();
    std::string summary(std::list<ValidatorPtr>& list) ;
    std::list<std::string> getInstances() ;
    std::list<std::string> getActiveInstances() ;
    std::map<std::string, std::list<std::string> > getVersionInstancesMap() ;
    static void getdependentaPrivateMailServers(std::set<std::string>& mailServers, const std::string& pfs) ;
} ;
typedef boost::shared_ptr<ExchangeApplication> ExchangeApplicationPtr ;

#endif