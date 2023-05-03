#include "autoresyncobj.h"

namespace ConfSchema
{
    AutoResyncObject::AutoResyncObject()
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
        m_pName = "AutoResync";
        m_pStateAttrName = "state";
        m_pBetweenAttrName = "between";
        m_pWaitTimeAttrName = "waitTime";
    }
}