#include "app/appfactory.h"
#include "appglobals.h"
#include "oracle/Oraclecontroller.h"
#include "Array/arraycontroller.h"
#include "db2/Db2Controller.h"
#include "util/common/bulkProtection.h"
#include "util/common/cloudcontroller.h"
#include "appexception.h"
#include "controller.h"
#include "util/common/util.h"

AppFactoryPtr AppFactory::m_instancePtr ;

std::list<AppControllerPtr> AppFactory::getApplicationControllers(ACE_Thread_Manager* ptr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<AppControllerPtr> appControllers ;
    AppControllerPtr appControllerPtr ;

    appControllerPtr = OracleController::getInstance(ptr);
    appControllers.push_back(appControllerPtr);

    appControllerPtr = ArrayController::getInstance(ptr);
    appControllers.push_back(appControllerPtr);

    appControllerPtr = Db2Controller::getInstance(ptr);
    appControllers.push_back(appControllerPtr);
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return appControllers ;
}


AppControllerPtr AppFactory::CreateAppController(ACE_Thread_Manager* ptr, InmProtectedAppType appType)
{
    AppControllerPtr appControllerPtr ;
    switch( appType )
    {
		case INM_APP_BULK :
			appControllerPtr = BulkProtectionController::getInstance(ptr);
			break;

		case INM_APP_ORACLE:
			appControllerPtr = OracleController::getInstance(ptr);
			break;

		case INM_APP_ARRAY:
			appControllerPtr = ArrayController::getInstance(ptr);
			break;

        case INM_APP_DB2:
            appControllerPtr = Db2Controller::getInstance(ptr);
            break;

		case INM_APP_CLOUD:
			appControllerPtr = CloudController::getInstance(ptr);

        case INM_APP_NONE :
        default:
            break ;
    }
    if( appControllerPtr.get() != NULL )
    {
        if( -1 == appControllerPtr->open() )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "FAILED AppFactory::CreateAppController failed to start the appspecific thread: %d\n", 
                ACE_OS::last_error()) ;
            throw AppException("AppFactory::CreateAppController failed to start the appspecific thread\n") ;
        }
    }
    return appControllerPtr ;
}

AppControllerPtr AppFactory::CreateAppController(ACE_Thread_Manager* ptr, const std::string& appTypeinStr, const std::set<std::string>& selectedAppNameSet)
{
    if(isAvailableInTheSet(appTypeinStr, selectedAppNameSet) == false)
    {
        bool bThrow = true;
        if(strcmpi(appTypeinStr.c_str(), "BULK") == 0 || 
                strcmpi(appTypeinStr.c_str(), "SYSTEM") == 0)
        {
            std::string tempApp = "BULK";
            if(strcmpi(appTypeinStr.c_str(), "BULK") == 0)
            {
                tempApp = "SYSTEM";
            }
            if(isAvailableInTheSet(tempApp, selectedAppNameSet))
            {
                bThrow = false;
            }
        }
        if(bThrow)
        {
            DebugPrintf(SV_LOG_DEBUG, "The application name: %s is not specified in drscout.conf to support. \n", appTypeinStr.c_str());
            throw AppException("The application name is not specified in drscout.conf to support.");        
        }
    }
    InmProtectedAppType appType;
    if(appTypeinStr.find("BULK") == 0 || 
            appTypeinStr.find("SYSTEM") == 0  )
    {
        appType = INM_APP_BULK;
    }
    if(appTypeinStr.find("ORACLE") == 0)
    {
        appType = INM_APP_ORACLE;
    }
    if(appTypeinStr.find("ARRAY") == 0 || appTypeinStr.find("Array") == 0)
    {
        appType = INM_APP_ARRAY;
    }
    if(appTypeinStr.find("DB2") == 0)
    {
        appType = INM_APP_DB2;
    }
	if(appTypeinStr.find("CLOUD") == 0)
    {
        appType = INM_APP_CLOUD;
    }

    return CreateAppController(ptr, appType);
}
AppFactoryPtr AppFactory::GetInstance()
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new AppFactory() ) ;
    }
    return m_instancePtr ;
}
