#ifndef POLICY__INSTANCE__H__
#define POLICY__INSTANCE__H__

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class AppPolicyInstanceObject
    {
        const char *m_pName;
        const char *m_pPolicyIdAttrName;
        const char *m_pInstanceIdAttrName;
        const char *m_pStatusAttrName;
        const char *m_pExecutionLogAttrName;
        const char *m_pUniqueIdAttrName;
        const char *m_pDependentInstanceIdAttrName;
        const char *m_pScenarioIdAttrName;

    public:
        AppPolicyInstanceObject();
        const char * GetName(void);
        const char * GetPolicyIdAttrName(void);
        const char * GetInstanceIdAttrName(void);
        const char * GetStatusAttrName(void);
        const char * GetExecutionLogAttrName(void);
        const char * GetUniqueIdAttrName(void);
        const char * GetDependentInstanceIdAttrName(void);
        const char * GetScenarioIdAttrName(void);
    };

    inline const char * AppPolicyInstanceObject::GetScenarioIdAttrName(void)
    {
        return m_pScenarioIdAttrName;
    }
    inline const char * AppPolicyInstanceObject::GetName(void)
    {
        return m_pName;
    }
         
    inline const char * AppPolicyInstanceObject::GetPolicyIdAttrName(void)
    {
        return m_pPolicyIdAttrName;
    }

    inline const char * AppPolicyInstanceObject::GetInstanceIdAttrName(void)
    {
        return m_pInstanceIdAttrName;
    }

    inline const char * AppPolicyInstanceObject::GetStatusAttrName(void)
    {
        return m_pStatusAttrName;
    }

    inline const char * AppPolicyInstanceObject::GetExecutionLogAttrName(void)
    {
        return m_pExecutionLogAttrName;
    }
    inline const char * AppPolicyInstanceObject::GetUniqueIdAttrName(void)
    {
        return m_pUniqueIdAttrName;
    }
    inline const char * AppPolicyInstanceObject::GetDependentInstanceIdAttrName(void)
    {
        return m_pDependentInstanceIdAttrName;
    }
}

#endif /* POLICY__INSTANCE__H__ */
