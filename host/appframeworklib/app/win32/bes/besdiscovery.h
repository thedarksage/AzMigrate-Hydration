#ifndef __BES_DISCOVERY__
#define __BES_DISCOVERY__
#include "appglobals.h"
#include "service.h"
#include <sstream>

enum BES_SUPPORTED_VERSIONS
{
	BES_4_1,
	BES_5_0
};
struct BESDiscoveryInfo
{
    std::list<InmService> m_services ;

    std::string m_configServerName;
    std::string m_installVersion;
    std::string m_installPath;
    std::string m_configInstallType;
    std::string m_configKeyStoreUserName;
    std::string m_installUser;
    SV_LONGLONG m_no_of_agents;
    std::string m_no_of_Acitveagents;
    std::list<std::pair<std::string, std::string> > getProperties() ;
    std::string getSummary();
};

struct BESMetaData
{
    std::string  m_dataBaseName;
    std::string  m_dataBseServerMachineName;
    std::list<std::string>  m_exchangeServerNameList;
    std::list<std::pair<std::string, std::string> > getProperties() ;
    std::string getSummary();
};

class BESAplicationImpl
{
public:
    SVERROR discoverBESData( BESDiscoveryInfo& );
    SVERROR discoverBESMetaData( BESMetaData& );
    SVERROR discoverServices(std::list<InmService>&, BES_SUPPORTED_VERSIONS );
};

#endif
