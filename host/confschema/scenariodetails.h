#ifndef SCENARIO__DETAILS__OBJECT__H_
#define SCENARIO__DETAILS__OBJECT__H_

namespace ConfSchema
{
    class ScenraionDetailsObject
    {
        const char *m_pName;
        const char *m_pRunReadinessCheckAttrName;
        const char *m_pSoultionTypeAttrName;
        const char *m_pRecoveryFeatureAttrName;
        const char *m_pActionOnNewVolumeDiscoveryAttrName;
        const char *m_pTargetHostId ;
        const char *m_pSrcHostName ;
        const char *m_pTgtHostName ;
        const char *m_pIPAddress ;
        const char *m_pSrcHostId ;
    public:
        ScenraionDetailsObject();
        const char * GetName(void);
        const char * GetRunReadinessCheckAttrName(void);
        const char * GetSoulutionTypeAttrName(void);
        const char * GetRecoveryFeatrureAttrName(void);
        const char * GetActionOnNewVolumeDiscoveryAttrName(void);
        const char * GetTargetHostId(void) ;
        const char * GetSrcHostName(void) ;
        const char * GetTgtHostName(void) ;
        const char * GetSrcHostId(void) ;
    };
    inline const char * ScenraionDetailsObject::GetSrcHostId(void)
    {
        return m_pSrcHostId ;
    }
    inline const char * ScenraionDetailsObject::GetSrcHostName(void)
    {
        return m_pSrcHostName ;
    }
    inline const char * ScenraionDetailsObject::GetTgtHostName(void)
    {
        return m_pTgtHostName ;
    }
    inline const char * ScenraionDetailsObject::GetTargetHostId(void)
    {
        return m_pTargetHostId ;
    }
    inline const char * ScenraionDetailsObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * ScenraionDetailsObject::GetRunReadinessCheckAttrName(void)
    {
        return m_pRunReadinessCheckAttrName;
    }
    inline const char * ScenraionDetailsObject::GetSoulutionTypeAttrName(void)
    {
        return m_pSoultionTypeAttrName;
    }
    inline const char * ScenraionDetailsObject::GetRecoveryFeatrureAttrName(void)
    {
        return m_pRecoveryFeatureAttrName;
    }
    inline const char * ScenraionDetailsObject::GetActionOnNewVolumeDiscoveryAttrName(void)
    {
        return m_pActionOnNewVolumeDiscoveryAttrName;
    }
}
#endif /*SCENARIO__DETAILS__OBJECT__H_*/