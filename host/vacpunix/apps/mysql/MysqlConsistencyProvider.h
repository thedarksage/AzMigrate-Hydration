#ifndef MYSQLCONSISTENCYPROVIDER_H
#define MYSQLCONSISTENCYPROVIDER_H

#include <string>
#include <map>
#include <boost/thread.hpp>
#include "ApplicationConsistencyProvider.h"


class MysqlDiscoveryInfo: public ApplicationDiscoveryInfo
{
    std::string instanceName;
    std::string username; // my sql username
};

class MysqlConsistencyProvider ;

typedef boost::shared_ptr<MysqlConsistencyProvider> MysqlConsistencyProviderPtr;

class MysqlConsistencyProvider: public ApplicationConsistencyProvider
{
    static MysqlConsistencyProviderPtr m_instancePtr;
    static boost::mutex m_instanceMutex;

    std::map<std::string, ApplicationDiscoveryInfoPtr>  appInfo;
    std::string installPath;
    std::string discoveryOutputFile;
    std::string logFile;
public:
    bool Discover();
    bool Freeze();
    bool Unfreeze();

    MysqlConsistencyProvider();
    ~MysqlConsistencyProvider();

    static MysqlConsistencyProviderPtr getInstance();
};

#endif
