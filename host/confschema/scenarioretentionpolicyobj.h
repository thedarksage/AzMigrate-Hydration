#ifndef RETEN__POLICY__OBJECT__H__
#define RETEN__POLICY__OBJECT__H__

namespace ConfSchema
{
    class ScenarioRetentionPolicyObject
    {
        const char *m_pName;
        const char *m_pRetentionTypeAttrName;
        const char *m_pDurationAttrName;
        const char *m_pSizeAttrName;
    public:
        ScenarioRetentionPolicyObject();
        const char * GetName(void);
        const char * GetRetentionTypeAttrName(void);
        const char * GetDurationAttrName(void);
        const char * GetSizeAttrName(void);

    };
    inline const char * ScenarioRetentionPolicyObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * ScenarioRetentionPolicyObject::GetRetentionTypeAttrName(void)
    {
        return m_pRetentionTypeAttrName;
    }

    inline const char * ScenarioRetentionPolicyObject::GetDurationAttrName(void)
    {
        return m_pDurationAttrName;
    }

    inline const char * ScenarioRetentionPolicyObject::GetSizeAttrName(void)
    {
        return m_pSizeAttrName;
    }
}
#endif /*RETEN__POLICY__OBJECT__H__ */