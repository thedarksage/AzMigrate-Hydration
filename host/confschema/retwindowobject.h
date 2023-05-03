#ifndef RETWINDOW__OBJECT__H_
#define RETWINDOW__OBJECT__H_

namespace ConfSchema
{
    class RetWindowObject
    {
        const char *m_pName;
        const char *m_pIdAttrName;
        const char *m_pRetWindowIdAttrName;
        const char *m_pStartTimeAttrName;
        const char *m_pEndTimeAttrName;
        const char *m_pTimeGranularityAttrName;
        const char *m_pTimeIntervalUnitAttrName;
        const char *m_pGranularityUnitAttrName;

    public:
        RetWindowObject();
        const char * GetName(void);
        const char * GetIdAttrName(void);
        const char * GetRetWindowIdAttrName(void);
        const char * GetStartTimeAttrName(void);
        const char * GetEndTimeAttrName(void);
        const char * GetTimeGranularityAttrName(void);
        const char * GetTimeIntervalUnitAttrName(void);
        const char * GetGranularityUnitAttrName(void);
    };

    inline const char * RetWindowObject::GetName(void)
    {
        return m_pName;
    }

    inline const char * RetWindowObject::GetIdAttrName(void)
    {
        return m_pIdAttrName;
    }

    inline const char * RetWindowObject::GetRetWindowIdAttrName(void)
    {
        return m_pRetWindowIdAttrName;
    }

    inline const char * RetWindowObject::GetStartTimeAttrName(void)
    {
        return m_pStartTimeAttrName;
    }

    inline const char * RetWindowObject::GetEndTimeAttrName(void)
    {
        return m_pEndTimeAttrName;
    }

    inline const char * RetWindowObject::GetTimeGranularityAttrName(void)
    {
        return m_pTimeGranularityAttrName;
    }

    inline const char * RetWindowObject::GetTimeIntervalUnitAttrName(void)
    {
        return m_pTimeIntervalUnitAttrName;
    }

    inline const char * RetWindowObject::GetGranularityUnitAttrName(void)
    {
        return m_pGranularityUnitAttrName;
    }
}
#endif /* RETWINDOW__OBJECT__H_ */
