#include "apppolicyobject.h"

namespace ConfSchema
{
    ApplicationPolicyObject::ApplicationPolicyObject()
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
        m_pName = "Policy";
        m_pIdAttrName = "id";
        m_pTypeAttrName = "type";
        m_pContextAttrName = "context";
        m_pScheduleTypeAttrName = "scheduleType";
        m_pIntervalAttrName = "interval";
        m_pExcludeFromAttrName = "excludeFrom";
        m_pExcludeToAttrName = "excludeTo";
        m_pHostIdAttrName = "hostId";
        m_pPolicyDataAttrName = "policyData";
        m_pScenarioIdAttrName = "scenarioId";
        m_pPolicyTimestampAttrName = "policyTimestamp";
        m_pConsistencyCommandAttrName = "ConsistencyCmd";
		m_pDoNotSkipAttrName = "DoNotSkip";
        m_pSuccessRunsAttrName = "SuccesRuns" ;
        m_pFailedRunsAttrName = "FailedRuns" ;
        m_pTotalRunsAttrName = "TotalRuns" ;
        m_pSkippedRunsAttrName = "SkippedRuns" ;
		m_pGetVolpacksforVolumesAttrName = "Volumes" ;
    }
}
