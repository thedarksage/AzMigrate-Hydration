#include <boost/shared_ptr.hpp>
#include "appcontroller.h"

class AppFactory ;
typedef boost::shared_ptr<AppFactory> AppFactoryPtr ;
class AppFactory
{
    static AppFactoryPtr m_instancePtr ;
public:
    static AppFactoryPtr GetInstance() ;
    AppControllerPtr CreateAppController(ACE_Thread_Manager* tm, InmProtectedAppType appType) ;
    AppControllerPtr CreateAppController(ACE_Thread_Manager* tm, const std::string &appType, const std::set<std::string>& selectedAppNameSet) ;
    std::list<AppControllerPtr> getApplicationControllers(ACE_Thread_Manager* tm) ;
} ;