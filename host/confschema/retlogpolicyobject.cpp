#include "retlogpolicyobject.h"

namespace ConfSchema
{
    RetLogPolicyObject::RetLogPolicyObject()
    {
        m_pName = "RetentionLogPolicy";
        m_pIdAttrName = "Id";
        m_pMetaFilePathAttrName = "metafilepath";
        m_pCurrLogSizeAttrName = "currLogSize";
        m_pCreatedDateAttrName = "createdDate";
        m_pModifiedDateAttrName = "modifiedDate";
        m_pRetStateAttrName = "retState";
        m_pDiskSpaceThresholdAttrName = "diskSpaceThreshold";
        m_pUniqueIdAttrName = "uniqueId";
        m_pIsSparceEnabledAttrName = "isSparseEnabled";
    }
}
