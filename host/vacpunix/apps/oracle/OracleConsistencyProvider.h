#ifndef ORACLECONSISTENCYPROVIDER_H
#define ORACLECONSISTENCYPROVIDER_H

#include <string>
#include <map>
#include <boost/thread.hpp>
#include "ApplicationConsistencyProvider.h"


class OracleDiscoveryInfo: public ApplicationDiscoveryInfo
{
    std::string instanceName; // SID
    std::string username; // oracle user name
};

class OracleConsistencyProvider ;

typedef boost::shared_ptr<OracleConsistencyProvider> OracleConsistencyProviderPtr;

class OracleConsistencyProvider: public ApplicationConsistencyProvider
{
    static OracleConsistencyProviderPtr m_instancePtr;
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

    OracleConsistencyProvider();
    ~OracleConsistencyProvider();

    static OracleConsistencyProviderPtr getInstance();
};

#endif
