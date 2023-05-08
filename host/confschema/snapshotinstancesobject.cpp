#include "snapshotinstancesobject.h"

namespace ConfSchema
{
    SnapshotInstancesObject::SnapshotInstancesObject()
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
        m_pName = "SnapshotInstances";
        m_pSnapshotIdAttrName = "snapshotId";
        m_pExecutionStateAttrName = "executionState";
        m_pProgressAttrName = "progress";
        m_pLastUpdateTimeAttrName = "lastUpdateTime";
        m_pStartTimeAttrName = "startTime";
        m_pEndTimeAttrName = "endTime";
        m_pErrorMessageAttrName = "errorMessage";
        m_pMountedOnAttrName = "mountedOn";
        m_pDestDeviceNameAttrName = "destDeviceName";
        m_pRecoveryOptionAttrName = "recoveryOption";
        m_pSourceDeviceNameAttrName = "sourceDeviceName";
        m_pOperationAttrName = "operation";
        m_pRecoveryPointAttrName = "recoveryPoint";
        m_pDeleteMapFlagAttrName = "deleteMapFlag";
        m_pIsEventBasedAttrName = "isEventBased";
        m_pRecoveryTypeAttrName = "recoveryType";
        m_pReadWriteAttrName = "readWrite";
        m_pDataLogPathAttrName = "dataLogPath";
        m_pLastScheduledVolAttrName = "lastScheduledVol";


    }
}
