#include "snapshotsobject.h"

namespace ConfSchema
{
    SnapshotsObject::SnapshotsObject()
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
        m_pName = "SnapShots";
        m_pSourceDeviceNameAttrName = "sourceDeviceName";
        m_pOperationAttrName = "operation";
        m_pRecoveryPointAttrName = "recoveryPoint";
        m_pDeleteMapFlagAttrName = "deleteMapFlag";
        m_pIsEventBasedAttrName = "isEventBased";
        m_pRecoveryTypeAttrName = "recoveryType";
        m_pReadWriteAttrName = "readWrite";
        m_pDataLogPathAttrName = "dataLogPath";
        m_pDestDeviceNameAttrName = "destDeviceName";
        m_pLastScheduledVolAttrName = "lastScheduledVol";
    }
}
