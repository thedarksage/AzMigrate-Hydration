	#ifndef ORACLE_APPLICATION_H
#define ORACLE_APPLICATION_H

#include "app/application.h"
//#include "Oraclemetadata.h"
#include "OracleDiscovery.h"
//#include "Oraclemetadatadiscovery.h"
#include <boost/shared_ptr.hpp>
#include "ruleengine/validator.h"
#include <list>

class OracleApplication : public Application
{
    boost::shared_ptr<OracleDiscoveryImpl> m_oracleDiscoveryImplPtr ;
    //boost::shared_ptr<OracleMetaDataDiscoveryImpl> m_oraclemetaDataDiscoveryImplPtr ;
    bool m_discoverDependencies ;
    //std::list<std::string> m_evsList ;
    std::list<std::string> m_oracleHosts ;
    std::list<std::string> m_oracleDatabases;
    std::list<std::string> m_allInstanceNameList ;
    bool m_isInstalled ;
public:
    OracleApplication();
    //OracleApplication(bool DiscoverDependetHosts=true) ;
    //OracleApplication(std::list<std::string> evsNamesList,bool DiscoverDependetHosts=true ) ;    

    //std::map<std::string, OracleMetaData> m_oracleMetaDataMap ;
	//std::map<std::string, OracleMetaData> m_oracleMetaDataMap ;
    std::map<std::string, OracleAppVersionDiscInfo> m_oracleDiscoveryInfoMap ;
    OracleAppDiscoveryInfo m_oracleAppDiscoveryInfo;

    InmProtectedAppType m_appType ;
    SVERROR init();//const std::string& dnsname) ;
    virtual bool isInstalled() ;
    virtual SVERROR discoverApplication() ;
    virtual SVERROR discoverMetaData() ;
	virtual SVERROR validateApplicationProtection() ;
	virtual SVERROR validateApplicationFailOver() ;
	virtual SVERROR validateApplicationFailBack() ;
	void getOracleAppDiscoveryInfo(OracleAppDiscoveryInfo& oracleDiscInfo){oracleDiscInfo = m_oracleAppDiscoveryInfo;}
    void prepareOracleHostsList() ;
    void clean();
    std::string summary(std::list<ValidatorPtr>& list) ;
    std::list<std::string> getActiveInstances() ;
    std::map<std::string, std::list<std::string> > getVersionInstancesMap() ;
} ;
typedef boost::shared_ptr<OracleApplication> OracleApplicationPtr ;

#endif
