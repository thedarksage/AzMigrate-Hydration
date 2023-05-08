#ifndef VOLUMES__OBJECT__H__
#define VOLUMES__OBJECT__H__

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class VolumesObject
    {
        const char *m_pName;
        const char *m_pGUIDAttrName;
        const char *m_pNameAttrName;
        const char *m_pFileSystemAttrName;
        const char *m_pCapacityAttrName;
        const char *m_pIsSystemVolumeAttrName;
        const char *m_pIsBootVolumeAttrName;
        const char *m_pFreeSpaceAttrName;
        const char *m_pAttachModeAttrName;
        const char *m_pVolumeLabelAttrName;
        const char *m_pMountPointAttrName;
        const char *m_pRawSizeAttrName;
        const char *m_pWriteCacheStateAttrName;
		const char *m_pDeviceInUseAttrName;
		const char *m_pVolumeType;
		const char *m_pInterfaceType;
        const char *m_pSizeOnDIsk ;
        const char *m_pDiskSpaceSavedByCompression ;
        const char *m_pDiskSpaceSavedByThinProvisioning ;
		const char *m_pIsDeletedVolumeAttrName;
        const char *m_pIsLockedAttrName ;
		const char *m_pVolumeGrpAttrName ;
    public:
		const char * GetVolumeGrpAttrName() ;
        VolumesObject();
        const char * GetName(void);
        const char * GetGUIDAttrName(void);
        const char * GetNameAttrName(void);
        const char * GetFileSystemAttrName(void);
        const char * GetCapacityAttrName(void);
        const char * GetIsSystemVolumeAttrName(void);
        const char * GetIsBootVolumeAttrName(void);
        const char * GetFreeSpaceAttrName(void);
        const char * GetAttachModeAttrName(void);
        const char * GetVolumeLabelAttrName(void);
        const char * GetMountPointAttrName(void);
        const char * GetRawSizeAttrName(void);
        const char * GetWriteCacheStateAttrName(void);
		const char * GetDeviceInUseAttrName(void);
		const char * GetVolumeTypeAttrName(void);
		const char * GetInterfaceTypeAttrName(void);
        const char * GetSizeOnDiskAttrName(void) ;
        const char * GetDiskSpaceSavedByCompressionAttrName(void) ;
        const char * GetDiskSpaceSavedByThinProvisioningAttrName(void) ;
		const char * GetIsDeletedVolumeAttrName(void) ;
        const char * GetIsLockedVolumeAttarName(void) ;

    };

    inline const char * VolumesObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * VolumesObject::GetVolumeGrpAttrName()
	{
		return m_pVolumeGrpAttrName ;
	}
    inline const char * VolumesObject::GetGUIDAttrName(void)
    {
        return m_pGUIDAttrName;
    }
    
    inline const char * VolumesObject::GetNameAttrName(void)
    {
        return m_pNameAttrName;
    }
    
    inline const char * VolumesObject::GetFileSystemAttrName(void)
    {
        return m_pFileSystemAttrName;
    }
    
    inline const char * VolumesObject::GetCapacityAttrName(void)
    {
        return m_pCapacityAttrName;
    }
    
    inline const char * VolumesObject::GetIsSystemVolumeAttrName(void)
    {
        return m_pIsSystemVolumeAttrName;
    }
    
    inline const char * VolumesObject::GetIsBootVolumeAttrName(void)
    {
        return m_pIsBootVolumeAttrName;
    }
    
    inline const char * VolumesObject::GetFreeSpaceAttrName(void)
    {
        return m_pFreeSpaceAttrName;
    }
    
    inline const char * VolumesObject::GetAttachModeAttrName(void)
    {
        return m_pAttachModeAttrName;
    }
    
    inline const char * VolumesObject::GetVolumeLabelAttrName(void)
    {
        return m_pVolumeLabelAttrName;
    }
    
    inline const char * VolumesObject::GetMountPointAttrName(void)
    {
        return m_pMountPointAttrName;
    }

    inline const char * VolumesObject::GetRawSizeAttrName(void)
    {
        return m_pRawSizeAttrName;
    }

    inline const char * VolumesObject::GetWriteCacheStateAttrName(void)
    {
        return m_pWriteCacheStateAttrName;
    }
	inline const char * VolumesObject::GetDeviceInUseAttrName(void)
	{
		return m_pDeviceInUseAttrName;
	}
	inline const char * VolumesObject::GetVolumeTypeAttrName(void)
	{
		return m_pVolumeType;
	}
	inline const char * VolumesObject::GetInterfaceTypeAttrName(void)
	{
		return m_pInterfaceType;
	}
    inline const char * VolumesObject::GetSizeOnDiskAttrName(void)
    {
        return  m_pSizeOnDIsk ;
    }
    inline const char * VolumesObject::GetDiskSpaceSavedByCompressionAttrName(void)
    {
        return m_pDiskSpaceSavedByCompression ;
    }
    inline const char * VolumesObject::GetDiskSpaceSavedByThinProvisioningAttrName(void)
    {
        return m_pDiskSpaceSavedByThinProvisioning ;
    }
	inline const char * VolumesObject::GetIsDeletedVolumeAttrName (void )
	{
		return m_pIsDeletedVolumeAttrName;
	}
    inline const char * VolumesObject::GetIsLockedVolumeAttarName(void)
    {
        return m_pIsLockedAttrName ;
    }
}

namespace ConfSchema
{
    class VolumeResizeHistoryObject
    {
         const char* m_pOldRawSize ;
         const char* m_pOldCapacity ;
         const char* m_pNewCapacity ;
         const char* m_pNewRawSize ;
		 const char* m_pIsSparseFilesCreated;
         const char* m_pTimeStamp  ;
         const char* m_pName ;
    public:
        VolumeResizeHistoryObject();
        const char * GetOldRawSizeAttrName(void);
        const char * GetCurrentRawSizeAttrName(void);
        const char * GetOldCapacityAttrName(void);
        const char * GetCurrentCapacityAttrName(void);
		const char * GetIsSparseFilesCreatedAttrName(void);
        const char * GetTimeStampAttrName(void) ;
        const char * GetName() ;
     } ;
    inline const char  * VolumeResizeHistoryObject::GetOldRawSizeAttrName(void)
    {
        return m_pOldRawSize ;
    }
    inline const char  * VolumeResizeHistoryObject::GetCurrentRawSizeAttrName(void)
    {
        return m_pNewRawSize ;
    }
    inline const char  * VolumeResizeHistoryObject::GetOldCapacityAttrName(void)
    {
        return m_pOldCapacity ;
    }
    inline const char * VolumeResizeHistoryObject::GetCurrentCapacityAttrName(void)
    {
        return m_pNewCapacity ;
    }
	inline const char * VolumeResizeHistoryObject::GetIsSparseFilesCreatedAttrName(void)
    {
        return m_pIsSparseFilesCreated ;
    }

    inline const char * VolumeResizeHistoryObject::GetTimeStampAttrName(void)
    {
        return m_pTimeStamp ;
    }
        inline const char * VolumeResizeHistoryObject::GetName(void)
    {
        return m_pName ;
    }
}
#endif /* VOLUMES__SCHEMA__H__ */
