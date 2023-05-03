#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>

#include "dlrestore.h"


DLM_ERROR_CODE DLRestore::CollectDisksInfo (const char * BinaryFileName,
                                            DisksInfoMap_t & SrcMapDiskNamesToDiskInfo,
                                            DisksInfoMap_t & TgtMapDiskNamesToDiskInfo,
                                            std::list<std::string>& erraticVolumeGuids)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    //1. Collect Source DISK_INFO structure map from BLOB file
    RetVal = GetDiskInfoMapFromBlob(BinaryFileName, SrcMapDiskNamesToDiskInfo);
    if(DLM_ERR_SUCCESS != RetVal)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", BinaryFileName, RetVal);
        return RetVal;
    }

    //2. Collect the DISK_INFO structure from this machine.
    //This will be target map here.
	//need to work on this
	DlmPartitionInfoSubMap_t PartitionsInfoMapFromSys;
	bool bPartitionExist;
    RetVal = GetDiskInfoMapFromSystem(TgtMapDiskNamesToDiskInfo, PartitionsInfoMapFromSys, erraticVolumeGuids, bPartitionExist);
    if(DLM_ERR_SUCCESS != RetVal)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to collect the Target disks info map from this machine. Error Code - %u\n", RetVal);
        return RetVal;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Get the list of corrupted disks from the given source and target diskinfo map.
// Also return the map of source disknames to its possible target disknames(vector).
// Possible target disk names is vector because, we may not be able to uniquely find a target disk
//  to each source disk.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::GetCorruptedDisks (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                             DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                             std::vector<std::string> & CorruptedSrcDiskNames,
                                             std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks
                                             )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    //First compare based on disk sizes.
    FilterDisksBySizes(SrcMapDiskNamesToDiskInfo, TgtMapDiskNamesToDiskInfo, MapSrcDisksToTgtDisks);

    std::vector<std::string> IdentifiedSrcDisks; //Src disk names which found its corresponding target disk.
    std::vector<std::string> IdentifiedTgtDisks; //Tgt disk names which are uniquely mapped to Src disks.
    std::vector<std::string> GoodSrcDiskNames; //The source disk names which are not corrupted.
    FilterDisksByBootRecords(SrcMapDiskNamesToDiskInfo,
                             TgtMapDiskNamesToDiskInfo,
                             MapSrcDisksToTgtDisks,
                             IdentifiedSrcDisks,
                             IdentifiedTgtDisks,
                             GoodSrcDiskNames
                             );

    RemoveTheIsolatedDisks(MapSrcDisksToTgtDisks, IdentifiedSrcDisks, IdentifiedTgtDisks);

    //Find Corrupted Disks
    //Remove the good source disk names from all the source disk names
    FindCorruptedDisks(MapSrcDisksToTgtDisks, GoodSrcDiskNames, CorruptedSrcDiskNames);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// Restore the source disks structure onto the given target disks.
