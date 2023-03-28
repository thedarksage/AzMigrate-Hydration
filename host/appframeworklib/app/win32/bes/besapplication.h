#ifndef __BES_APPLICATION__
#define __BES_APPLICATION__

#include <boost/shared_ptr.hpp>
#include "app/application.h"
#include "besdiscovery.h"

class BESAplication;

typedef boost::shared_ptr<BESAplication> BESAplicationPtr ;


class BESAplication : public Application
{
public:
	boost::shared_ptr<BESAplicationImpl> m_BESAplicationImplPtr ;
	BESDiscoveryInfo m_discoveredData;
    BESMetaData m_discoveredMetaData;
    bool m_bInstalled;

    BESAplication();
	std::string getSummary() ;

    virtual bool isInstalled() ;
    virtual SVERROR discoverApplication(); 
	virtual SVERROR discoverMetaData();
    void dumpDiscoveredInfo();
    void clean();
};

#endif