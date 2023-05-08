#ifndef RETLOGPOLICY__OBJECT__H_
#define RETLOGPOLICY__OBJECT__H_

namespace ConfSchema
{
    class RetLogPolicyObject
    {
        const char *m_pName;
        const char *m_pIdAttrName;
        const char *m_pMetaFilePathAttrName;
        const char *m_pCurrLogSizeAttrName;
        const char *m_pCreatedDateAttrName;
        const char *m_pModifiedDateAttrName;
        const char *m_pRetStateAttrName;
        const char *m_pDiskSpaceThresholdAttrName;
        const char *m_pUniqueIdAttrName;
        const char *m_pIsSparceEnabledAttrName;

    public:
        RetLogPolicyObject();
        const char * GetName(void);
        const char * GetIdAttrName(void);
        const char * GetMetaFilePathAttrName(void);
        const char * GetCurrLogSizeAttrName(void);
        const char * GetCreatedDateAttrName(void);
        const char * GetModifiedDateAttrName(void);
        const char * GetRetStateAttrName(void);
        const char * GetDiskSpaceThresholdAttrName(void);
        const char * GetUniqueIdAttrName(void);
        const char * GetIsSparceEnabledAttrName(void);
    };

    inline const char * RetLogPolicyObject::GetName(void)
    {
        return m_pName;
    }

    inline const char * RetLogPolicyObject::GetIdAttrName(void)
    {
        return m_pIdAttrName;
    }

    inline const char * RetLogPolicyObject::GetMetaFilePathAttrName(void)
    {
        return m_pMetaFilePathAttrName;
    }

    inline const char * RetLogPolicyObject::GetCurrLogSizeAttrName(void)
    {
        return m_pCurrLogSizeAttrName;
    }

    inline const char * RetLogPolicyObject::GetCreatedDateAttrName(void)
    {
        return m_pCreatedDateAttrName;
    }

    inline const char * RetLogPolicyObject::GetModifiedDateAttrName(void)
    {
        return m_pModifiedDateAttrName;
    }

    inline const char * RetLogPolicyObject::GetRetStateAttrName(void)
    {
        return m_pRetStateAttrName;
    }

    inline const char * RetLogPolicyObject::GetDiskSpaceThresholdAttrName(void)
    {
        return m_pDiskSpaceThresholdAttrName;
    }

    inline const char * RetLogPolicyObject::GetUniqueIdAttrName(void)
    {
        return m_pUniqueIdAttrName;
    }

    inline const char * RetLogPolicyObject::GetIsSparceEnabledAttrName(void)
    {
        return m_pIsSparceEnabledAttrName;
    }
}
#endif /* RETLOGPOLICY__OBJECT__H_ */
