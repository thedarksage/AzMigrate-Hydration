#ifndef LICENCING__HANDLER__H
#define LICENCING__HANDLER__H

#include "Handler.h"
#include "confschema.h"
#include "confschemamgr.h"
#include "planobj.h"
#include "appscenarioobject.h"
#include "scenariodetails.h"
#include "pairinfoobj.h"
#include "pairsobj.h"
#include "scenarioretentionpolicyobj.h"
#include "scenarioretentionwindowobj.h"
#include "consistencypolicyobj.h"
#include "apppolicyobject.h"
#include "protectionpairobject.h"
#include "retwindowobject.h"

class LicencingHandler :
	public Handler
{
private:
    INM_ERROR GetNodeLockString(FunctionInfo& request) ;
    INM_ERROR OnLicenseExpiry(FunctionInfo& request) ;
	INM_ERROR OnLicenseExtension(FunctionInfo& request) ;
	INM_ERROR UpdateProtectionPolicies(FunctionInfo& request);
	INM_ERROR IsLicenseExpired(FunctionInfo& request) ;
public:
	LicencingHandler(void);
	~LicencingHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
};


#endif  /* LICENCING__HANDLER__H */
