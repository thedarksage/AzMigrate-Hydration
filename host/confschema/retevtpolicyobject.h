#ifndef RETEVTPOLICY__OBJECT__H_
#define RETEVTPOLICY__OBJECT__H_

namespace ConfSchema
{
    class RetEventPoliciesObject
    {
        const char *m_pName;
        const char *m_pIdAttrName;
        const char *m_pStoragePathAttrName;
        const char *m_pStorageDeviceAttrName;
        const char *m_pStoragePrunePolicyAttrName;
        const char *m_pConsistencyTagAttrName;
        const char *m_pAlertThresholdAttrName;
        const char *m_pTagCountAttrName;
        const char *m_pUserDefinedTagAttrName;
        const char *m_pCatalogPathAttrName;

    public:
        RetEventPoliciesObject();
        const char * GetName(void);
        const char * GetIdAttrName(void);
        const char * GetStoragePathAttrName(void);
        const char * GetStorageDeviceAttrName(void);
        const char * GetStoragePrunePolicyAttrName(void);
        const char * GetConsistencyTagAttrName(void);
        const char * GetAlertThresholdAttrName(void);
        const char * GetTagCountAttrName(void);
        const char * GetUserDefinedTagAttrName(void);
        const char * GetCatalogPathAttrName(void);
    };

    inline const char * RetEventPoliciesObject::GetName(void)
    {
        return m_pName;
    }

    inline const char * RetEventPoliciesObject::GetIdAttrName(void)
    {
        return m_pIdAttrName;
    }

    inline const char * RetEventPoliciesObject::GetStoragePathAttrName(void)
    {
        return m_pStoragePathAttrName;
    }

    inline const char * RetEventPoliciesObject::GetStorageDeviceAttrName(void)
    {
        return m_pStorageDeviceAttrName;
    }

    inline const char * RetEventPoliciesObject::GetStoragePrunePolicyAttrName(void)
    {
        return m_pStoragePrunePolicyAttrName;
    }

    inline const char * RetEventPoliciesObject::GetConsistencyTagAttrName(void)
    {
        return m_pConsistencyTagAttrName;
    }

    /* TODO: this repeats twice in schema */
    inline const char * RetEventPoliciesObject::GetAlertThresholdAttrName(void)
    {
        return m_pAlertThresholdAttrName;
    }

    inline const char * RetEventPoliciesObject::GetTagCountAttrName(void)
    {
        return m_pTagCountAttrName;
    }

    inline const char * RetEventPoliciesObject::GetUserDefinedTagAttrName(void)
    {
        return m_pUserDefinedTagAttrName;
    }

    inline const char * RetEventPoliciesObject::GetCatalogPathAttrName(void)
    {
        return m_pCatalogPathAttrName;
    }
}
#endif /* RETEVTPOLICY__OBJECT__H_ */
