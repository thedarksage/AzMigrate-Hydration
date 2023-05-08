#ifndef EXCHANGE_METADATA_DISCOVERY_H
#define EXCHANGE_METADATA_DISCOVERY_H
#include "appglobals.h"
#include <boost/shared_ptr.hpp>
#include "ldap/exchangeldaphelper.h"
enum DiscoveryOption
{
	DISCOVER_ALL,
	DISCOVER_PUBLIC_STORES,
	DISCOVER_PRIVATE_STORES
} ;
class ExchangeMetaDataDiscoveryImpl
{
    boost::shared_ptr<ExchangeLdapHelper> m_ldapHelper ;
    SVERROR getMDBMetaData(const std::string storageGrpName, const std::string& mdbName, const ExchangeMdbType& mdbType, ExchangeMDBMetaData& mdbMetaData) ;
	SVERROR get2010MDBMetaData(const std::string& mdbName, const ExchangeMdbType& mdbType, ExchangeMDBMetaData& mdbMetaData) ;
    SVERROR getStorageGroupMetaData(const std::string storageGrpName, ExchangeStorageGroupMetaData& storageGrpMetaData, enum DiscoveryOption=DISCOVER_ALL) ;
    std::string m_hostname ;
public:
    SVERROR init(const std::string& dnsname) ;
    SVERROR getExchangeMetaData(const std::string& exchHost, ExchangeMetaData& metadata,enum DiscoveryOption=DISCOVER_ALL) ;
	SVERROR getExchange2k10MetaData(const std::string& exchHost, Exchange2k10MetaData& metadata,enum DiscoveryOption=DISCOVER_ALL) ;
    SVERROR Display(const ExchangeMetaData& metadata) ;
	void dumpMetaData( const Exchange2k10MetaData& metadata ) ;
    static boost::shared_ptr<ExchangeMetaDataDiscoveryImpl> getInstance() ;
    static boost::shared_ptr<ExchangeMetaDataDiscoveryImpl> m_instance ;
} ;
#endif