#ifndef WIN32
#include "dllinux.h"
#include "volumegroupsettings.h"
#include "unix/volumeinfocollector.h"

//*************************************************************************************************
// This function exists in both windows and linux (and implementation is platform specific).
// This function collects disks info from the machine 
// Linux Platform Flow
// 1. First fetch vector of disks, multimap of disks and its partitions(along with volume info
// 2. For each disk from disk vector, fill all the required info.
// Output   - DisksInfoMap
// Returns DLM_ERR_SUCCESS on success
//*************************************************************************************************
DLM_ERROR_CODE DisksInfoCollector::GetDiskInfoMapFromSystem(DisksInfoMap_t & DisksInfoMapFromSys,
														    DlmPartitionInfoSubMap_t & PartitionsInfoMapFromSys,
                                                            std::list<std::string>& erraticVolumeGuids,
															bool& bPartitionExist,
															const std::string & BinaryFile, 
															const std::string & dlmFileFlag)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;
    do
    {
        std::vector<std::string> DiskNames;
        VolumesInfoMultiMap_t mmapDiskToVolInfo;
        RetVal = (DLM_ERROR_CODE)GetDisksAndVolumesInfo(DiskNames, mmapDiskToVolInfo);
        if(0 != RetVal)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to fetch the disks and partitions info for this machine.\n");
            break;
        }

        std::vector<std::string>::iterator iter = DiskNames.begin();
        for(; iter != DiskNames.end(); iter++)
        {
            DISK_INFO d; 

            //Update disk name
            SetDiskName(d, (*iter).c_str());

            //Update SCSI ID
            SCSIID ScsiId;
            if(false == GetScsiId((*iter).c_str(), ScsiId))
                SetDiskFlag(d, FLAG_NO_SCSIID);
            else
                SetDiskScsiId(d, ScsiId);

            //Update VOLUME_INFO vector and Volume Count
            SetDiskVolInfo(d, (*iter).c_str(), mmapDiskToVolInfo);

            //Update Disk Size
            SV_LONGLONG ds = 0;
            if(false == GetDiskSize((*iter).c_str(), ds))
                SetDiskFlag(d, FLAG_NO_DISKSIZE);
            else                
                SetDiskSize(d, ds);

            //Update Disk Bytes Per Sector
            SV_ULONG bytesPerSector = 0;
            if(false == GetDiskBytesPerSector((*iter).c_str(), bytesPerSector))
                SetDiskFlag(d, FLAG_NO_DISK_BYTESPERSECTOR);
            else
                SetDiskBytesPerSector(d, (SV_ULONGLONG)bytesPerSector);

			std::string strDiskType = "";
            //Update remaining disk info (mbr,gpt,type,formattype,ebr info)
            ProcessDiskAndUpdateDiskInfo(d, (*iter).c_str(), strDiskType);
            
            DisksInfoMapFromSys[*iter] = d;            
        }
    }while(0);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Fetches the disk size for given disk name
// BLKGETSIZE64 returns size in bytes
//*************************************************************************************************
bool DisksInfoCollector::GetDiskSize(const char * DiskName, SV_LONGLONG & DiskSize)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int fd;
    fd = open(DiskName, O_RDONLY);
    if (fd < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file descriptor for disk(%s). Error code: %d\n", DiskName, errno);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    if (ioctl(fd, BLKGETSIZE64, &DiskSize) < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL(BLKGETSIZE64) call failed for disk(%s). Error code: %d\n", DiskName, errno);
        close(fd);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    close(fd);
    DebugPrintf(SV_LOG_DEBUG,"device(%s) - Size = %lld\n", DiskName, DiskSize);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//*************************************************************************************************
