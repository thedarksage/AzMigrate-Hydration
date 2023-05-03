#ifndef SNAPSHOT_INSTANCES_OBJECT_H_
#define SNAPSHOT_INSTANCES_OBJECT_H_

namespace ConfSchema
{
    class SnapshotInstancesObject
    {
        const char *m_pName;
        const char *m_pSnapshotIdAttrName;
        const char *m_pExecutionStateAttrName;
        const char *m_pProgressAttrName;
        const char *m_pLastUpdateTimeAttrName;
        const char *m_pStartTimeAttrName;
        const char *m_pEndTimeAttrName;
        const char *m_pErrorMessageAttrName;
        const char *m_pMountedOnAttrName;
        const char *m_pRecoveryOptionAttrName;
        const char *m_pSourceDeviceNameAttrName;
        const char *m_pOperationAttrName;
        const char *m_pRecoveryPointAttrName;
        const char *m_pDeleteMapFlagAttrName;
        const char *m_pIsEventBasedAttrName;
        const char *m_pRecoveryTypeAttrName;
        const char *m_pReadWriteAttrName;
        const char *m_pDataLogPathAttrName;
        const char *m_pDestDeviceNameAttrName;
        const char *m_pLastScheduledVolAttrName;
      public:
        SnapshotInstancesObject();
        const char * GetName(void);
        const char * GetSnapshotIdAttrName(void);
        const char * GetExecutionStateAttrName(void);
        const char * GetProgressAttrName(void);
        const char * GetLastUpdateTimeAttrName(void);
        const char * GetStartTimeAttrName(void);
        const char * GetEndTimeAttrName(void);
        const char * GetErrorMessageAttrName(void);
        const char * GetMountedOnAttrName(void);
        const char * GetDestDeviceNameAttrName(void);
        const char * GetRecoveryOptionAttrName(void);
        const char * GetSourceDeviceNameAttrName(void);
        const char * GetOperationAttrName(void);
        const char * GetRecoveryPointAttrName(void);
        const char * GetDeleteMapFlagAttrName(void);
        const char * GetIsEventBasedAttrName(void);
        const char * GetRecoveryTypeAttrName(void);
        const char * GetReadWriteAttrName(void);
        const char * GetDataLogPathAttrName(void);
        const char * GetLastScheduledVolAttrName(void);
    };

    inline const char * SnapshotInstancesObject::GetName(void)
    {
        return m_pName;
    }

    inline const char * SnapshotInstancesObject::GetSnapshotIdAttrName(void)
    {
        return m_pSnapshotIdAttrName;
    }

    inline const char * SnapshotInstancesObject::GetExecutionStateAttrName(void)
    {
        return m_pExecutionStateAttrName;
    }

    inline const char * SnapshotInstancesObject::GetProgressAttrName(void)
    {
        return m_pProgressAttrName;
    }

    inline const char * SnapshotInstancesObject::GetLastUpdateTimeAttrName(void)
    {
        return m_pLastUpdateTimeAttrName;
    }

    inline const char * SnapshotInstancesObject::GetStartTimeAttrName(void)
    {
        return m_pStartTimeAttrName;
    }

    inline const char * SnapshotInstancesObject::GetEndTimeAttrName(void)
    {
        return m_pEndTimeAttrName;
    }
    inline const char * SnapshotInstancesObject::GetErrorMessageAttrName(void)
    {
        return m_pErrorMessageAttrName;
    }

    inline const char * SnapshotInstancesObject::GetMountedOnAttrName(void)
    {
        return m_pMountedOnAttrName;
    }

    inline const char * SnapshotInstancesObject::GetDestDeviceNameAttrName(void)
    {
        return m_pDestDeviceNameAttrName;
    }
     inline const char * SnapshotInstancesObject::GetRecoveryOptionAttrName(void)
    {
        return m_pRecoveryOptionAttrName;
    }
    inline const char * SnapshotInstancesObject::GetSourceDeviceNameAttrName(void)
    {
        return m_pSourceDeviceNameAttrName;
    }

    inline const char * SnapshotInstancesObject::GetRecoveryPointAttrName(void)
    {
        return m_pRecoveryPointAttrName;
    }

    inline const char * SnapshotInstancesObject::GetOperationAttrName(void)
    {
        return m_pOperationAttrName;
    }

    inline const char * SnapshotInstancesObject::GetDeleteMapFlagAttrName(void)
    {
        return m_pDeleteMapFlagAttrName;
    }

    inline const char * SnapshotInstancesObject::GetIsEventBasedAttrName(void)
    {
        return m_pIsEventBasedAttrName;
    }

    inline const char * SnapshotInstancesObject::GetRecoveryTypeAttrName(void)
    {
        return m_pRecoveryTypeAttrName;
    }
    inline const char * SnapshotInstancesObject::GetReadWriteAttrName(void)
    {
        return m_pReadWriteAttrName;
    }
    inline const char * SnapshotInstancesObject::GetDataLogPathAttrName(void)
    {
        return m_pDataLogPathAttrName;
    }
    inline const char * SnapshotInstancesObject::GetLastScheduledVolAttrName(void)
    {
        return m_pLastScheduledVolAttrName;
    }
}
#endif /* SNAPSHOT_INSTANCES_OBJECT_H_ */
