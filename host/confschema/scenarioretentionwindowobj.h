#ifndef RETEN__WINDOW__OBJECT__H__
#define RETEN__WINDOW__OBJECT__H__

namespace ConfSchema
{
    class ScenarioRetentionWindowObject
    {
        const char *m_pName;
        const char *m_pRetentionWindowIdAttrName;
        const char *m_pDurationAttrName;
        const char *m_pGranualarityAttrName;
        const char *m_pCountAttrName;
    public:
        ScenarioRetentionWindowObject();
        const char * GetName(void);
        const char * GetRetentionWindowIdAttrName(void);
        const char * GetdurationAttrName(void);
        const char * GetGranualarityAttrName(void);
        const char * GetCountAttrName(void);
    };
    inline const char * ScenarioRetentionWindowObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * ScenarioRetentionWindowObject::GetRetentionWindowIdAttrName(void)
    {
        return m_pRetentionWindowIdAttrName;
    }
    inline const char * ScenarioRetentionWindowObject::GetdurationAttrName(void)
    {
        return m_pDurationAttrName;
    }
    inline const char * ScenarioRetentionWindowObject::GetGranualarityAttrName(void)
    {
        return m_pGranualarityAttrName;
    }
    inline const char * ScenarioRetentionWindowObject::GetCountAttrName(void)
    {
        return m_pCountAttrName;
    }

}
#endif /*RETEN__WINDOW__OBJECT__H__*/
