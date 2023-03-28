#include "retevtpolicyobject.h"

namespace ConfSchema
{
    RetEventPoliciesObject::RetEventPoliciesObject()
    {
        m_pName = "RetentionEventPolicies";
        m_pIdAttrName = "Id";
        m_pStoragePathAttrName = "storagePath";
        m_pStorageDeviceAttrName = "storageDeviceName";
        m_pStoragePrunePolicyAttrName = "storagePruningPolicy";
        m_pConsistencyTagAttrName = "consistencyTag";
        m_pAlertThresholdAttrName = "alertThreshold";
        m_pTagCountAttrName = "tagCount";
        m_pUserDefinedTagAttrName = "userDefinedTag";
        m_pCatalogPathAttrName = "catalogPath";
    }
}
