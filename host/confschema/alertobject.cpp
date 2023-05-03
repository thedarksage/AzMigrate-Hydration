#include "alertobject.h"

namespace ConfSchema
{
    AlertObjcet::AlertObjcet()
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
        m_pName = "Alerts";
        m_pAlertTimestampAttrName = "alertTimestamp";
        m_pLevelAttrName = "level";
        m_pTypeAttrName = "type";
        m_pMessageAttrName = "message";
        m_pModuleAttrName = "module";
        m_pFixStepsAttrName = "fixSteps";
        m_pCountAttrName = "count";
    }
}
