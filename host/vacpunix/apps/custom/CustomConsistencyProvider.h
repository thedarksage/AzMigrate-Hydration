#ifndef CUSTOMCONSISTENCYPROVIDER_H
#define CUSTOMCONSISTENCYPROVIDER_H

#include <string>
#include <set>
#include <map>
#include <boost/thread.hpp>
#include "ApplicationConsistencyProvider.h"


class CustomConsistencyProvider ;

typedef boost::shared_ptr<CustomConsistencyProvider> CustomConsistencyProviderPtr;

class CustomConsistencyProvider: public ApplicationConsistencyProvider
{
    static CustomConsistencyProviderPtr m_instancePtr;
    static boost::mutex m_instanceMutex;

    std::string m_scriptPath;
    std::string m_logFile;

    CustomConsistencyProvider();

    bool Run(const std::string& action); 
    void RunCommand(const std::string& command); 

public:
    bool Discover();
    bool Freeze();
    bool Unfreeze();

    ~CustomConsistencyProvider();

    static CustomConsistencyProviderPtr getInstance();
};

#endif
