#ifndef POLICY__HANDLER__H_
#define POLICY__HANDLER__H_

#include "Handler.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "volumesobject.h"
#include "apppolicyobject.h"
#include "inmapihandlerdefs.h"
#include "appscenarioobject.h"
#include "policyinstanceobj.h"
#include "planobj.h"
#include "pairsobj.h"
#include "pairinfoobj.h"
#include "scenariodetails.h"
#include "applicationsettings.h"



enum POLICYUPDATE_ENUM
{
    APP_POLICY_UPDATE_COMPLETED = 0,
    APP_POLICY_UPDATE_FAILED,
    APP_POLICY_UPDATE_DELETED,
    APP_POLICY_UPDATE_DUPLICATE
};


class PolicyHandler :
	public Handler
{
    void UpdateScenarioByVolumeProvisionResult(SV_UINT policyID,
												const std::string& srcVolume,
												const std::string & policyStaus,
											   VOLPACK_ACTION volpackAction,
                                               const VolPackUpdate& volpackUpdate) ;

public:
	PolicyHandler(void);
	~PolicyHandler(void);

	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    INM_ERROR PolicyUpdate(FunctionInfo &f);
	void SendPolicyAlert(const std::string& policyType,
                         SV_UINT policyId, 
                         SV_LONGLONG instanceId, 
                         const std::string& planName, 
                         const std::string& status) ;

} ;

#endif /* POLICY__HANDLER__H_ */
