#ifndef APP__POLICY__OBJECT__H__
#define APP__POLICY__OBJECT__H__

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class ApplicationPolicyObject
    {
        const char *m_pName;
        const char *m_pIdAttrName;
        const char *m_pTypeAttrName;
        const char *m_pContextAttrName;
        const char *m_pScheduleTypeAttrName;
        const char *m_pIntervalAttrName;
        const char *m_pExcludeFromAttrName;
        const char *m_pExcludeToAttrName;
        const char *m_pHostIdAttrName;
        const char *m_pPolicyDataAttrName;
        const char *m_pScenarioIdAttrName;
        const char *m_pPolicyTimestampAttrName;
        const char *m_pConsistencyCommandAttrName;
		const char *m_pDoNotSkipAttrName;
        const char *m_pSuccessRunsAttrName ;
        const char *m_pFailedRunsAttrName ;
        const char *m_pTotalRunsAttrName ;
        const char *m_pSkippedRunsAttrName ;
		const char *m_pGetVolpacksforVolumesAttrName ;
    public:
        ApplicationPolicyObject();
        const char * GetName(void);
        const char * GetIdAttrName(void);
        const char * GetTypeAttrName(void);
        const char * GetContextAttrName(void);
        const char * GetScheduleTypeAttrName(void);
        const char * GetIntervalAttrName(void);
        const char * GetExcludeFromAttrName(void);
        const char * GetExcludeToAttrName(void);
        const char * GetHostIdAttrName(void);
        const char * GetPolicyDataAttrName(void);
		const char * GetVolpacksForVolumes(void) ;
        const char * GetScenarioIdAttrName(void);
        const char * GetPolicyTimestampAttrName(void);
        const char * GetconsistencyCommandAttrName(void);
		const char * GetDoNotSkipAttrName(void);
        const char * GetSuccessRunsCountAttrName(void) ;
        const char * GetFailedRunsCountAttrName(void) ;
        const char * GetTotalRunsCountAttrName(void) ;
        const char * GetSkippedRunsCountAttrName(void) ;
    };
	inline const char * ApplicationPolicyObject::GetVolpacksForVolumes(void)
	{
		return m_pGetVolpacksforVolumesAttrName ;
	}
    inline const char * ApplicationPolicyObject::GetSkippedRunsCountAttrName(void)
    {
        return m_pSkippedRunsAttrName ;
    }
    inline const char * ApplicationPolicyObject::GetSuccessRunsCountAttrName(void)
    {
        return m_pSuccessRunsAttrName ;

    }
    inline const char * ApplicationPolicyObject::GetFailedRunsCountAttrName(void)
    {
        return m_pFailedRunsAttrName ;
    }
    inline const char * ApplicationPolicyObject::GetTotalRunsCountAttrName(void)
    {
        return m_pTotalRunsAttrName ;
    }

    inline const char * ApplicationPolicyObject::GetName(void)
    {
        return m_pName;
    }
         
    inline const char * ApplicationPolicyObject::GetIdAttrName(void)
    {
        return m_pIdAttrName;
    }

    inline const char * ApplicationPolicyObject::GetTypeAttrName(void)
    {
        return m_pTypeAttrName;
    }

    inline const char * ApplicationPolicyObject::GetContextAttrName(void)
    {
        return m_pContextAttrName;
    }

    inline const char * ApplicationPolicyObject::GetScheduleTypeAttrName(void)
    {
        return m_pScheduleTypeAttrName;
    }
    inline const char * ApplicationPolicyObject::GetIntervalAttrName(void)
    {
        return m_pIntervalAttrName;
    }
    inline const char * ApplicationPolicyObject::GetExcludeFromAttrName(void)
    {
        return m_pExcludeFromAttrName;
    }
    inline const char * ApplicationPolicyObject::GetExcludeToAttrName(void)
    {
        return m_pExcludeToAttrName;
    }
    inline const char * ApplicationPolicyObject::GetHostIdAttrName(void)
    {
        return m_pHostIdAttrName;
    }
    inline const char * ApplicationPolicyObject::GetPolicyDataAttrName(void)
    {
        return m_pPolicyDataAttrName;
    }
    inline const char * ApplicationPolicyObject::GetScenarioIdAttrName(void)
    {
        return m_pScenarioIdAttrName;
    }
    inline const char * ApplicationPolicyObject::GetPolicyTimestampAttrName(void)
    {
        return m_pPolicyTimestampAttrName;
    }
    inline const char * ApplicationPolicyObject::GetconsistencyCommandAttrName(void)
    {
        return m_pConsistencyCommandAttrName;
    }
	 inline const char * ApplicationPolicyObject::GetDoNotSkipAttrName(void)
    {
        return m_pDoNotSkipAttrName;
    }
	
}

#endif /* APP__POLICY__OBJECT__H__ */
