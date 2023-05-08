#ifndef __INPUTVALUE_OBJ_
#define __INPUTVALUE_OBJ_
#include <ace/ACE.h>
#include <set>
#include "appglobals.h"
class ConfigValueObj
{	
	std::set<InmProtectedAppType> m_applications ;
	bool m_runOnconsole ;
	
    static ConfigValueObj m_confValueObjInstance ;
public:
	ConfigValueObj();

	void setApplication(InmProtectedAppType) ;
    void clearApplications() 
    {
        m_applications.clear() ;
    }
	void setRunOnconsole(bool ) ;
	
	bool getRunOnconsole() ;
	std::set<InmProtectedAppType> getApplications() ;
    static ConfigValueObj& getInstance();
    static bool parse(ACE_TCHAR** argv, int argc) ;	
    bool isApplicationSelected(InmProtectedAppType);
} ;

void usage();
#endif