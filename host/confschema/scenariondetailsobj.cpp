#include "scenariodetails.h"

namespace ConfSchema
{
    ScenraionDetailsObject::ScenraionDetailsObject()
    {
        m_pName = "ScenarioDetails";
        m_pRunReadinessCheckAttrName = "runReadinessCheck";
        m_pSoultionTypeAttrName = "solutionType";
        m_pRecoveryFeatureAttrName = "recoveryFeature";
        m_pActionOnNewVolumeDiscoveryAttrName = "actionOnNewVolumeDiscovery";
        m_pTargetHostId = "TargetHostId" ;
        m_pSrcHostName = "SourceHostName" ;
        m_pTgtHostName  = "TargetHostName" ;
        m_pSrcHostId = "SourceHostId" ;
    }
}
