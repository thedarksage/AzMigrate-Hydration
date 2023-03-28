#ifndef PAIRS__OBJECT__H__
#define PAIRS__OBJECT__H__

namespace ConfSchema
{
    class PairsObject
    {
        const char *m_pName;
        const char *m_pSourceVolumeAttrName;
        const char *m_pTargetVolumeAttrName;
        const char *m_pRetentionVolumeAttrName;
        const char *m_pRetentionPathAttrName;
        const char *m_pSourceCapacityAttrName;
        const char *m_pProcessServerAttrName;
        const char *m_pMoveRetentionPathAttrName;
        const char *m_pRpoThresholdAttrName;
        const char *m_pVolumeProvisioningStatusAttrName ;
        const char *m_pSrcVolRawSize ;
        const char *m_pSrcVolCapacity ;
    public:
        PairsObject();
        const char * GetName(void);
        const char * GetSourceVolumeAttrName(void);
        const char * GetTargetVolumeAttrName(void);
        const char * GetRetentionVolumeAttrName(void);
        const char * GetRetentionPathAttrName(void);
        const char * GetProcessServerAttrName(void);
        const char * GetMoveRetentionPathAttrName(void);
        const char * GetRpoThresholdAttrName(void);
        const char * GetVolumeProvisioningStatusAttrName(void) ;
        const char * GetSrcRawSizeAttrName(void) ;
        const char * GetSrcCapacityAttrName(void) ;
    };
    inline const char * PairsObject::GetVolumeProvisioningStatusAttrName(void)
    {
        return m_pVolumeProvisioningStatusAttrName ;
    }
    inline const char * PairsObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * PairsObject::GetSourceVolumeAttrName(void)
    {
        return m_pSourceVolumeAttrName;
    }
    inline const char * PairsObject::GetTargetVolumeAttrName(void)
    {
        return m_pTargetVolumeAttrName;
    }
    inline const char * PairsObject::GetRetentionVolumeAttrName(void)
    {
        return m_pRetentionVolumeAttrName;
    }
    inline const char * PairsObject::GetRetentionPathAttrName(void)
    {
        return m_pRetentionPathAttrName;
    }
    inline const char * PairsObject::GetProcessServerAttrName(void)
    {
        return m_pProcessServerAttrName;
    }

    inline const char * PairsObject::GetMoveRetentionPathAttrName(void)
    {
        return m_pMoveRetentionPathAttrName;
    }

    inline const char * PairsObject::GetRpoThresholdAttrName(void)
    {
        return m_pRpoThresholdAttrName;
    }
    inline const char * PairsObject::GetSrcRawSizeAttrName(void)
    {
        return m_pSrcVolRawSize ;
    }
    inline const char * PairsObject::GetSrcCapacityAttrName(void)
    {
        return m_pSrcVolCapacity ;
    }

}
#endif /* PAIRS__OBJECT__H__ */
