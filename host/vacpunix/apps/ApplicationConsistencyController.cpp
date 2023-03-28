#include <string>
#include <list>

#include "ApplicationConsistencyProvider.h"
#include "OracleConsistencyProvider.h"
#include "Db2ConsistencyProvider.h"
#include "MysqlConsistencyProvider.h"
#include "CustomConsistencyProvider.h"
#include "ApplicationConsistencyController.h"

std::list<ApplicationConsistencyProviderPtr> appConsistencyProviderPtrList;

ApplicationConsistencyControllerPtr ApplicationConsistencyController::m_instancePtr;
boost::mutex ApplicationConsistencyController::m_instanceMutex;

ApplicationConsistencyController::ApplicationConsistencyController()
{

    // 1710 - Bug 1747820: [V2A] Disable oracle and DB2 app consistency on Linux
    // disabling app consistency until the apps are certified
    ApplicationConsistencyProviderPtr appConExecPtr;
#ifdef ENABLE_APP_CONSISTENCY

    appConExecPtr = OracleConsistencyProvider::getInstance();
    appConsistencyProviderPtrList.push_back(appConExecPtr);

    appConExecPtr = Db2ConsistencyProvider::getInstance();
    appConsistencyProviderPtrList.push_back(appConExecPtr);

    appConExecPtr = MysqlConsistencyProvider::getInstance();
    appConsistencyProviderPtrList.push_back(appConExecPtr);

#endif

    // 1901 - enable custom consistency
    appConExecPtr = CustomConsistencyProvider::getInstance();
    appConsistencyProviderPtrList.push_back(appConExecPtr);

}

ApplicationConsistencyController::~ApplicationConsistencyController()
{

}

ApplicationConsistencyControllerPtr ApplicationConsistencyController::getInstance()
{
    if (m_instancePtr.get() == NULL)
    {
        boost::unique_lock<boost::mutex> lock(m_instanceMutex);
        if (m_instancePtr.get() == NULL)
        {
            m_instancePtr.reset(new ApplicationConsistencyController);
        }
    }

    return m_instancePtr;
}

bool ApplicationConsistencyController::Discover()
{
    std::list<ApplicationConsistencyProviderPtr>::iterator iter = appConsistencyProviderPtrList.begin(); 
    while (iter != appConsistencyProviderPtrList.end())
    {
        ApplicationConsistencyProviderPtr appConExecPtr = *iter;
        // if (! appConExecPtr->Discover())
            // return false;
        ++iter;
    }
    return true;
}

bool ApplicationConsistencyController::Freeze()
{
    std::list<ApplicationConsistencyProviderPtr>::iterator iter = appConsistencyProviderPtrList.begin(); 
    while (iter != appConsistencyProviderPtrList.end())
    {
        ApplicationConsistencyProviderPtr appConExecPtr = *iter;
        if (!appConExecPtr->Freeze())
            return false;
        ++iter;
    }
    return true;
}

bool ApplicationConsistencyController::Unfreeze()
{
    std::list<ApplicationConsistencyProviderPtr>::iterator iter = appConsistencyProviderPtrList.begin(); 
    while (iter != appConsistencyProviderPtrList.end())
    {
        ApplicationConsistencyProviderPtr appConExecPtr = *iter;
        if(!appConExecPtr->Unfreeze())
            return false;
        ++iter;
    }
    return true;
}
