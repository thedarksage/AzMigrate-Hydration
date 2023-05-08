#include "pairsobj.h"
namespace ConfSchema
{
    PairsObject::PairsObject()
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
        m_pName = "Pairs";
        m_pSourceVolumeAttrName = "sourceVolume";
        m_pTargetVolumeAttrName = "targetVolume";
        m_pRetentionVolumeAttrName = "retentionVolume";
        m_pRetentionPathAttrName = "retentionPath";
        m_pProcessServerAttrName = "processServer";
        m_pMoveRetentionPathAttrName = "moveRetentionPath";
        m_pRpoThresholdAttrName = "rpoThreshold";
        m_pVolumeProvisioningStatusAttrName = "VolumeProvisioningStatus" ;
        m_pSrcVolRawSize = "SrcVolumeRawSize" ;
        m_pSrcVolCapacity = "SrcVolumeCapacity" ;
    }
}