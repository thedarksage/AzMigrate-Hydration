#ifndef POLICY_CONFIG_H
#define POLICY_CONFIG_H
#include <boost/shared_ptr.hpp>

#include "apppolicyobject.h"
#include "policyinstanceobj.h"
#include "confschema.h"
#include "svtypes.h"
#include "VolumeHandler.h"
#include "scenarioconfig.h"
#include "appglobals.h"
#include "policyhandler.h"

class PolicyConfig ;
typedef boost::shared_ptr<PolicyConfig> PolicyConfigPtr ;

class PolicyConfig
{
    ConfSchema::ApplicationPolicyObject m_policyAttrNamesObj ;
    ConfSchema::AppPolicyInstanceObject m_polInstanceAttrNamesObj ;
    ConfSchema::Group* m_policyGroup ;
    ConfSchema::Group* m_policyInstanceGrp ;
    bool m_isChanged ;
    
    void loadPolicyConfig() ;
    
    bool GetScheduleType( SV_ULONGLONG policyId, SV_UINT& scheduleType ) ;
    
    bool IsPolicyAvailable( SV_ULONGLONG policyId ) ;
    
    bool GetPolicyObject( SV_ULONGLONG policyId,
                          ConfSchema::Object** policyObj ) ;

    bool GetPolicyInstanceObj( SV_ULONGLONG policyId, SV_LONGLONG instanceId, ConfSchema::Object** policyInstanceObj, bool matchInstanceId) ;

    
    void UpdateRunOncePolicy( SV_ULONGLONG policyId, 
                              SV_LONGLONG policyInstanceId,
                              const std::string& status,
                              const std::string& log ) ;

    void UpdateRunEveryPolicy( SV_ULONGLONG policyId, 
                               SV_LONGLONG policyInstanceId,
                               const std::string& status,
                              const std::string& log ) ;

    void PopulateConsistencyCmd( const ConsistencyOptions& consistencyOptions,
                                 const std::list<std::string>& volumes,
                                 std::string& consistencyCmd) ;

	void GetVolpackPolicyData(const std::string& srcVol,
							  const std::string& tgtVol,
                              const std::map<std::string,volumeProperties>& volpropmap,
                              VOLPACK_ACTION volpackAction,
                              std::string& volpackPolicyData) ;

    std::string GetPolicyType(ConfSchema::Object* policyObj) ;

    void EnablePolicy( ConfSchema::Object* policyObj, bool runOnce) ;

    bool IsPolicyEnabled(ConfSchema::Object* policyObj) ;
    
    void DisablePolicy(ConfSchema::Object* policyObj) ;

    PolicyConfig() ;

    void RemoveOldInstances(SV_ULONGLONG policyId) ;
    static PolicyConfigPtr m_policyConfigPtr ;
    SV_ULONGLONG GetNewPolicyInstanceId(SV_ULONGLONG policyId) ;
    SV_ULONGLONG GetNewPolicyId() ;
    bool GetPolicyObjectsGroup(ConfSchema::Group** policyObjsGrp) ;
    bool GetPolicyGlobalParamsGroup(ConfSchema::Group** globalParamsGrp) ;
public:
    POLICYUPDATE_ENUM UpdatePolicy(SV_ULONGLONG policyId, 
                      SV_LONGLONG policyInstanceId, 
                      const std::string& status,
                      const std::string& log) ;

    SV_ULONGLONG CreateConsistencyPolicy(const ConsistencyOptions& consistencyOptions,
                                    const std::list<std::string>& volumes,
                                    int scenarioId,
                                    bool runEvery,
                                    bool enabled = true) ;

    void CreateVolumeProvisioningPolicy(const std::map<std::string, std::string>& srcTgtVolMap,
                                           const std::map<std::string,volumeProperties>& volpropmap,
                                           int scenarioId) ;

    void CreateVolumeUnProvisioningPolicy(const std::map<std::string, std::string>& srcTgtVolMap,
                                             const std::map<std::string,volumeProperties>& volpropmap,
                                             int scenarioId) ;
    bool IsPolicyEnabled(SV_ULONGLONG policyId) ;

    void EnablePolicy(SV_ULONGLONG policyId, bool runOnce) ;

    void DisablePolicy(SV_ULONGLONG policyId) ;

    void GetPolicyType(SV_ULONGLONG policyId, std::string& policyType) ;

    VOLPACK_ACTION GetVolPackAction(SV_ULONGLONG policyid) ;
	void GetSourceVolume(SV_ULONGLONG policyid, std::string& volumename) ;
    bool isModified() ;

    void EnableConsistencyPolicy() ;
    bool PoliciesAvailable() ;
    void EnableVolumeUnProvisioningPolicy() ;
    static PolicyConfigPtr GetInstance() ;

    void UpdateConsistencyPolicy(const ConsistencyOptions& consistencyOptions,
                                 const std::list<std::string>& volumes) ;

    void ClearPolicies() ;
    bool isConsistencyPolicyEnabled() ;
	std::list <std::string> GetVolumesInVolumeUnProvisioningPolicyData (SV_UINT policyID);
} ;
#endif