#include "policyinstanceobj.h"

namespace ConfSchema
{
    AppPolicyInstanceObject::AppPolicyInstanceObject()
    {
        /* TODO: all these below are hard coded (taken from
         *       schema document)                      
         *       These have to come from some external
         *       source(conf file ?) ; Also curretly we use string constants 
         *       so that any instance of volumesobject gives 
         *       same addresses; 
         *       Not using std::string members and static volumesobject 
         *       for now since calling constructors of statics across 
         *       libraries is causing trouble (no constructors getting 
         *       called) */
        m_pName = "PolicyInstance";
        m_pPolicyIdAttrName = "policyId";
        m_pInstanceIdAttrName = "instanceId";
        m_pStatusAttrName = "status";
        m_pExecutionLogAttrName = "executionLog";
        m_pUniqueIdAttrName = "uniqueId";
        m_pDependentInstanceIdAttrName = "dependentInstanceId" ;
        m_pScenarioIdAttrName = "scenarioId" ;

    }
}
