#ifndef MSSQL_METADATA_DISCOVERY_H
#define MSSQL_METADATA_DISCOVERY_H
#include "appglobals.h"
#include <boost/shared_ptr.hpp>
#include "sqldiscovery.h"
#include "mssqlmetadata.h"

class MSSQLMetaDataDiscoveryImpl
{

public:
    SVERROR discoverMetaData(const MSSQLDiscoveryInfo&, MSSQLMetaData&) ;
    static boost::shared_ptr<MSSQLMetaDataDiscoveryImpl> getInstance() ;
    static boost::shared_ptr<MSSQLMetaDataDiscoveryImpl> m_instancePtr ;
} ;
#endif