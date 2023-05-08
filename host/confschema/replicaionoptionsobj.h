#ifndef REPLICATION__OPTIONS__OBJECT__H__
#define REPLICATION__OPTIONS__OBJECT__H__

namespace ConfSchema
{
    class ReplicationOptionsObject
    {
        const char *m_pName;
        const char *m_pBatchResyncAttrName;
    public:
        ReplicationOptionsObject();	
        const char * GetName(void);
        const char * GetBatchResyncAttrName(void);
    };
    inline const char * ReplicationOptionsObject::GetName(void)
    {
        return m_pName;
    }

    inline const char * ReplicationOptionsObject::GetBatchResyncAttrName(void)
    {
        return m_pBatchResyncAttrName;
    }
}
#endif /*REPLICATION__OPTIONS__OBJECT__H__*/