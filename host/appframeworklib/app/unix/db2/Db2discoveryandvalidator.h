#ifndef DB2DISCOVERYANDVALIDATOR_H
#define DB2DISCOVERYANDVALIDATOR_H

#include "discovery/unix/discoveryandvalidator.h"
#include "Db2Application.h"
#include "applicationsettings.h"

class Db2DiscoveryAndValidator : public DiscoveryAndValidator
{
	SVERROR PostDiscoveryInfoToCx();
    SVERROR Discover(bool shouldUpdateCx) ;
    SVERROR PerformReadinessCheckAtSource() ;
    SVERROR PerformReadinessCheckAtTarget() ;
    SVERROR UnregisterDB() ;
	Db2ApplicationPtr m_Db2AppPtr ;
    void convertToDb2DiscoveryInfo(const Db2AppDiscoveryInfo& db2DiscoveryInfo, Db2DatabaseDiscoveryInfo& db2discoveryInfo ) ;    

public:
    Db2DiscoveryAndValidator(const ApplicationPolicy& policies) ;
} ;

#endif