// Fetches the partition info of given partition
// Linux(Parition) is same as Windows(Volume). 
//*************************************************************************************************
bool DisksInfoCollector::GetVolumeInfo(char * VolName, VOLUME_INFO & VolInfo) 
{    
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    SV_LONGLONG DiskSize = 0;
    if(false == GetDiskSize(VolName, DiskSize))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Disk Size for device(%s)\n",VolName);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    
    SV_LONGLONG StartingSector = 0;
    if(false == GetVolumeStartingSector(VolName, StartingSector))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get the Starting Sector for device(%s)\n",VolName);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    
    SV_ULONG BytesPerSector = DISK_BYTES_PER_SECTOR;
    if(false == GetDiskBytesPerSector(VolName, BytesPerSector))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Bytes Per Sector for device(%s)\n",VolName);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    inm_strncpy_s(VolInfo.VolumeName, ARRAYSIZE(VolInfo.VolumeName), VolName, MAX_PATH_COMMON);
    VolInfo.VolumeName[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1
    VolInfo.VolumeLength = DiskSize;
    VolInfo.StartingOffset = StartingSector * BytesPerSector;    
    VolInfo.EndingOffset = VolInfo.StartingOffset + VolInfo.VolumeLength;

    DebugPrintf(SV_LOG_DEBUG,"Volume Name     = %s\n", VolInfo.VolumeName);
    DebugPrintf(SV_LOG_DEBUG,"Volume Length   = %lld\n", VolInfo.VolumeLength); 
    DebugPrintf(SV_LOG_DEBUG,"Starting Offset = %lld\n", VolInfo.StartingOffset);
    DebugPrintf(SV_LOG_DEBUG,"Ending Offset   = %lld\n", VolInfo.EndingOffset);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;    
}

//*************************************************************************************************
// Fetches the starting sector of given volume(partition in linux)
//*************************************************************************************************
bool DisksInfoCollector::GetVolumeStartingSector(char * Name, SV_LONGLONG & StartingSector)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int fd;
    fd = open(Name, O_RDONLY);
    if (fd < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file descriptor for device(%s). Error code: %d\n", Name, errno);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    struct hd_geometry geom;
    if (ioctl(fd, HDIO_GETGEO, &geom) < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL(HDIO_GETGEO) call failed for device(%s). Error code: %d\n", Name, errno);
        close(fd);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    close(fd);
    StartingSector = geom.start;
    DebugPrintf(SV_LOG_DEBUG,"device(%s) - Starting Sector = %lld\n", Name, StartingSector);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}
	
//*************************************************************************************************
// Fetches the Bytes Per Sector in given disk
//*************************************************************************************************
bool DisksInfoCollector::GetDiskBytesPerSector(const char * DiskName, SV_ULONG & BytesPerSector)
{    
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    //for now, we can return 512 bytes always. since 4k disks are yet to come in the market.
    //Anyways keep code ready.
    BytesPerSector = DISK_BYTES_PER_SECTOR;

    int fd;
    fd = open(DiskName, O_RDONLY);
    if (fd < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file descriptor for device(%s). Error code: %d\n", DiskName, errno);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    if (ioctl(fd, BLKSSZGET, &BytesPerSector) < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL(BLKSSZGET) call failed for device(%s). Error code: %d\n", DiskName, errno);
        close(fd);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    close(fd);
    DebugPrintf(SV_LOG_DEBUG,"device(%s) - BytesPerSector = %lu\n", DiskName, BytesPerSector);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// Fetches the Parent device of given partition
// if partition is sda1 , returns sda
//*************************************************************************************************
char * DisksInfoCollector::GetParent(char * Partition)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    static char Parent[MAX_PATH_COMMON + 1];
    size_t len = strlen(Partition);
    size_t i;
    for(i = len-1; i>=0; i--)
    {
        if(!isdigit(Partition[i]))
            break;
    }
    inm_strncpy_s(Parent, ARRAYSIZE(Parent), Partition, i+1);
    Parent[i+1] = '\0';
    DebugPrintf(SV_LOG_DEBUG,"Parition(%s) => Parent(%s) strlen=(%u)\n", Partition, Parent, strlen(Parent));
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return Parent;
}

int DisksInfoCollector::GetDisksAndVolumesInfo(std::vector<std::string> & DiskNames, VolumesInfoMultiMap_t& mmapDiskToVolInfo ) 
{
     DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	VolumeSummaries_t volumeSummaries;
    VolumeDynamicInfos_t volumeDynamicInfos;
	VolumeInfoCollector volumeCollector;
	volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, true); // TODO: should have a configurator option to deterime
    const VolumeInfoCollectorInfos_t& volumeInfos = volumeCollector.getVolumeInfos() ;
    ConstVolInfoIter volInfoIter = volumeInfos.begin() ;
    for( ; volInfoIter != volumeInfos.end() ; volInfoIter++ )
    {
        const VolumeInfoCollectorInfo& volInfo = volInfoIter->second ;
        if( volInfo.m_BootDisk == 0 && volInfo.m_DeviceType == VolumeSummary::DISK ) //exclude Linux Boot disks
        {
            DiskNames.push_back( volInfo.m_DeviceName ) ;
            ConstVolInfoIter childVolInfoIter = volInfo.m_Children.begin() ;
            for( ; childVolInfoIter != volInfo.m_Children.end() ; childVolInfoIter++ )
            {
                VOLUME_INFO dlmVolumeInfo ;
                const VolumeInfoCollectorInfo& childVolInfo = childVolInfoIter->second ;
                inm_strcpy_s( dlmVolumeInfo.VolumeName, ARRAYSIZE(dlmVolumeInfo.VolumeName), childVolInfo.m_DeviceName.c_str() ) ;
                dlmVolumeInfo.VolumeLength = childVolInfo.m_RawSize ;
                dlmVolumeInfo.StartingOffset = childVolInfo.m_PhysicalOffset ;
                dlmVolumeInfo.EndingOffset = childVolInfo.m_PhysicalOffset + childVolInfo.m_RawSize ;               
                mmapDiskToVolInfo.insert(std::make_pair(std::string(volInfo.m_DeviceName), dlmVolumeInfo));
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return 0 ;
}
//*************************************************************************************************
// Fetches the block devices in /dev/
// Output - DiskNames vector
// Output - Multimap of DiskNames to its Partitions/Volumes.
//*************************************************************************************************
//int DisksInfoCollector::GetDisksAndVolumesInfo(std::vector<std::string> & DiskNames, VolumesInfoMultiMap_t & mmapDiskToVolInfo)
//{
//    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
//    DIR * directory;
//
//    directory = opendir(LINUX_DEVNAME_DIR);
//
//    if (NULL == directory)
//    {
//        DebugPrintf(SV_LOG_ERROR,"Unable to open the device directory - %s. errno - %d\n", LINUX_DEVNAME_DIR, errno);
//        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//        return errno;
//    }
//
//    struct dirent * dirP;
//    while ((dirP = readdir(directory)) != NULL)
//    {
//        if ((strcmp(dirP->d_name, ".") == 0) || (strcmp(dirP->d_name, "..") == 0))
//        {
//            continue;
//        }
//        if (strstr(dirP->d_name, "ram" ) != NULL || strstr(dirP->d_name, "root" ) != NULL ) 
//        {
//            //if not sd device, skip it
//            continue;
//        }
//        char * FullName;
//        FullName = new char [MAX_PATH_COMMON + 1];
//        struct stat statBuf;
//        if (stat(FullName, &statBuf))
//        {
//            //stat failed, do not add it to the list
//            continue;
//        }
//        if (S_ISBLK(statBuf.st_mode))
//        {
//            //Block device found            
//            DebugPrintf(SV_LOG_DEBUG,"Block device - %s\n", dirP->d_name);
//            if (isdigit(dirP->d_name[strlen(dirP->d_name) - 1]))
//            {
//                DebugPrintf(SV_LOG_INFO,"%s is partition\n", FullName);
//                VOLUME_INFO VolInfo;
//                if(false == GetVolumeInfo(FullName, VolInfo))
//                {
//                    DebugPrintf(SV_LOG_ERROR,"Failed to fetch information about partition(%s).\n", FullName);
//                    continue;
//                }
//                char * ParentFullName;
//                ParentFullName = new char [MAX_PATH_COMMON + 1];
//                DebugPrintf(SV_LOG_INFO,"Parent of %s is %s\n", VolInfo.VolumeName, ParentFullName);
//                mmapDiskToVolInfo.insert(std::make_pair(std::string(ParentFullName), VolInfo));
//            }
//            else
//            {
//                //this is not a partition. hence a disk. so push it to disknames vector.
//                DebugPrintf(SV_LOG_INFO,"%s is disk\n", FullName);                
//                DiskNames.push_back(std::string(FullName));
//            }
//        }
//    }
//    if (closedir(directory) == -1)
//    {
//        DebugPrintf(SV_LOG_ERROR,"Unable to close the device directory - %s\n", LINUX_DEVNAME_DIR);
//        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//        return(errno);
//    }
//    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//    return 0;
//}

//*************************************************************************************************
// Fetches the SCSIID
// Output - Return 0:0:0:0 for linux for now.
//*************************************************************************************************
bool DisksInfoCollector::GetScsiId(const char * DiskName, SCSIID & ScsiId)
{
    ScsiId.Host     = 0;
	ScsiId.Channel  = 0;
	ScsiId.Target   = 0;
	ScsiId.Lun      = 0;
    return true;
}

//*************************************************************************************************
// This function will refresh the given disk. This will be called after updating MBRs/GPTs on a disk.
// Not sure if this is required in linux. So returning true.
//*************************************************************************************************
bool DisksInfoCollector::RefreshDisk(const char * DiskName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int fd;
    fd = open(DiskName, O_RDONLY);
    if (fd < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file descriptor for disk(%s). Error code: %d\n", DiskName, errno);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    DebugPrintf(SV_LOG_INFO,"Calling ioctl(BLKRRPART) to re-read partition table.\n");
    sync();
    sleep(2);
    if (ioctl(fd, BLKRRPART) != 0)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL(BLKRRPART) call failed for device(%s). Error code: %d\n\n", DiskName, errno);        
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

DLM_ERROR_CODE DisksInfoCollector::RestoreMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
													  std::list<std::string>& erraticVolumeGuids,
                                                      int mountNameType)
{
    return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DisksInfoCollector::OnlineOrOfflineDisk(const std::string& DiskName, const bool& bOnline)
{
	return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DisksInfoCollector::PostDynamicDiskRestoreOperns(std::vector<std::string>& RestoredDisk, bool restartVds)
{
	return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DisksInfoCollector::ConvertEfiToNormalPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp)
{
	return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DisksInfoCollector::ConvertNormalToEfiPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp)
{
	return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DisksInfoCollector::DeleteDlmRegitryEntries()
{
	return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DisksInfoCollector::CleanDisk(const std::string& DiskName)
{
	return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DiskCheckAndRectify()
{
	return DLM_ERR_SUCCESS;
}

#endif /* WIN32 */
