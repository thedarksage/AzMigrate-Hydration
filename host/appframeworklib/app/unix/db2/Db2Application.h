#ifndef DB2_APPLICATION_H
#define DB2_APPLICATION_H

#include "app/application.h"
#include "Db2Discovery.h"
#include <boost/shared_ptr.hpp>
#include "ruleengine/validator.h"
#include <list>

class Db2Application : public Application
{
    boost::shared_ptr<Db2DiscoveryImpl> m_db2DiscoveryImplPtr ;    
    bool m_discoverDependencies ;
    std::list<std::string> m_db2Hosts ;
    std::list<std::string> m_db2Databases;
    std::list<std::string> m_allInstanceNameList ;
    bool m_isInstalled ;
public:
    Db2Application();
   
    std::map<std::string, Db2AppInstanceDiscInfo> m_db2DiscoveryInfoMap ;
    Db2AppDiscoveryInfo m_db2AppDiscoveryInfo;

    InmProtectedAppType m_appType ;
    SVERROR init();
    virtual bool isInstalled() ;
    virtual SVERROR discoverApplication() ;
    virtual SVERROR discoverMetaData() ;
	virtual SVERROR validateApplicationProtection() ;
	virtual SVERROR validateApplicationFailOver() ;
	virtual SVERROR validateApplicationFailBack() ;
	void getDb2AppDiscoveryInfo(Db2AppDiscoveryInfo& db2DiscInfo){db2DiscInfo = m_db2AppDiscoveryInfo;}
    void prepareDb2HostsList() ;
    void clean();
    std::string summary(std::list<ValidatorPtr>& list) ;
    std::list<std::string> getActiveInstances() ;
    std::map<std::string, std::list<std::string> > getVersionInstancesMap() ;
} ;
typedef boost::shared_ptr<Db2Application> Db2ApplicationPtr ;

#endif
