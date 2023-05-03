#ifndef SQLDISCOVERYANDVALIDATOR_H
#define SQLDISCOVERYANDVALIDATOR_H
#include "appglobals.h"
#include "discoveryandvalidator.h"
#include "mssqlapplication.h"

class SQLDiscoveryAndValidator : public DiscoveryAndValidator
{
	MSSQLApplicationPtr m_SqlApp ;
	void PostDiscoveryInfoToCx( std::list<std::string>&) ;
    SVERROR Discover(bool shouldUpdateCx) ;
    SVERROR PerformReadinessCheckAtSource() ;
    SVERROR PerformReadinessCheckAtTarget() ;
	SVERROR fillSQLDiscData(const std::string&, std::map<std::string, std::string>&, const bool& );
	SVERROR fillSQLDiscMetaData(const std::string&, std::map<std::string, MSSQLDBMetaDataInfo>&, const bool&);
    bool isSQL2000ServicesRunning(std::list<std::string>& sql2000Services);
    bool IsItActiveNode() ;
public:
    SQLDiscoveryAndValidator(const ApplicationPolicy& policy) ;
} ;


#endif
