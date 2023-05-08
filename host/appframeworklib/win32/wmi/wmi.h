#ifndef __WMI__H
#define __WMI__H
#include <string>
#include <list>
#include "appglobals.h"
#include "wmiutil.h"
#define _WIN32_DCOM
#include <iostream>
#include <comdef.h>
#include <Wbemidl.h>
#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>

typedef std::list<std::list<std::string> > ResultSet ;

class WMIConnector
{

    IWbemServices* m_WbemServices ;
    IWbemLocator* m_WbemLocator ;
    std::string m_objectNameSpace ;
    typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuard;
    static ACE_Recursive_Thread_Mutex m_lock ;
public:
    ~WMIConnector() ;
    WMIConnector() ;
    SVERROR ConnectToServer(const std::string& hostName, const std::string& objectPath) ;
    SVERROR execQuery(const std::wstring& query, IEnumWbemClassObject** pEnumerator) ;
    SVERROR execQuery(const std::string& query, IEnumWbemClassObject** ) ;
} ;
#endif