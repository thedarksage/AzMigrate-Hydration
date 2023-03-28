#ifndef AUTO__RESYNC__OBJECT__H__
#define AUTO__RESYNC__OBJECT__H__

namespace ConfSchema
{
    class AutoResyncObject
    {
        const char *m_pName;
        const char *m_pStateAttrName;
        const char *m_pBetweenAttrName;
        const char *m_pWaitTimeAttrName;
    public:
        AutoResyncObject();
        const char * GetName(void);
        const char * GetStateAttrName(void);
        const char * GetBetweenAttrName(void);
        const char * GetWaitTimeAttrName(void);

    };

    inline const char * AutoResyncObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * AutoResyncObject::GetStateAttrName(void)
    {
        return m_pStateAttrName;
    }
    inline const char * AutoResyncObject::GetBetweenAttrName(void)
    {
        return m_pBetweenAttrName;
    }
    inline const char * AutoResyncObject::GetWaitTimeAttrName(void)
    {
        return m_pWaitTimeAttrName;
    }
}
#endif /*AUTO__RESYNC__OBJECT__H__*/