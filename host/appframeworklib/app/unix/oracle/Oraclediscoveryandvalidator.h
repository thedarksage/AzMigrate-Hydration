#ifndef ORACLEDISCOVERYANDVALIDATOR_H
#define ORACLEDISCOVERYANDVALIDATOR_H
#include "discovery/unix/discoveryandvalidator.h"
#include "Oracleapplication.h"
#include "applicationsettings.h"
class OracleDiscoveryAndValidator : public DiscoveryAndValidator
{
    SVERROR PostDiscoveryInfoToCx();//(const std::list<std::string>& instancesList,const std::list<std::string>&) ;
    SVERROR Discover(bool shouldUpdateCx) ;
    SVERROR PerformReadinessCheckAtSource() ;
    SVERROR PerformReadinessCheckAtTarget() ;
    SVERROR UnregisterDB() ;
	OracleApplicationPtr m_OracleAppPtr ;
    void convertToOracleDiscoveryInfo(const OracleAppDiscoveryInfo& oracleDiscoveryInfo, 
                                                                    OracleDiscoveryInfo& oraclediscoveryInfo ) ;
    /*void convertToOracleDiscoveryInfo(const ExchAppVersionDiscInfo& oracleDiscoveryInfo, 
                                                                    const OracleMetaData& oracleMetadata, 
                                                                    OracleDiscoveryInfo oraclediscoveryInfo ) ;*/

public:
    OracleDiscoveryAndValidator(const ApplicationPolicy& policies) ;
} ;

#endif
