#include "app/appfactory.h"
#include "appglobals.h"
#include "mssql/mssqlcontroller.h"
#include "exchange/exchangecontroller.h"
#include "fileserver/fileservercontroller.h"
#include "util/common/bulkProtection.h"
#include "util/common/cloudcontroller.h"
#include "appexception.h"
#include "controller.h"
#include "util/common/util.h"

AppFactoryPtr AppFactory::m_instancePtr ;

std::list<AppControllerPtr> AppFactory::getApplicationControllers(ACE_Thread_Manager* tm)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<AppControllerPtr> appControllers ;
    AppControllerPtr appControllerPtr ;

	appControllerPtr = MSExchangeController::getInstance(tm); 

    appControllers.push_back(appControllerPtr) ;

	appControllerPtr = MSSQLController::getInstance(tm) ;

    appControllers.push_back(appControllerPtr) ;    

	appControllerPtr = FileServerController::getInstance(tm);
    appControllers.push_back(appControllerPtr) ;    

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return appControllers ;
}


AppControllerPtr AppFactory::CreateAppController(ACE_Thread_Manager* tm, 
                                                 InmProtectedAppType appType)
{
    AppControllerPtr appControllerPtr ;
    
    switch( appType )
    {
        case INM_APP_EXCHNAGE :
        case INM_APP_EXCH2003 :
        case INM_APP_EXCH2007 :
		case INM_APP_EXCH2010 :
				appControllerPtr = MSExchangeController::getInstance(tm);
            break ;

        case INM_APP_MSSQL :
        case INM_APP_MSSQL2005 :
        case INM_APP_MSSQL2000 :
        case INM_APP_MSSQL2008 :
		case INM_APP_MSSQL2012 :
			appControllerPtr = MSSQLController::getInstance(tm);
            break ;

        case INM_APP_FILESERVER :
			appControllerPtr = FileServerController::getInstance(tm);
            break ;

		case INM_APP_BULK :
			appControllerPtr = BulkProtectionController::getInstance(tm);
			break;

		case INM_APP_CLOUD:
			appControllerPtr = CloudController::getInstance(tm);

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
    else
    {
        throw AppException("Invalid APplication type specified.. Unable to create appcontroller") ;
    }
    return appControllerPtr ;
}

AppControllerPtr AppFactory::CreateAppController(ACE_Thread_Manager* tm,
                                                 const std::string& appTypeinStr, 
                                                 const std::set<std::string>& selectedAppNameSet)
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
	if(appTypeinStr.find("EXCHANGE") == 0)
	{
		appType = INM_APP_EXCHNAGE;
	}
	else if(appTypeinStr.find("SQL") == 0)
	{
		appType = INM_APP_MSSQL;
	}
	else if(appTypeinStr.find("BULK") == 0 || 
	        appTypeinStr.find("SYSTEM") == 0 )
	{
		appType = INM_APP_BULK;
	}
	else if(appTypeinStr.find("FILESERVER") == 0)
	{
		appType = INM_APP_FILESERVER;
	}
	else if(appTypeinStr.find("CLOUD") == 0)
	{
		appType = INM_APP_CLOUD;
	}

	return CreateAppController(tm,appType);
}
AppFactoryPtr AppFactory::GetInstance()
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new AppFactory() ) ;
    }
    return m_instancePtr ;
}