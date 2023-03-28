#ifndef APP_SETTINGS__HANDLER__H_
#define APP_SETTINGS__HANDLER__H_

#include "Handler.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "volumesobject.h"
#include "apppolicyobject.h"
#include "inmapihandlerdefs.h"
#include "appscenarioobject.h"
#include "planobj.h"
#include "pairsobj.h"
#include "scenarioretentionpolicyobj.h"
#include "retevtpolicyobject.h"
#include "retwindowobject.h"
#include "retlogpolicyobject.h"
#include "replicaionoptionsobj.h"
#include "autoresyncobj.h"
#include "pairinfoobj.h"
#include "scenarioretentionwindowobj.h"
#include "scenariodetails.h"
#include "applicationsettings.h"
#include "policyinstanceobj.h"

class AppSettingsHandler :
	public Handler
{
public:
	AppSettingsHandler(void);
	~AppSettingsHandler(void);

	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    INM_ERROR GetApplicationSettings(FunctionInfo &f);
private:
    bool GetPolicyObjsGroup(ConfSchema::Group* policyConfigGrp,
                            ConfSchema::Group** policyObjsGrp) ;
    void ValidateAndUpdateRequestPGForApplicationSettings(FunctionInfo &f);
    void ReadGroupsAndFillApplicationSettings(ConstParameterGroupsIter_t & policyTimeStampPGIter,ParameterGroup & reponsePG);
    void FillApplicationSettings(ConstParameterGroupsIter_t & policyTimeStampPGIter,ParameterGroup & pg);
    void FillApplicationPolicySettings(ParameterGroup &appPolicyPG,ConstParameterGroupsIter_t &policyTimeStampPGIter);
    void UpdateApplicationPolicySettingsPG(int srcIndex,
                                            ConfSchema::Object &objIter,
                                            ParameterGroup & appPoliciesPG,
                                            ConstParameterGroupsIter_t &policyTimeStampPGIter,
                                            std::list<std::string> & deletedpolicies);
    void UpdateApplicationPolicyPropertiesPG(ParameterGroup &propertyPG,
                                             ConfSchema::Object &objIter,
                                             ConstParameterGroupsIter_t &policyTimeStampPGIter,
                                             std::list<std::string> & deletedpolicies);
    bool isPolicyNeedToSend(const std::string & policyId,
                                            const std::string &timeStamp,
                                            const std::string &scheduleType,
                                            ConstParameterGroupsIter_t & policyTimeStampPGIter);
    void FillApplicationProtectionSettings(ParameterGroup &protecionSettingsPG);
    void UpdateApplicationProtectionSettingsPG(ConfSchema::Object &objIter,ParameterGroup & protecionSettingsPG);
    void UpdateApplicationProtectionPropertiesPG(ParameterGroup &propertyPG,ConfSchema::Object &objIter);
    void FillApplicationProtecionAppVolumesPG(ParameterGroup &protecionAppVolumesPG);
    void UpdateApplicationProtecionAppVolumesPG(int srcIndex,ConfSchema::Object & pairObject,ParameterGroup &protecionAppVolumesPG);
    void FillApplicationProtecionPairPG(ParameterGroup &protectionPairPG);
    void UpdateApplicationProtecionPairPG(int srcIndex,ConfSchema::Object & pairObject,ParameterGroup &protectionPairPG);
    void FillApplicationReplicationPairPG(ParameterGroup &replicationPairPG,ConfSchema::Object & pairObject);
    void UpdateApplicationReplicationPairPG(ParameterGroup &replicationPairPropetiesPG,ConfSchema::Object &pairObject);
    void GetDeletedPoliciesList(ConfSchema::Group &appPolicyGroup,ConstParameterGroupsIter_t &policyTimeStampPGIter,std::list<std::string> & deletedpolicies);

    ConfSchema::Group *m_pAppPolicyInstanceGroup;
    ConfSchema::AppPolicyInstanceObject m_AppPolicyInstanceObj;

    ConfSchema::Group *m_pAppPolicyGroup;
    ConfSchema::ApplicationPolicyObject m_AppPolicyObj;

    ConfSchema::Group *m_pPlanGroup;
    ConfSchema::PlanObject m_PlanObj;
    
    ConfSchema::Group *m_pAppScenarioGroup;
    ConfSchema::AppScenarioObject m_AppScenarionObj;

    ConfSchema::Group *m_pProtectionPairGroup;
    ConfSchema::Group *m_pVolumesGroup;
    
    ConfSchema::ProtectionPairObject m_ProtectionPairObj;

    ConfSchema::Group *m_pPairGroup;
    ConfSchema::PairsObject m_PairObject;
};


#endif /* APP_SETTINGS__HANDLER__H_ */
