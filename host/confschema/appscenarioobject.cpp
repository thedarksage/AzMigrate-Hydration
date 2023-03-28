#include "appscenarioobject.h"

namespace ConfSchema
{
    AppScenarioObject::AppScenarioObject()
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
        m_pName = "Scenario";
        m_pScenarioIdAttrName = "scenarioId";
        m_pScenarioTypeAttrName = "scenarioType";
        m_pScenarioNameAttrName = "scenarioName";
        m_pScenarioDescriptionAttrName = "scenarioDescription";
        m_pScenarionVersionAttrName = "scenarioVersion";
        m_pExecutionStatusAttrName = "executionStatus";
        m_pApplicationTypeAttrName = "applicationType";
        m_pReferenceIdAttrName = "referenceId";
        m_pIsPrimaryAttrName = "isPrimary";
		m_pIsSystemPaused = "isSystemPaused";
		m_pSystemPauseReason = "SyetmPauseReason" ;
    }
}
