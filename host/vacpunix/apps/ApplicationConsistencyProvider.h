#ifndef APPLICATIONCONSISTENCYPROVIDER_H
#define APPLICATIONCONSISTENCYPROVIDER_H

#include <string>
#include <list>
#include <boost/shared_ptr.hpp>

class ApplicationConsistencyProvider
{
public:
    virtual bool Discover() = 0;
    virtual bool Freeze() = 0;
    virtual bool Unfreeze() = 0;
};

typedef boost::shared_ptr<ApplicationConsistencyProvider> ApplicationConsistencyProviderPtr;

class ApplicationDiscoveryInfo
{
    std::string             appName;
    std::list<std::string>  filterDevices;
    std::list<std::string>  fileSystemMountPoints;
};

typedef boost::shared_ptr<ApplicationDiscoveryInfo> ApplicationDiscoveryInfoPtr;

#endif
