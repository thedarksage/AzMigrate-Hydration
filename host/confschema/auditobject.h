#ifndef AUDIT_OBJECT_H_
#define AUDIT_OBJECT_H_

namespace ConfSchema
{
    class AuditObject
    {
        const char *m_pName;
		const char *m_pMessageAttrName;
        const char *m_pAuditTimestampAttrName;
    public:
        AuditObject();
        const char * GetName(void);
		const char * GetMessageAttrName (void);
        const char * GetAuditTimestampAttrName(void);
    };

    inline const char * AuditObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * AuditObject::GetMessageAttrName(void)
    {
        return m_pMessageAttrName;
    }         
    inline const char * AuditObject::GetAuditTimestampAttrName(void)
    {
        return m_pAuditTimestampAttrName;
    }
}

#endif /*AUDIT_OBJECT_H_*/ 