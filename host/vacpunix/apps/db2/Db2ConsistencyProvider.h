#ifndef DB2CONSISTENCYPROVIDER_H
#define DB2CONSISTENCYPROVIDER_H

#include <string>
#include <map>
#include <boost/thread.hpp>
#include "ApplicationConsistencyProvider.h"


class Db2DiscoveryInfo: public ApplicationDiscoveryInfo
{
    std::string instanceName; 
    std::string username; 
};

class Db2ConsistencyProvider ;

typedef boost::shared_ptr<Db2ConsistencyProvider> Db2ConsistencyProviderPtr;

class Db2ConsistencyProvider: public ApplicationConsistencyProvider
{
    static Db2ConsistencyProviderPtr m_instancePtr;
    static boost::mutex m_instanceMutex;

    std::map<std::string, ApplicationDiscoveryInfoPtr>  appInfo;
    std::string installPath;
    std::string discoveryOutputFile;
    std::string logFile;
    bool    bDbsFrozen;

public:
    bool Discover();
    bool Freeze();
    bool Unfreeze();

    Db2ConsistencyProvider();
    ~Db2ConsistencyProvider();

    static Db2ConsistencyProviderPtr getInstance();
};

#endif
