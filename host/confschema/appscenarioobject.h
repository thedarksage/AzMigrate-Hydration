#ifndef APP__SCENARIO__OBJECT__H__
#define APP__SCENARIO__OBJECT__H__

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class AppScenarioObject
    {
        const char *m_pName;
        const char *m_pScenarioIdAttrName;
        const char *m_pScenarioTypeAttrName;
        const char *m_pScenarioNameAttrName;
        const char *m_pScenarioDescriptionAttrName;
        const char *m_pScenarionVersionAttrName;
        const char *m_pExecutionStatusAttrName;
        const char *m_pApplicationTypeAttrName;
        const char *m_pReferenceIdAttrName;
        const char *m_pIsPrimaryAttrName;
		const char *m_pIsSystemPaused;
		const char *m_pSystemPauseReason;

    public:
        AppScenarioObject();
        const char * GetName(void);
        const char * GetScenarioIdAttrName(void);
        const char * GetScenarioTypeAttrName(void);
        const char * GetScenarioNameAttrName(void);
        const char * GetScenarioDescriptionAttrName(void);
        const char * GetScenarionVersionAttrName(void);
        const char * GetExecutionStatusAttrName(void);
        const char * GetApplicationTypeAttrName(void);
        const char * GetReferenceIdAttrName(void);
        const char * GetIsPrimaryAttrName(void);
		const char * GetIsSystemPausedAttrName(void);
		const char * GetSystemPauseReasonAttrName(void) ;
    };

    inline const char * AppScenarioObject::GetName(void)
    {
        return m_pName;
    }
         
    inline const char * AppScenarioObject::GetScenarioIdAttrName(void)
    {
        return m_pScenarioIdAttrName;
    }

    inline const char * AppScenarioObject::GetScenarioTypeAttrName(void)
    {
        return m_pScenarioTypeAttrName;
    }

    inline const char * AppScenarioObject::GetScenarioNameAttrName(void)
    {
        return m_pScenarioNameAttrName;
    }

    inline const char * AppScenarioObject::GetScenarioDescriptionAttrName(void)
    {
        return m_pScenarioDescriptionAttrName;
    }
    inline const char * AppScenarioObject::GetScenarionVersionAttrName(void)
    {
        return m_pScenarionVersionAttrName;
    }
    inline const char * AppScenarioObject::GetExecutionStatusAttrName(void)
    {
        return m_pExecutionStatusAttrName;
    }
    inline const char * AppScenarioObject::GetApplicationTypeAttrName(void)
    {
        return m_pApplicationTypeAttrName;
    }
    inline const char * AppScenarioObject::GetReferenceIdAttrName(void)
    {
        return m_pReferenceIdAttrName;
    }
    inline const char * AppScenarioObject::GetIsPrimaryAttrName(void)
    {
        return m_pIsPrimaryAttrName;
    }
	inline const char * AppScenarioObject::GetIsSystemPausedAttrName(void)
	{
		return m_pIsSystemPaused;
	}
	inline const char * AppScenarioObject::GetSystemPauseReasonAttrName(void)
	{
		return m_pSystemPauseReason;
	}
}

#endif /* APP__SCENARIO__OBJECT__H__ */
