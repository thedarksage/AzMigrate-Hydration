#ifndef ALERT_OBJECT_H_
#define ALERT_OBJECT_H_

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class AlertObjcet
    {
        const char *m_pName;
        const char *m_pAlertTimestampAttrName;
        const char *m_pLevelAttrName;
        const char *m_pTypeAttrName;
        const char *m_pMessageAttrName;
        const char *m_pModuleAttrName;
        const char *m_pFixStepsAttrName;
        const char *m_pCountAttrName;
    public:
        AlertObjcet();
        const char * GetName(void);
        const char * GetAlertTimestampAttrName(void);
        const char * GetLevelAttrName(void);
        const char * GetTypeAttrName(void);
        const char * GetMessageAttrName(void);
        const char * GetModuleAttrName(void);
        const char * GetFixStepsAttrNameAttrName(void);
        const char * GetCountAttrName(void);
    };

    inline const char * AlertObjcet::GetName(void)
    {
        return m_pName;
    }
         
    inline const char * AlertObjcet::GetAlertTimestampAttrName(void)
    {
        return m_pAlertTimestampAttrName;
    }

    inline const char * AlertObjcet::GetLevelAttrName(void)
    {
        return m_pLevelAttrName;
    }

    inline const char * AlertObjcet::GetTypeAttrName(void)
    {
        return m_pTypeAttrName;
    }

    inline const char * AlertObjcet::GetMessageAttrName(void)
    {
        return m_pMessageAttrName;
    }
    inline const char * AlertObjcet::GetModuleAttrName(void)
    {
        return m_pModuleAttrName;
    }
    inline const char * AlertObjcet::GetFixStepsAttrNameAttrName(void)
    {
        return m_pFixStepsAttrName;
    }
    inline const char * AlertObjcet::GetCountAttrName(void)
    {
        return m_pCountAttrName;
    }
}

#endif /* ALERT_OBJECT_H_ */
