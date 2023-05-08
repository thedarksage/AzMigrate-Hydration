#ifndef SNAPSHOTS_OBJECT_H_
#define SNAPSHOTS_OBJECT_H_

namespace ConfSchema
{
    class SnapshotsObject
    {
        const char *m_pName;
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
        SnapshotsObject();
        const char * GetName(void);
        const char * GetSourceDeviceNameAttrName(void);
        const char * GetOperationAttrName(void);
        const char * GetRecoveryPointAttrName(void);
        const char * GetDeleteMapFlagAttrName(void);
        const char * GetIsEventBasedAttrName(void);
        const char * GetRecoveryTypeAttrName(void);
        const char * GetReadWriteAttrName(void);
        const char * GetDataLogPathAttrName(void);
        const char * GetDestDeviceNameAttrName(void);
        const char * GetLastScheduledVolAttrName(void);
    };

    inline const char * SnapshotsObject::GetName(void)
    {
        return m_pName;
    }

    inline const char * SnapshotsObject::GetSourceDeviceNameAttrName(void)
    {
        return m_pSourceDeviceNameAttrName;
    }

    inline const char * SnapshotsObject::GetRecoveryPointAttrName(void)
    {
        return m_pRecoveryPointAttrName;
    }

    inline const char * SnapshotsObject::GetOperationAttrName(void)
    {
        return m_pOperationAttrName;
    }

    inline const char * SnapshotsObject::GetDeleteMapFlagAttrName(void)
    {
        return m_pDeleteMapFlagAttrName;
    }

    inline const char * SnapshotsObject::GetIsEventBasedAttrName(void)
    {
        return m_pIsEventBasedAttrName;
    }

    inline const char * SnapshotsObject::GetRecoveryTypeAttrName(void)
    {
        return m_pRecoveryTypeAttrName;
    }
    inline const char * SnapshotsObject::GetReadWriteAttrName(void)
    {
        return m_pReadWriteAttrName;
    }

    inline const char * SnapshotsObject::GetDataLogPathAttrName(void)
    {
        return m_pDataLogPathAttrName;
    }

    inline const char * SnapshotsObject::GetDestDeviceNameAttrName(void)
    {
        return m_pDestDeviceNameAttrName;
    }
     inline const char * SnapshotsObject::GetLastScheduledVolAttrName(void)
    {
        return m_pLastScheduledVolAttrName;
    }
}
#endif /* SNAPSHOTS_OBJECT_H_ */
