#include "backuprepositoryobj.h"

namespace ConfSchema
{
    BackupRepositoriesAttributeGroupObject::BackupRepositoriesAttributeGroupObject()
    {
        m_pName = "RepositoryName";
        m_pSourceVolume1AttrName = "SourceVolume1";
        m_pSourceVolume2AttrName = "SourceVolume2";
    }
}