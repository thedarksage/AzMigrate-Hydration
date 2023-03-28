#ifndef BACKUP__REPOSITORY__OBJECT__H__
#define BACKUP__REPOSITORY__OBJECT__H__

namespace ConfSchema
{
    class BackupRepositoriesAttributeGroupObject
    {
        const char *m_pName;
       // const char *m_pRepositoryNameAttrName;
        const char *m_pSourceVolume1AttrName;
        const char *m_pSourceVolume2AttrName;
    public:
        BackupRepositoriesAttributeGroupObject();
        const char * GetName(void);
     //   const char * GetRepositoryNameAttrName(void);
        const char * GetSourceVolume1AttrName(void);
        const char * GetSourceVolume2AttrName(void);
    };

    inline const char * BackupRepositoriesAttributeGroupObject::GetName(void)
    {
        return m_pName;
    }
    /*inline const char * BackupRepositoriesAttributeGroupObject::GetRepositoryNameAttrName(void)
    {
        return m_pRepositoryNameAttrName;
    }*/
    inline const char * BackupRepositoriesAttributeGroupObject::GetSourceVolume1AttrName(void)
    {
        return m_pSourceVolume1AttrName;
    }
    inline const char * BackupRepositoriesAttributeGroupObject::GetSourceVolume2AttrName(void)
    {
        return m_pSourceVolume2AttrName;
    }
}
#endif /* BACKUP__REPOSITORY__OBJECT__H__ */