// Input is given as map of source and target disk names
// Return the disk names of source which are successfully restored.
// if the Error is DLM_ERR_DISK_DISAPPEARED, then returns the list of target disks diappeared
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreDiskStructure (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                                DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                                std::map<std::string, std::string> MapSrcToTgtDiskNames,
                                                std::vector<std::string> & RestoredSrcDiskNames,
                                                bool restartVds
                                                )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    std::list<std::string> lstDynDisk;
    StringArrMapIter_t iterSrcToTgt;

    std::vector<std::string> RestoredTgtDisks;

    for(iterSrcToTgt = MapSrcToTgtDiskNames.begin();
        iterSrcToTgt != MapSrcToTgtDiskNames.end();
        iterSrcToTgt++)
    {
        DisksInfoMapIter_t iterTgt = TgtMapDiskNamesToDiskInfo.find(iterSrcToTgt->second);
		DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find(iterSrcToTgt->first);

        if(iterTgt != TgtMapDiskNamesToDiskInfo.end())
        {
            if((DYNAMIC == iterTgt->second.DiskInfoSub.Type) || (DYNAMIC == iterSrc->second.DiskInfoSub.Type))
            {
                if(DLM_ERR_SUCCESS != CleanDisk(iterSrcToTgt->second))
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed to clean the dynamic disk : %s.\n",iterSrcToTgt->second.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_INFO,"Successfully cleaned the dynamic disk : %s.\n",iterSrcToTgt->second.c_str());
                    boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
                }
            }
        }        
        if(iterSrc != SrcMapDiskNamesToDiskInfo.end())
        {
            if(DYNAMIC == iterSrc->second.DiskInfoSub.Type)
            {
				lstDynDisk.push_back(iterSrcToTgt->second);
            }
        }
    }

    for(iterSrcToTgt = MapSrcToTgtDiskNames.begin();
        iterSrcToTgt != MapSrcToTgtDiskNames.end();
        iterSrcToTgt++)
    {
        bool addToList = true;
        DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find(iterSrcToTgt->first);
        if(iterSrc != SrcMapDiskNamesToDiskInfo.end())
        {
            //If the flag of source disk is not complete, do not restore it.
            //Ignore this for raw disks and we need not to do anything for raw disks.
            if( (RAWDISK != iterSrc->second.DiskInfoSub.FormatType) &&
                (FLAG_COMPLETE != iterSrc->second.DiskInfoSub.Flag) )
            {
				if(BASIC == iterSrc->second.DiskInfoSub.Type)
				{
					DebugPrintf(SV_LOG_ERROR,"Skipping the restore for disk(%s) because its complete info is not available(Flag - %llu).\n", iterSrcToTgt->second.c_str(), iterSrc->second.DiskInfoSub.Flag);
					/*RetVal = DLM_ERR_INCOMPLETE;*/
					continue;
				}
            }

            //Restore Single Disk (Tgt , Src , SrcDiskInfo)
            if( DLM_ERR_SUCCESS == RestoreDisk(iterSrcToTgt->second.c_str(), iterSrcToTgt->first.c_str(), iterSrc->second))
            {
                if(DYNAMIC != iterSrc->second.DiskInfoSub.Type)
                {
                    // Refresh the target disk after writing MBRs/GPTs.
                    // This is required in windows to update diskmgmt.
                    if(!RefreshDisk(iterSrcToTgt->second.c_str()))
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to refresh/update the disk properties for disk(%s) after writing MBR.\n",iterSrcToTgt->second.c_str());
                        addToList = false;
                        RetVal = DLM_ERR_INCOMPLETE;
                        
                    }
                }
                if(addToList)
                {
                   RestoredSrcDiskNames.push_back(iterSrc->first);
                   RestoredTgtDisks.push_back(iterSrcToTgt->second);
                }  
            }
            else
                RetVal = DLM_ERR_INCOMPLETE;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "The Source disk(%s) does not exist in Source Disk Info Map.\n",iterSrcToTgt->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
        }
    }

	boost::this_thread::sleep(boost::posix_time::milliseconds(2000));

    if(!lstDynDisk.empty())
    {
        if(RetVal == DLM_ERR_SUCCESS)
        {
            RetVal = PostDynamicDiskRestoreOperns(RestoredTgtDisks, restartVds);
            if(DLM_ERR_SUCCESS != RetVal)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to perform dynamic disk operations post dynamic disk structure update on some target disk\n");
                DebugPrintf(SV_LOG_ERROR,"Some of the dynamic disks might not be in proper state\n");
                if(DLM_ERR_DISK_DISAPPEARED == RetVal)
                {
                    RestoredSrcDiskNames = RestoredTgtDisks;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"Successfully performed dynamic disk operations post dynamic disk structure update on target disks\n");
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Get New Target Disk Info Map from the system.
// This is required because this will be called after restore disk structure call.
// Compare the volumes offsets of source map and target map of corresponding disks
// and get the map of corresponding source and target volumes.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::GetMapSrcToTgtVol (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                             std::map<std::string, std::string> MapSrcToTgtDiskNames,
                                             std::map<std::string, std::string> & MapSrcToTgtVolumeNames,
                                             std::list<std::string>& erraticVolumeGuids)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    //Get the target disk info map.
    DisksInfoMap_t  TgtMapDiskNamesToDiskInfo;
	DlmPartitionInfoSubMap_t PartitionsInfoMapFromSys;
	bool bPartitionExist;
    RetVal = GetDiskInfoMapFromSystem(TgtMapDiskNamesToDiskInfo, PartitionsInfoMapFromSys, erraticVolumeGuids, bPartitionExist);
    if(DLM_ERR_SUCCESS != RetVal)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to collect the disks info from the system. Error Code - %u\n", RetVal);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return RetVal;
    }

    //For each source and target disk
    for(StringArrMapIter_t iterDisks = MapSrcToTgtDiskNames.begin();
        iterDisks != MapSrcToTgtDiskNames.end();
        iterDisks++)
    {
        DebugPrintf(SV_LOG_INFO,"SrcDisk = %s <==> TgtDisk = %s\n", iterDisks->first.c_str(), iterDisks->second.c_str());

        //Get the corresponding diskinfoiter from the diskinfomap
        DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find(iterDisks->first);
        if(iterSrc == SrcMapDiskNamesToDiskInfo.end()) //Src
        {
            DebugPrintf(SV_LOG_ERROR,"The input Source disk(%s) is not found in Source DiskInfo Map.\n",iterDisks->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }
        DisksInfoMapIter_t iterTgt = TgtMapDiskNamesToDiskInfo.find(iterDisks->second);
        if(iterTgt == TgtMapDiskNamesToDiskInfo.end()) //Tgt
        {
            DebugPrintf(SV_LOG_ERROR,"The input Target disk(%s) is not found in Target DiskInfo Map.\n",iterDisks->second.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }

        //Get the VolumesInfo of these disks.
        if(0 == iterSrc->second.DiskInfoSub.VolumeCount) //Src
        {
            //This is fine as a source disk can be empty.
            DebugPrintf(SV_LOG_INFO,"No volumes are present on the Source disk(%s).\n",iterSrc->first.c_str());
            continue;
        }
        VolumesInfoVec_t vecSrcVolInfo = iterSrc->second.VolumesInfo;
        if(0 == iterTgt->second.DiskInfoSub.VolumeCount) //Tgt
        {
            //This means few source volumes will not have corresponding target volumes.
            //So not fine. hence return DLM_ERR_INCOMPLETE
            DebugPrintf(SV_LOG_ERROR,"No volumes are present on the Target disk(%s).\n",iterTgt->first.c_str());
            RetVal = DLM_ERR_INCOMPLETE;
            continue;
        }
        VolumesInfoVec_t vecTgtVolInfo = iterTgt->second.VolumesInfo;

        //Compare the VolumesInfo vectors and generate the map of volumes.
        VolumesInfoVecIter_t iterSrcVol = vecSrcVolInfo.begin(); //Src
        VolumesInfoVecIter_t iterSrcVolLast = vecSrcVolInfo.end(); //Src
        for(; iterSrcVol != iterSrcVolLast; iterSrcVol++) //Src
        {
            DebugPrintf(SV_LOG_DEBUG,"SourceVolumeName = %s\n",iterSrcVol->VolumeName);
            bool bMatchExist = false;
            VolumesInfoVecIter_t iterTgtVol = vecTgtVolInfo.begin(); //Tgt
            VolumesInfoVecIter_t iterTgtVolLast = vecTgtVolInfo.end(); //Tgt
            for(; iterTgtVol != iterTgtVolLast; iterTgtVol++) //Tgt
            {
                DebugPrintf(SV_LOG_DEBUG,"TargetVolumeName = %s\n",iterTgtVol->VolumeName);
                if((iterSrcVol->StartingOffset == iterTgtVol->StartingOffset)
                    && (iterSrcVol->EndingOffset == iterTgtVol->EndingOffset)
                    && (iterSrcVol->VolumeLength == iterTgtVol->VolumeLength))
                {
                    DebugPrintf(SV_LOG_DEBUG,"Found the match.\n");

                    if (boost::algorithm::istarts_with(iterTgtVol->VolumeName, VOLUME_DUMMYNAME_PREFIX)) {
                        // use guid as there is no drive letter nor mount point name
                        MapSrcToTgtVolumeNames[iterSrcVol->VolumeName] = iterTgtVol->VolumeGuid;
                    } else {
                        MapSrcToTgtVolumeNames[iterSrcVol->VolumeName] = iterTgtVol->VolumeName;
                    }
                    bMatchExist = true;
                    break;
                }
            }
            if(!bMatchExist)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the corresponding Target volume for the Source volume(%s).\n",iterSrcVol->VolumeName);
                RetVal = DLM_ERR_INCOMPLETE;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// This function clears the disk meta on the given target disk based on the given soruce disk
// Ignore if RAW disk - Need not restore and its not an error too.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::ClearDynamicDiskMetaData(DisksInfoMap_t&  SrcMapDiskNamesToDiskInfo,
                                                   std::map<std::string, std::string>& MapSrcToTgtDiskNames)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE rc = DLM_ERR_SUCCESS;
    StringArrMapIter_t iter(MapSrcToTgtDiskNames.begin());
    StringArrMapIter_t iterEnd(MapSrcToTgtDiskNames.end());
    for(/* empty */; iter != iterEnd; iter++)
    {
        DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find((*iter).first);
        if(iterSrc != SrcMapDiskNamesToDiskInfo.end())
        {
            if(DYNAMIC == (*iterSrc).second.DiskInfoSub.Type)
            {
                std::vector<unsigned char> buffer;
                if(MBR == (*iterSrc).second.DiskInfoSub.FormatType)
                {
                    buffer.resize(MBR_DYNAMIC_BOOT_SECTOR_LENGTH);
                }
                else if(GPT == (*iterSrc).second.DiskInfoSub.FormatType)
                {
                    buffer.resize(GPT_DYNAMIC_BOOT_SECTOR_LENGTH);
                }
                memset(&buffer[0], 0, buffer.size());
                if(!WriteToDisk((*iter).second.c_str(), 0, buffer.size(), &buffer[0]))
                {
                    std::string msg("Failed to clear ");
                    msg += MBR == (*iterSrc).second.DiskInfoSub.FormatType ? "MBR" : "GPT";
                    msg += " disk meta data for target disk ";
                    msg += (*iter).second;
                    msg += " (source disk ";
                    msg += (*iter).first;
                    msg +=")\n";
                    DebugPrintf(SV_LOG_ERROR, msg.c_str());
                    rc = DLM_ERR_DISK_WRITE;
                    break;
                }
                else
                {
                    std::string msg("Successfully cleared ");
                    msg += MBR == (*iterSrc).second.DiskInfoSub.FormatType ? "MBR" : "GPT";
                    msg += " disk meta data for target disk ";
                    msg += (*iter).second;
                    msg += " (source disk ";
                    msg += (*iter).first;
                    msg +=")\n";
                    DebugPrintf(SV_LOG_ERROR, msg.c_str());
                    if(MBR == (*iterSrc).second.DiskInfoSub.FormatType)
                    {
                        SV_LONGLONG bytestoskip = (*iterSrc).second.DiskInfoSub.Size - LDM_DYNAMIC_BOOT_SECTOR_LENGTH;
                        buffer.resize(LDM_DYNAMIC_BOOT_SECTOR_LENGTH);
                        memset(&buffer[0], 0, buffer.size());
                        if( !WriteToDisk((*iter).second.c_str(), bytestoskip, buffer.size(), &buffer[0]) )
                        {
                            std::string msg("Failed to clear ");
                            msg += MBR == (*iterSrc).second.DiskInfoSub.FormatType ? "MBR" : "GPT";
                            msg += " disk meta data for target disk ";
                            msg += (*iter).second;
                            msg += " (source disk ";
                            msg += (*iter).first;
                            msg +=")\n";
                            DebugPrintf(SV_LOG_ERROR, msg.c_str());
                            rc = DLM_ERR_DISK_WRITE;
                            break;
                        }
                    }
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rc;
}

//*************************************************************************************************
// This function restores the given source disk structure onto the given target disk.
// Restore MBR and EBR if MBR disk
// Restore MBR and LDM if dynamic + MBR disk
// Restore GPT if GPT disk
// Ignore if RAW disk - Need not restore and its not an error too.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo)
{
    if(RAWDISK == SrcDiskInfo.DiskInfoSub.FormatType)
    {
        DebugPrintf(SV_LOG_INFO,"The Source disk(%s) for the corresponding Target disk(%s) is RAW disk.\n", SrcDiskName, TgtDiskName);
        DebugPrintf(SV_LOG_INFO,"Hence not changing the target disk. Treating this as success.\n");
        return DLM_ERR_SUCCESS;
    }
    else if(MBR == SrcDiskInfo.DiskInfoSub.FormatType)
    {
        return RestoreMbrDisk(TgtDiskName, SrcDiskName, SrcDiskInfo);
    }
    else if(GPT == SrcDiskInfo.DiskInfoSub.FormatType)
    {
        return RestoreGptDisk(TgtDiskName, SrcDiskName, SrcDiskInfo);
    }
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// Restore the MBR of given source disk to given target disk
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreMbrDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"Restoring MBR of source disk (%s) onto target disk (%s).\n", SrcDiskName, TgtDiskName);

	if(BASIC == SrcDiskInfo.DiskInfoSub.Type)
	{
		if(!WriteToDisk(TgtDiskName, 0, MBR_BOOT_SECTOR_LENGTH, SrcDiskInfo.MbrSector))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write MBR Record of Source disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_DISK_WRITE;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated MBR Record of Source disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
		}
		//Check if any EBRs exist and write EBRs to disk if they exist.
		if(0 != SrcDiskInfo.DiskInfoSub.EbrCount)
		{
			DebugPrintf(SV_LOG_INFO,"Restoring EBR's of source disk (%s) onto target disk (%s).\n", SrcDiskName, TgtDiskName);
			if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(std::string(TgtDiskName), false))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Offline the disk : %s.\n",TgtDiskName);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully offline the disk : %s.\n",TgtDiskName);
                boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
			}
			//Read each partition and check for extended.
			for(int i = 0; i < MBR_MAGIC_COUNT; i++)
			{
				size_t PartitionTypeOffset = MBR_PARTITION_1_PARTITIONTYPE + i * 16;
				if(isExtendedPartition(SrcDiskInfo.MbrSector, PartitionTypeOffset))
				{
					size_t BootRecordOffset = MBR_PARTITION_1_STARTINGSECTOR + i * 16;
					DebugPrintf(SV_LOG_INFO,"Partition-%d is an Extended Partition. MBR Offset for starting sector of this - %u.\n",i+1,BootRecordOffset);
					SV_LONGLONG ExtPartitionStartOffsetValue = 0;
					GetEBRStartingOffset(SrcDiskInfo.MbrSector, BootRecordOffset, ExtPartitionStartOffsetValue);
					if(0 != ExtPartitionStartOffsetValue)
					{
						ExtPartitionStartOffsetValue *= SrcDiskInfo.DiskInfoSub.BytesPerSector;
						if(DLM_ERR_SUCCESS != RestoreEBRs(TgtDiskName, ExtPartitionStartOffsetValue, SrcDiskInfo.DiskInfoSub.BytesPerSector, SrcDiskInfo.EbrSectors))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to restore EBRs of Source disk(%s) onto Target disk(%s).\n", SrcDiskName, TgtDiskName);

							if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(std::string(TgtDiskName), true))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to Online the disk : %s.\n",TgtDiskName);
							}
							else
							{
								DebugPrintf(SV_LOG_INFO,"Successfully online the disk : %s.\n",TgtDiskName);
								boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
							}

							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return DLM_ERR_WRITE_EBR;
						}
					}
					break; //as only one extended partition can exist per disk
				}
			}
			if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(std::string(TgtDiskName), true))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Online the disk : %s.\n",TgtDiskName);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully online the disk : %s.\n",TgtDiskName);
				boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
			}
		}
	}
	else if(DYNAMIC == SrcDiskInfo.DiskInfoSub.Type)
	{
		if(!WriteToDisk(TgtDiskName, 0, MBR_BOOT_SECTOR_LENGTH, SrcDiskInfo.MbrDynamicSector))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write MBR Record of Source dynamic disk(%s) to Target disk(%s)..\n", SrcDiskName, TgtDiskName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_DISK_WRITE;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated MBR Record of Source dynamic disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
		}

		SV_LONGLONG bytestoskip = SrcDiskInfo.DiskInfoSub.Size - LDM_DYNAMIC_BOOT_SECTOR_LENGTH ;
		if(!WriteToDisk(TgtDiskName, bytestoskip, LDM_DYNAMIC_BOOT_SECTOR_LENGTH, SrcDiskInfo.LdmDynamicSector))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write LDM  Record of Source dynamic disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_DISK_WRITE;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated LDM Record of Source dynamic disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// Restore the GPT of given source disk to given target disk
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreGptDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"Restoring GPT of source disk (%s) onto target disk (%s).\n", SrcDiskName, TgtDiskName);

	size_t bytestowrite;
    if(DYNAMIC == SrcDiskInfo.DiskInfoSub.Type)
	{
		bytestowrite = GPT_DYNAMIC_BOOT_SECTOR_LENGTH;
		if(!WriteToDisk(TgtDiskName, 0, bytestowrite, SrcDiskInfo.GptDynamicSector))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write GPT Record of Source dynamic disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_DISK_WRITE;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated GPT Record of Source dynamic disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
		}
	}
	else if(BASIC == SrcDiskInfo.DiskInfoSub.Type)
	{
		bytestowrite = GPT_BOOT_SECTOR_LENGTH;
		if(!WriteToDisk(TgtDiskName, 0, bytestowrite, SrcDiskInfo.GptSector))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write GPT Record of Source disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_DISK_WRITE;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated GPT Record of Source disk(%s) to Target disk(%s).\n", SrcDiskName, TgtDiskName);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// Restore the EBRs of given source disk to given target disk
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreEBRs(const char * DiskName, SV_LONGLONG PartitionStartOffset, SV_ULONGLONG BytesPerSector, std::vector<EBR_SECTOR> vecEBRs)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;
    SV_LONGLONG BytesToSkip = PartitionStartOffset;

    std::vector<EBR_SECTOR>::iterator iterVec = vecEBRs.begin();
    for(; iterVec != vecEBRs.end(); iterVec++)
    {
        DebugPrintf(SV_LOG_INFO,"Bytes to skip = %lld\n", BytesToSkip);
        if(!WriteToDisk(DiskName, BytesToSkip, MBR_BOOT_SECTOR_LENGTH, iterVec->EbrSector))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to write the EBR sector to disk - %s\n",DiskName);
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            RetVal = DLM_ERR_DISK_WRITE;
            break;
        }

        SV_UCHAR EbrSecondEntry[MBR_MAGIC_COUNT];
        GetBytesFromMBR(iterVec->EbrSector, EBR_SECOND_ENTRY_STARTING_INDEX, MBR_MAGIC_COUNT, EbrSecondEntry);
        SV_LONGLONG Offset = 0;
        ConvertArrayFromHexToDec(EbrSecondEntry, MBR_MAGIC_COUNT, Offset);
        DebugPrintf(SV_LOG_INFO,"Offset = %lld\n",Offset);
        if(Offset == 0)
        {
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            RetVal = DLM_ERR_SUCCESS;
            break;
        }

        BytesToSkip = PartitionStartOffset + Offset * BytesPerSector;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Get Map of Source disk to possible Target disks based on disk size.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::FilterDisksBySizes(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                             DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                             std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks
                                             )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.begin();
    for(; iterSrc != SrcMapDiskNamesToDiskInfo.end(); iterSrc++) //for each src disk
    {
        DebugPrintf(SV_LOG_DEBUG,"Source Disk Name(%s) - Size(%lld) ==> \n", iterSrc->first.c_str(), iterSrc->second.DiskInfoSub.Size);
        std::vector<std::string> PossibleTgtDisks;

        DisksInfoMapIter_t iterTgt = TgtMapDiskNamesToDiskInfo.begin();
        for(; iterTgt != TgtMapDiskNamesToDiskInfo.end(); iterTgt++) //for each tgt disk
        {
            DebugPrintf(SV_LOG_DEBUG,"\t Target Disk Name(%s) - Size(%lld)\n", iterTgt->first.c_str(), iterTgt->second.DiskInfoSub.Size);
            if(iterSrc->second.DiskInfoSub.Size <= iterTgt->second.DiskInfoSub.Size) //chk disk size
            {
                DebugPrintf(SV_LOG_DEBUG,"Found Match for Disk Size.\n");
                PossibleTgtDisks.push_back(iterTgt->first); //push to possible tgt disks if matches
            }
        }
        MapSrcDisksToTgtDisks[iterSrc->first] = PossibleTgtDisks;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// Here the following info is fetched.
// 1. Filter the Map of Source disk to possible Target disks based on boot records(MBR/GPT).
// 2. Fill the vector of source disks which found its unique target disk.
// 3. Fill the vector of target disks which found its unique source disk.
// 4. Fill the Good source disk names (For this it should satisfy follow conditions)
//      a. If the disk is uniquely identified and
//      b. its MBR/EBRs match for EBR disk and GPTs match for GPT disks.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::FilterDisksByBootRecords(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                                   DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                                   std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks,
                                                   std::vector<std::string> & IdentifiedSrcDisks,
                                                   std::vector<std::string> & IdentifiedTgtDisks,
                                                   std::vector<std::string> & GoodSrcDiskNames
                                                   )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::map<std::string, std::vector<std::string> > TempMapSrcDisksToTgtDisks; //This will contain filtered disks map in the end.

	std::vector<std::string>::iterator iterVecTgtDisk;
    std::map<std::string, std::vector<std::string> >::iterator iter = MapSrcDisksToTgtDisks.begin();
    for(; iter != MapSrcDisksToTgtDisks.end(); iter++)
    {
        std::vector<std::string> PossibleTgtDisks;
        iterVecTgtDisk = iter->second.begin();
        for(; iterVecTgtDisk != iter->second.end(); iterVecTgtDisk++)
        {
            if( CompareBootRecords(iter->first,                 //Source Disk Name
                                   *iterVecTgtDisk,             //Target Disk Name
                                   SrcMapDiskNamesToDiskInfo,   //Source Map of DISK_INFO
                                   TgtMapDiskNamesToDiskInfo,   //Target Map of DISK_INFO
                                   GoodSrcDiskNames             //Disks which are not corrupted.
                                   )
              )
            {
                //This means two disk are uniquely mapped.
                PossibleTgtDisks.clear();
                PossibleTgtDisks.push_back(*iterVecTgtDisk);
                IdentifiedSrcDisks.push_back(iter->first);
                IdentifiedTgtDisks.push_back(*iterVecTgtDisk); //Push uniquely identified disk.
                continue;
            }
            PossibleTgtDisks.push_back(*iterVecTgtDisk);
        }
        TempMapSrcDisksToTgtDisks[iter->first] = PossibleTgtDisks;
    }

    MapSrcDisksToTgtDisks = TempMapSrcDisksToTgtDisks; //Assign the filtered disks back to the map.

	//just for debugging purpose
	for(iter = MapSrcDisksToTgtDisks.begin(); iter != MapSrcDisksToTgtDisks.end(); iter++)
    {
		DebugPrintf(SV_LOG_INFO,"Source Disk - %s.\n", iter->first.c_str());
		DebugPrintf(SV_LOG_INFO,"Possible Target Disks - \n");
		iterVecTgtDisk = iter->second.begin();
        for(; iterVecTgtDisk != iter->second.end(); iterVecTgtDisk++)
        {
			DebugPrintf(SV_LOG_INFO,"\t%s\n", iterVecTgtDisk->c_str());
		}
	}

	iterVecTgtDisk = IdentifiedSrcDisks.begin();
	DebugPrintf(SV_LOG_INFO,"Uniquely identified Source Disks - \n");
	for(; iterVecTgtDisk != IdentifiedSrcDisks.end(); iterVecTgtDisk++)
	{
		DebugPrintf(SV_LOG_INFO,"\t%s\n", iterVecTgtDisk->c_str());
	}

	iterVecTgtDisk = IdentifiedTgtDisks.begin();
	DebugPrintf(SV_LOG_INFO,"Uniquely identified Target Disks - \n");
	for(; iterVecTgtDisk != IdentifiedTgtDisks.end(); iterVecTgtDisk++)
	{
		DebugPrintf(SV_LOG_INFO,"\t%s\n", iterVecTgtDisk->c_str());
	}

	iterVecTgtDisk = GoodSrcDiskNames.begin();
	DebugPrintf(SV_LOG_INFO,"Good Source Disks - \n");
	for(; iterVecTgtDisk != GoodSrcDiskNames.end(); iterVecTgtDisk++)
	{
		DebugPrintf(SV_LOG_INFO,"\t%s\n", iterVecTgtDisk->c_str());
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// Check the boot records of source and target disks.
// Comparison is if Source disk is MBR , then check if target disk is MBR and then compare MBRs
//          else if Source disk is GPT , then check if target disk is GPT and then compare GPTs
//          else return false for all other scenarios(this includes RAW disk).
//*************************************************************************************************
bool DLRestore::CompareBootRecords(std::string SrcDiskName,
                                   std::string TgtDiskName,
                                   DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                   DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                   std::vector<std::string> & GoodSrcDiskNames
                                   )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    bool RetVal = false;
    DisksInfoMapIter_t iterSrc = SrcMapDiskNamesToDiskInfo.find(SrcDiskName);
    DisksInfoMapIter_t iterTgt = TgtMapDiskNamesToDiskInfo.find(TgtDiskName);

    if( (iterSrc != SrcMapDiskNamesToDiskInfo.end())
        && (iterTgt != TgtMapDiskNamesToDiskInfo.end()) )
    {
        //MBR, BASIC scnario
        if( (MBR == iterSrc->second.DiskInfoSub.FormatType)
            && (MBR == iterTgt->second.DiskInfoSub.FormatType)
			&& (BASIC == iterSrc->second.DiskInfoSub.Type)
			&& (BASIC == iterTgt->second.DiskInfoSub.Type))
        {
            RetVal = CompareMBRs(iterSrc->second.MbrSector, iterTgt->second.MbrSector);
            if(RetVal)
            {
                //MBRs matched, also check for EBRs if exists.
                //If EBRs match too, then this disk is not corrupted. So a good disk.
                //Note: Even if EBRs do not match, do not change RetVal as disk is still identified.
                if(CompareEBRs(iterSrc->second, iterTgt->second))
                {
                    DebugPrintf(SV_LOG_INFO,"Good Disk(Basic) found(MBR type) [Src-%s <==> Tgt-%s].\n", iterSrc->first.c_str(), iterTgt->first.c_str());
                    GoodSrcDiskNames.push_back(SrcDiskName);
                }
            }
        }
        //GPT , BASIC scenario
        else if( (GPT == iterSrc->second.DiskInfoSub.FormatType)
                 && (GPT == iterTgt->second.DiskInfoSub.FormatType)
				 && (BASIC == iterSrc->second.DiskInfoSub.Type)
				 && (BASIC == iterTgt->second.DiskInfoSub.Type))
        {
            RetVal = CompareGPTs(iterSrc->second.GptSector, iterTgt->second.GptSector);
            if( RetVal ) //Since GPTs matched for GPT disks, this is not corrupted. Hence good.
            {
                DebugPrintf(SV_LOG_INFO,"Good Disk(Basic) found(GPT type) [Src-%s <==> Tgt-%s].\n", iterSrc->first.c_str(), iterTgt->first.c_str());
                GoodSrcDiskNames.push_back(SrcDiskName);
            }
        }
		//MBR, DYNAMIC scenario
		else if( (MBR == iterSrc->second.DiskInfoSub.FormatType)
                 && (MBR == iterTgt->second.DiskInfoSub.FormatType)
				 && (DYNAMIC == iterSrc->second.DiskInfoSub.Type)
				 && (DYNAMIC == iterTgt->second.DiskInfoSub.Type))
        {
			RetVal = CompareMBRs(iterSrc->second.MbrDynamicSector, iterTgt->second.MbrDynamicSector);
            if( RetVal ) //Since MBRs matched for dynamic disks, need to check the LDMs.
            {
				DebugPrintf(SV_LOG_INFO,"Good Disk(Dynamic) found(MBR type) [Src-%s <==> Tgt-%s].\n", iterSrc->first.c_str(), iterTgt->first.c_str());
				GoodSrcDiskNames.push_back(SrcDiskName);
				//Removed LDM comparison as some inconsistency found, We may need to add soemother logic to find out a good disk
				//RetVal = CompareDynamicLDMs(iterSrc->second.LdmDynamicSector, iterTgt->second.LdmDynamicSector);
				//if( RetVal ) //Since LDMs matched for MBR dynamic disks, this is not corrupted. Hence good.
				//{
				//	DebugPrintf(SV_LOG_INFO,"Good Disk(Dynamic) found(MBR type) [Src-%s <==> Tgt-%s].\n", iterSrc->first.c_str(), iterTgt->first.c_str());
				//	GoodSrcDiskNames.push_back(SrcDiskName);
				//}
            }
        }
		//GPT , DYNAMIC scenario
        else if( (GPT == iterSrc->second.DiskInfoSub.FormatType)
                 && (GPT == iterTgt->second.DiskInfoSub.FormatType)
				 && (DYNAMIC == iterSrc->second.DiskInfoSub.Type)
				 && (DYNAMIC == iterTgt->second.DiskInfoSub.Type))
        {
			//Removed LDM comparison as some inconsistency found, We may need to add soemother logic to find out a good disk
			RetVal = CompareGPTs(iterSrc->second.GptDynamicSector, iterTgt->second.GptDynamicSector);
            if( RetVal ) //Since GPTs matched for GPT disks, this is not corrupted. Hence good.
            {
                DebugPrintf(SV_LOG_INFO,"Good Disk(Dynamic) found(GPT type) [Src-%s <==> Tgt-%s].\n", iterSrc->first.c_str(), iterTgt->first.c_str());
                GoodSrcDiskNames.push_back(SrcDiskName);
            }
        }
        //RAW scenario.
        //If both source and target disks are raw, consider them as good.
        else if( (RAWDISK == iterSrc->second.DiskInfoSub.FormatType)
                 && (RAWDISK == iterTgt->second.DiskInfoSub.FormatType) )
        {
            DebugPrintf(SV_LOG_INFO,"Good Disk found(RAW type) [Src-%s <==> Tgt-%s].\n", iterSrc->first.c_str(), iterTgt->first.c_str());
            GoodSrcDiskNames.push_back(SrcDiskName);
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

//*************************************************************************************************
// Remove the uniquely identified target disks from the possible target disks of the input map
// If source disk is already identified, skip the removal part.
// For remaining source disks, remove the target disks from its existing possible target disks
//  vector which are already identified to some other source disks uniquely.
//*************************************************************************************************
void DLRestore::RemoveTheIsolatedDisks(std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks,
                                       std::vector<std::string> IdentifiedSrcDisks,
                                       std::vector<std::string> IdentifiedTgtDisks
                                       )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::map<std::string, std::vector<std::string> > TempMapSrcDisksToTgtDisks; //This will contain filtered disks map in the end.
	std::vector<std::string>::iterator iterVecTgtDisk;

    std::map<std::string, std::vector<std::string> >::iterator iter = MapSrcDisksToTgtDisks.begin();
    for(; iter != MapSrcDisksToTgtDisks.end(); iter++)
    {
        if( VectorFind(iter->first, IdentifiedSrcDisks) ) //if source disk is already identified, skip the removal part.
        {
            TempMapSrcDisksToTgtDisks[iter->first] = iter->second;
        }
        else // remove the identified target disks now
        {
            //For each source disk, remove the target disks from vector which
            //are already identified to some other source disk uniquely.
            std::vector<std::string> PossibleTgtDisks;
            iterVecTgtDisk = iter->second.begin();
            for(; iterVecTgtDisk != iter->second.end(); iterVecTgtDisk++)
            {
                if(!VectorFind(*iterVecTgtDisk, IdentifiedTgtDisks) )
                    PossibleTgtDisks.push_back(*iterVecTgtDisk);
            }
            TempMapSrcDisksToTgtDisks[iter->first] = PossibleTgtDisks;
        }
    }

    MapSrcDisksToTgtDisks = TempMapSrcDisksToTgtDisks;

	//just for debugging purpose
	for(iter = MapSrcDisksToTgtDisks.begin(); iter != MapSrcDisksToTgtDisks.end(); iter++)
    {
		DebugPrintf(SV_LOG_INFO,"Source Disk - %s.\n", iter->first.c_str());
		DebugPrintf(SV_LOG_INFO,"Possible Target Disks - \n");
		iterVecTgtDisk = iter->second.begin();
        for(; iterVecTgtDisk != iter->second.end(); iterVecTgtDisk++)
        {
			DebugPrintf(SV_LOG_INFO,"\t%s\n", iterVecTgtDisk->c_str());
		}
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//*************************************************************************************************
// This function returns the corrupted source disk names.
// Remove the good disks from all disks
//*************************************************************************************************
void DLRestore::FindCorruptedDisks(std::map<std::string, std::vector<std::string> > MapSrcDisksToTgtDisks,
                                   std::vector<std::string> GoodSrcDiskNames,
                                   std::vector<std::string> & CorruptedSrcDiskNames
                                   )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::map<std::string, std::vector<std::string> >::iterator iter = MapSrcDisksToTgtDisks.begin();
    for(; iter != MapSrcDisksToTgtDisks.end(); iter++)
    {
        if(!VectorFind(iter->first, GoodSrcDiskNames))
        {
            CorruptedSrcDiskNames.push_back(iter->first);
        }
    }

	//Debugging purpose
	DebugPrintf(SV_LOG_INFO,"Corrupted Disks are- \n");
	std::vector<std::string>::iterator iterVec = CorruptedSrcDiskNames.begin();
	for(; iterVec != CorruptedSrcDiskNames.end(); iterVec++)
	{
		DebugPrintf(SV_LOG_INFO,"\t%s\n", iterVec->c_str());
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//*************************************************************************************************
// Check if given element exists in the given Vector
// If exists return true, else return false
//*************************************************************************************************
bool DLRestore::VectorFind(std::string Element , std::vector<std::string> VecElements)
{
    std::vector<std::string>::iterator iter = VecElements.begin();

    for(; iter != VecElements.end(); iter++)
    {
        if(0 == strcmpi(Element.c_str(), (*iter).c_str()))
            return true;
    }

    return false;
}

//*************************************************************************************************
// Compare the BASIC, GPT sectors
//*************************************************************************************************
bool DLRestore::CompareGPTs(SV_UCHAR SrcGptSector[], SV_UCHAR TgtGptSector[])
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(int i=0; i<GPT_BOOT_SECTOR_LENGTH; i++)
    {
        if( SrcGptSector[i] != TgtGptSector[i] )
            return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// Compare the Dynamic, GPT sectors
//*************************************************************************************************
bool DLRestore::CompareDynamicGPTs(SV_UCHAR SrcGptSector[], SV_UCHAR TgtGptSector[])
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(int i=0; i<GPT_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
    {
        if( SrcGptSector[i] != TgtGptSector[i] )
            return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// Compare the Dynamic MBR sectors
//*************************************************************************************************
bool DLRestore::CompareDynamicMBRs(SV_UCHAR SrcMbrSector[], SV_UCHAR TgtMbrSector[])
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(int i=0; i<MBR_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
    {
        if( SrcMbrSector[i] != TgtMbrSector[i] )
            return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// Compare the Dynamic MBR sectors
//*************************************************************************************************
bool DLRestore::CompareDynamicLDMs(SV_UCHAR SrcLdmSector[], SV_UCHAR TgtLdmSector[])
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(int i=0; i<LDM_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
    {
        if( SrcLdmSector[i] != TgtLdmSector[i] )
            return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// Compare the Basic MBR sectors
//*************************************************************************************************
bool DLRestore::CompareMBRs(SV_UCHAR SrcMbrSector[], SV_UCHAR TgtMbrSector[])
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(int i=0; i<MBR_BOOT_SECTOR_LENGTH; i++)
    {
        if( SrcMbrSector[i] != TgtMbrSector[i] )
            return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// This function compares the EBRs of source and target disk
// If EBRs does not exist for both, return true.
// If number of EBRs for both does not match, return false
// If EBRs does not match, return false
//*************************************************************************************************
bool DLRestore::CompareEBRs(DISK_INFO SrcDiskInfo, DISK_INFO TgtDiskInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    if( (SrcDiskInfo.DiskInfoSub.EbrCount == 0)
        && (TgtDiskInfo.DiskInfoSub.EbrCount == 0) )
    {
        return true;
    }
    else if( SrcDiskInfo.DiskInfoSub.EbrCount != TgtDiskInfo.DiskInfoSub.EbrCount )
    {
        return false;
    }
    else
    {
        std::vector<EBR_SECTOR>::iterator iterSrcEbrSector = SrcDiskInfo.EbrSectors.begin();
        std::vector<EBR_SECTOR>::iterator iterTgtEbrSector = TgtDiskInfo.EbrSectors.begin();
        for(;iterSrcEbrSector != SrcDiskInfo.EbrSectors.end(); iterSrcEbrSector++, iterTgtEbrSector++)
        {
            if(!CompareMBRs(iterSrcEbrSector->EbrSector, iterTgtEbrSector->EbrSector))
                return false;
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//*************************************************************************************************
// Restore the source disks structure onto the given target disks.
// Input is given as map of source and target disk names
// Return the disk names of source which are successfully restored.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreDiskPartitions (const char * BinaryFileName,
                                                std::map<DiskName_t, DiskName_t>& MapSrcToTgtDiskNames,
                                                std::set<DiskName_t> & RestoredSrcDiskNames
                                                )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;
	DebugPrintf(SV_LOG_DEBUG, "The partition file name %s\n", BinaryFileName) ;
	DebugPrintf(SV_LOG_INFO,"Offlines the Disks for updating the partition information\n");
	std::map<DiskName_t, DiskName_t>::iterator iterDisk = MapSrcToTgtDiskNames.begin();

	for(; iterDisk != MapSrcToTgtDiskNames.end(); iterDisk++)
	{
		if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(iterDisk->second, false))		//need to make online all the disks once the API call succeed
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Offline the target Disk %s, Proceeding with other disks\n", iterDisk->second.c_str());
		}
	}

	FILE *pFileRead = fopen(BinaryFileName,"rb");
    if(!pFileRead)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", BinaryFileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }

    //Read Number of disks first.
    DLM_HEADER dlmHeader;
	DebugPrintf(SV_LOG_INFO,"Size of DLMHeader : %d\n", sizeof(dlmHeader));
    if(!fread(&(dlmHeader), sizeof(dlmHeader), 1, pFileRead))
    {
        fclose(pFileRead);
        DebugPrintf(SV_LOG_ERROR,"Failed to read header information from binary file-%s\n", BinaryFileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_READ;
    }

	DebugPrintf(SV_LOG_INFO,"Version Major: %d  and  Minor: %d\n", dlmHeader.Version.Major, dlmHeader.Version.Minor);
	DebugPrintf(SV_LOG_INFO,"Binary File Information checksum = %s\n", dlmHeader.MD5Checksum);

	// We had faced issue while parsing the file because there is string present in dlm_prefix struct.
	// due to which structure of size changes depends on compiler.
	// We had faced this issue in case of 64-bit bianry parsing when the file was created using a 32 bit binary
	DLM_PREFIX_U1 dlmPrefix_u1;
	DLM_PREFIX dlmPrefix;
	SV_UINT DiskCount;
	if((1 == dlmHeader.Version.Major) && (2 == dlmHeader.Version.Minor))
	{
		DebugPrintf(SV_LOG_INFO,"Size of dlmPrefix : %d\n", sizeof(dlmPrefix_u1));
		if(!fread(&(dlmPrefix_u1), sizeof(dlmPrefix_u1), 1, pFileRead))
		{
			fclose(pFileRead);
			DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", BinaryFileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_READ;
		}

		DebugPrintf(SV_LOG_INFO,"Identifier : %s\n", dlmPrefix_u1.Identifier);
		DebugPrintf(SV_LOG_INFO,"Number of disks informations are : %d\n", dlmPrefix_u1.numOfStructure);
		DiskCount = dlmPrefix_u1.numOfStructure;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Size of dlmPrefix : %d\n", sizeof(dlmPrefix));
		if(!fread(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileRead))
		{
			fclose(pFileRead);
			DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", BinaryFileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_READ;
		}

		DebugPrintf(SV_LOG_INFO,"Identifier : %s\n", dlmPrefix.strIdentifier.c_str());
		DebugPrintf(SV_LOG_INFO,"Number of disks informations are : %d\n", dlmPrefix.numOfStructure);
		DiskCount = dlmPrefix.numOfStructure;
	}

	if(0 == DiskCount)
	{
		fclose(pFileRead);
		DebugPrintf(SV_LOG_ERROR,"Found no disk information in partition info file: %s , Considering this as an error\n", BinaryFileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return DLM_ERR_FILE_READ;
	}
	//Read all disks info and generate the map.
    for(SV_UINT i = 0; i < DiskCount; i++)
    {
        DebugPrintf(SV_LOG_DEBUG,"--------------------------------------------------------------\n");

		SV_UINT NumOfPartition;
		if((1 == dlmHeader.Version.Major) && (2 == dlmHeader.Version.Minor))
		{
			dlmPrefix_u1.clear();
			DebugPrintf(SV_LOG_INFO,"Size of dlmPrefix : %d\n", sizeof(dlmPrefix_u1));
			if(!fread(&(dlmPrefix_u1), sizeof(dlmPrefix_u1), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", BinaryFileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}
			DebugPrintf(SV_LOG_INFO,"Identifier : %s\n", dlmPrefix_u1.Identifier);
			DebugPrintf(SV_LOG_INFO,"Number of Partitons information are : %d\n", dlmPrefix_u1.numOfStructure);
			NumOfPartition = dlmPrefix_u1.numOfStructure;
		}
		else
		{
			dlmPrefix.clear();
			DebugPrintf(SV_LOG_INFO,"Size of dlmPrefix : %d\n", sizeof(dlmPrefix));
			if(!fread(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", BinaryFileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}
			DebugPrintf(SV_LOG_INFO,"Identifier : %s\n", dlmPrefix.strIdentifier.c_str());
			DebugPrintf(SV_LOG_INFO,"Number of Partitons information are : %d\n", dlmPrefix.numOfStructure);
			NumOfPartition = dlmPrefix.numOfStructure;
		}

		for(SV_UINT j = 0; j < NumOfPartition; j++)
		{
			DLM_PARTITION_INFO_SUB PartitionSub;
			DebugPrintf(SV_LOG_INFO,"Size of PartitionSub : %d\n", sizeof(PartitionSub));
			if(!fread(&(PartitionSub), sizeof(PartitionSub), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read DISK_INFO_SUB data of a disk from binary file-%s\n", BinaryFileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}

			DebugPrintf(SV_LOG_INFO,"Disk Name                   = %s\n", PartitionSub.DiskName);
			DebugPrintf(SV_LOG_INFO,"Partition Name              = %s\n", PartitionSub.PartitionName);
			DebugPrintf(SV_LOG_INFO,"Partition Number            = %d\n", PartitionSub.PartitionNum);
			DebugPrintf(SV_LOG_INFO,"Partition Type              = %s\n", PartitionSub.PartitionType);
			DebugPrintf(SV_LOG_INFO,"Partition Length            = %lld\n", PartitionSub.PartitionLength);
			DebugPrintf(SV_LOG_INFO,"Partition Starting offset   = %lld\n", PartitionSub.StartingOffset);

			SV_UCHAR * PartitionSector = new SV_UCHAR[size_t(PartitionSub.PartitionLength)];
			if(NULL == PartitionSector)
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for partition sector sector of disk %s\n", PartitionSub.DiskName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
			}
            if(!fread(PartitionSector, sizeof(SV_UCHAR)*(size_t(PartitionSub.PartitionLength)), 1, pFileRead))
            {
                fclose(pFileRead);
				delete [] PartitionSector;
                DebugPrintf(SV_LOG_ERROR,"Failed to read Partition data of disk-%s from binary file-%s\n", PartitionSub.DiskName, BinaryFileName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
            }
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully read Partition data of disk-%s from binary file-%s\n", PartitionSub.DiskName, BinaryFileName);
				iterDisk = MapSrcToTgtDiskNames.find(std::string(PartitionSub.DiskName));
				if(iterDisk != MapSrcToTgtDiskNames.end())
				{
					if(!WriteToDisk(iterDisk->second.c_str(), PartitionSub.StartingOffset, size_t(PartitionSub.PartitionLength), PartitionSector))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to write partition information on Target disk : %s\n", iterDisk->second.c_str());
						delete [] PartitionSector;
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return DLM_ERR_DISK_WRITE;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Successfully updated partition information on Target disk : %s\n", iterDisk->second.c_str());
						delete [] PartitionSector;
						RestoredSrcDiskNames.insert(PartitionSub.DiskName);
					}
				}
			}
		}
	}
    fclose(pFileRead);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}


//*************************************************************************************************
// Restore the source disks structure onto the given target disks.
// Input is given as map of source and target disk names
// Return the disk names of source which are successfully restored.
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::RestorePartitions(const char * TgtDiskName, DlmPartitionInfoVec_t& vecPartitions)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

	DlmPartitionInfoIterVec_t iterPartition = vecPartitions.begin();
	for(; iterPartition != vecPartitions.end(); iterPartition++)
	{
		if(!WriteToDisk(TgtDiskName, iterPartition->PartitionInfoSub.StartingOffset, size_t(iterPartition->PartitionInfoSub.PartitionLength), iterPartition->PartitionSector))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write partition information on Target disk(%s).\n", TgtDiskName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_DISK_WRITE;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

#ifdef WIN32
//***************************************************************************************
// Makes changes for W2K8 EFI partition, so that machine will boot up properly at target
// Here input is the partition file and returns 0 on success.
//***************************************************************************************
DLM_ERROR_CODE DLRestore::RestoreW2K8Efi(const char * BinaryFileName,
										 std::map<std::string, std::string>& MapSrcToTgtDiskNames,
										 std::set<DiskName_t>& RestoredSrcDiskNames)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE retVal = DLM_ERR_SUCCESS;

	DlmPartitionInfoSubMap_t mapDiskToPartition;
	retVal = ReadPartitionSubInfoFromFile(BinaryFileName, mapDiskToPartition);
	if(DLM_ERR_SUCCESS == retVal)
	{
		DebugPrintf(SV_LOG_INFO,"Successfully read the Partition information from file : %s\n", BinaryFileName);

		std::set<DiskName_t>::iterator iter;
		for(iter = RestoredSrcDiskNames.begin(); iter != RestoredSrcDiskNames.end(); iter++)
		{
			DlmPartitionInfoSubIterMap_t iterMap = mapDiskToPartition.find(*iter);
			std::map<std::string, std::string>::iterator iterDisk = MapSrcToTgtDiskNames.find(*iter);
			if(!ProcessW2K8EfiPartition(iterDisk->second, iterMap->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to perform the W2K8 related EFI changes.\n");
				retVal = DLM_ERR_FAILURE;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Successfully performed the W2K8 related EFI changes.\n");
			}
		}
	}
	else
		DebugPrintf(SV_LOG_INFO,"Failed to read the Partition information from file : %s, DLM error code : %d\n", BinaryFileName, retVal);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return retVal;
}


//***************************************************************************************
// Makes changes for W2K8 EFI partition, so that machine will boot up properly at target
// Here input is the target disk and dlm info related to it.
// Logic implemented for it is:

//	1. Converts the EFI partition to normal partiton.
//	2. Mounts the partition to a mount point.
//	3. Aceesing the mount point doing the below changes in it.
//		  a. Creating directory \\EFI\\Boot, if not available.
//		  b. Copying the file \\EFI\\Microsoft\\Boot\\bootmgfw.EFI   to \\EFI\\Boot and renaming that file to \\bootx64.efi
//	4. Deleting the mount point.
//	5. Reverting the partition from normal to EFI.

// return true on success
//***************************************************************************************
bool DLRestore::ProcessW2K8EfiPartition(const std::string& strTgtDisk, DlmPartitionInfoSubVec_t & vecDp)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRet = true;

	std::map<std::string,std::string> mapOfPrevGuidsWithLogicalNames;
	std::map<std::string,std::string> mapOfCurGuidsWithLogicalNames;
    std::list<std::string> erraticVolumeGuids;
	std::map<std::string,std::string>::iterator iter;
	std::list<std::string> lstNewVolumes;
	std::string strEfiVol, strEfiMountPoint;
	DLDiscover obj;
	bool bEfiChange = false;

	do
	{
		DebugPrintf(SV_LOG_INFO,"Finding out all the volumes at Target\n");
		obj.GetVolumesPresentInSystem(mapOfPrevGuidsWithLogicalNames, erraticVolumeGuids);

		iter = mapOfPrevGuidsWithLogicalNames.begin();
		DebugPrintf(SV_LOG_INFO,"Initially discovered Volumes are: \n");
		for(;iter != mapOfPrevGuidsWithLogicalNames.end(); iter++)
		{
			DebugPrintf(SV_LOG_INFO,"\t %s \n", iter->first.c_str());
		}

		if(DLM_ERR_SUCCESS != ConvertEfiToNormalPartition(strTgtDisk, vecDp))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to convert EFI to Normal partition\n");
			bRet = false;
			break;
		}

		Sleep(5000); //5sec sleep because disk rescan happened
		bEfiChange = true;

		DebugPrintf(SV_LOG_INFO,"Finding out all the volumes at Target after EFI to Normal partition convertion\n");
		obj.GetVolumesPresentInSystem(mapOfCurGuidsWithLogicalNames, erraticVolumeGuids);

		iter = mapOfCurGuidsWithLogicalNames.begin();
		DebugPrintf(SV_LOG_INFO,"Volumes Discovered after EFI change: \n");
		for(;iter != mapOfCurGuidsWithLogicalNames.end(); iter++)
		{
			DebugPrintf(SV_LOG_INFO,"\t %s \n", iter->first.c_str());
		}

		NewlyGeneratedVolume(mapOfPrevGuidsWithLogicalNames, mapOfCurGuidsWithLogicalNames, lstNewVolumes);

		DebugPrintf(SV_LOG_INFO,"Finding out the EFI volume\n");
		GetEfiVolume(lstNewVolumes, vecDp, strEfiVol);

		DebugPrintf(SV_LOG_INFO,"Mounting the EFI volume\n");
		if( !obj.MountEfiVolume(strEfiVol, strEfiMountPoint))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to mount the EFI partition %s\n", strEfiVol.c_str());
			bRet = false;
			break;
		}

		if(!UpdateW2K8EfiChanges(strEfiMountPoint))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to perform the EFI related changes for W2K8 server\n");
			bRet = false;
		}

		obj.RemoveAndDeleteMountPoints(strEfiMountPoint);

	}while(0);

	if(bEfiChange)
	{
		if(DLM_ERR_SUCCESS != ConvertNormalToEfiPartition(strTgtDisk, vecDp))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to convert Normal To Efi partition\n");
			bRet = false;
		}
		Sleep(5000); //5sec sleep because disk rescan happened
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRet;
}


void DLRestore::NewlyGeneratedVolume(std::map<std::string,std::string>& mapPrevVolumes, std::map<std::string,std::string>& mapCurVolumes, std::list<std::string>& lstNewVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	std::map<std::string,std::string>::iterator iterCur = mapCurVolumes.begin();
	std::map<std::string,std::string>::iterator iterPrev;
	for(;iterCur != mapCurVolumes.end(); iterCur++)
	{
		iterPrev = mapPrevVolumes.find(iterCur->first);
		if(iterPrev == mapPrevVolumes.end())
		{
			DebugPrintf(SV_LOG_INFO,"Newly created volume after converting EFI to normal partition %s\n", iterCur->first.c_str());
			lstNewVolume.push_back(iterCur->first);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void DLRestore::GetEfiVolume(std::list<std::string>& lstVolumes, DlmPartitionInfoSubVec_t & vecDp, std::string& strEfiVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	SV_ULONGLONG PartitionLength;
	DlmPartitionInfoSubIterVec_t iterVec = vecDp.begin();
	for(; iterVec != vecDp.end(); iterVec++)
	{
		if(0 == strcmp(iterVec->PartitionType, "{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}"))
		{
			PartitionLength = (SV_ULONGLONG)iterVec->PartitionLength;
			DebugPrintf(SV_LOG_INFO,"EFI partition length : %lld \n",PartitionLength);
			break;
		}
	}

	std::list<std::string>::iterator iter = lstVolumes.begin();
	for(; iter != lstVolumes.end(); iter++)
	{
		strEfiVol = *iter;
		SV_ULONGLONG lnVolSize;
		DLDiscover objDl;
		if(objDl.GetSizeOfVolume(*iter, lnVolSize))
		{
			DebugPrintf(SV_LOG_INFO,"Volume size : %lld \n",lnVolSize);
			if((PartitionLength - lnVolSize) <= 5242880)  //checking for approximate size..5MB here taken
			{
				DebugPrintf(SV_LOG_INFO,"Got EFI volume : %s\n", strEfiVol.c_str());
				break;
			}
		}
		else
		{
			continue;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


bool DLRestore::UpdateW2K8EfiChanges(std::string & strEfiMountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	std::string w2k8BootEfi = strEfiMountPoint + std::string("EFI\\Boot") ;
	if(!CreateDirectory(w2k8BootEfi.c_str(),NULL))
	{
		if(GetLastError() != ERROR_ALREADY_EXISTS)// To Support Rerun scenario
		{
			DebugPrintf(SV_LOG_INFO,"Failed to create the directory : %s. ErrorCode : [%lu].\n", w2k8BootEfi.c_str(),GetLastError());
			bRet = false;
		}
		else
			DebugPrintf(SV_LOG_INFO,"Directory : %s already exists\n", w2k8BootEfi.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully created Directory : %s\n", w2k8BootEfi.c_str());
	}

	if(bRet == true)
	{
		w2k8BootEfi = w2k8BootEfi + std::string("\\bootx64.efi");
		std::string w2k8SrcBootPath = strEfiMountPoint + std::string("EFI\\Microsoft\\Boot\\bootmgfw.EFI");
		if(!CopyFile(w2k8SrcBootPath.c_str(),w2k8BootEfi.c_str(),false))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to copy the file %s to %s. GetLastError - %lu\n", w2k8SrcBootPath.c_str(), w2k8BootEfi.c_str(), GetLastError());
            bRet = false;
		}
		else
            DebugPrintf(SV_LOG_INFO, "Copied the w2k8 efi related boot files.\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
#else
DLM_ERROR_CODE DLRestore::RestoreW2K8Efi(const char * BinaryFileName,
										 std::map<std::string, std::string>& MapSrcToTgtDiskNames,
										 std::set<DiskName_t>& RestoredSrcDiskNames)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}
#endif


//*************************************************************************************************
// Gets the list of corrupted disks by considering only the selected disks.
// Returns the filtered corrupted disks
//*************************************************************************************************
DLM_ERROR_CODE DLRestore::GetFilteredCorruptedDisks (std::vector<DiskName_t>& SelectedSrcDiskNames,
										std::vector<DiskName_t> & CorruptedSrcDiskNames
										)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	std::vector<DiskName_t> temp;
	std::vector<DiskName_t>::iterator srcIter = SelectedSrcDiskNames.begin();
	std::vector<DiskName_t>::iterator crptIter;

	DebugPrintf(SV_LOG_INFO,"Corrupted disks after Filtering : \n");
	for(;srcIter != SelectedSrcDiskNames.end(); srcIter++)
	{
		for(crptIter = CorruptedSrcDiskNames.begin(); crptIter != CorruptedSrcDiskNames.end(); crptIter++)
		{
			if(srcIter->compare(*crptIter) == 0)
			{
				DebugPrintf(SV_LOG_INFO,"Corrupted disks after Filtering : %s\n", crptIter->c_str());
				temp.push_back(*crptIter);
			}
		}
	}

	CorruptedSrcDiskNames.clear();
	CorruptedSrcDiskNames = temp;

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}
