#include "volumesobject.h"

namespace ConfSchema
{
    VolumesObject::VolumesObject()
    {
        /* TODO: all these below are hard coded (taken from
         *       schema document)                      
         *       These have to come from some external
         *       source(conf file ?) ; Also curretly we use string constants 
         *       so that any instance of volumesobject gives 
         *       same addresses; 
         *       Not using std::string members and static volumesobject 
         *       for now since calling constructors of statics across 
         *       libraries is causing trouble (no constructors getting 
         *       called) */
        m_pName = "Volume";
        m_pGUIDAttrName = "GUID";
        m_pNameAttrName = "Name";
        m_pFileSystemAttrName = "FileSystemType";
        m_pCapacityAttrName = "Capacity";
        m_pIsSystemVolumeAttrName = "isSystemVolume";
        m_pIsBootVolumeAttrName = "isBootVolume";
        m_pFreeSpaceAttrName = "FreeSpace";
        m_pAttachModeAttrName = "AttachMode";
        m_pVolumeLabelAttrName = "VolumeLabel";
        m_pMountPointAttrName = "MountPoint";
        m_pRawSizeAttrName = "rawSize";
        m_pWriteCacheStateAttrName = "writeCacheState";
		m_pDeviceInUseAttrName = "deviceInUse";
		m_pVolumeType = "VolumeType";
		m_pInterfaceType = "InterFaceType";
        m_pSizeOnDIsk = "SizeOnDisk" ;
        m_pDiskSpaceSavedByCompression = "DiskSpaceSavedByCompression" ;
        m_pDiskSpaceSavedByThinProvisioning = "DiskSpaceSavedByThinProvisioning" ;
		m_pIsDeletedVolumeAttrName = "isDeletedVolume";
        m_pIsLockedAttrName = "isLocked" ;
		m_pVolumeGrpAttrName = "VolumeGroup" ;
    }
     VolumeResizeHistoryObject::VolumeResizeHistoryObject()
     {
         m_pOldRawSize = "OldRawSize" ;
         m_pOldCapacity = "OldCapacity" ;
         m_pNewCapacity = "NewCapacity" ;
         m_pNewRawSize = "NewRawSize" ;
		 m_pIsSparseFilesCreated = "IsSparseFilesCreated";
         m_pTimeStamp  = "TimeStamp" ;
         m_pName = "VolumeResizeHistory" ;
     }
}
