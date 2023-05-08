#include "dldiscover.h"

//*************************************************************************************************
// This function collects all disks info and writes them to a file in binary format
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::StoreDisksInfo(const std::string& binaryFile, std::list<std::string>& outFileNames, std::list<std::string>& erraticVolumeGuids, const std::string& dlmFileFlag)
{    
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);     
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    //1. Collect the DISK_INFO structure from the machine.
    DisksInfoMap_t srcDisksInfoMap;
	DlmPartitionInfoSubMap_t srcPartitionInfoMap;
	bool bPartitionExist = false;
    RetVal = GetDiskInfoMapFromSystem(srcDisksInfoMap, srcPartitionInfoMap, erraticVolumeGuids, bPartitionExist, binaryFile, dlmFileFlag);

    if(DLM_ERR_SUCCESS != RetVal)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to collect the disks info from the system. Error Code - %u\n", RetVal);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);     
        return RetVal;
    }

#ifdef WIN32
	bool bCreateMbrFile = false;
	bool bCreatePartitionFile = false;
	std::string dlmMbrCheckSum = "";
	unsigned char curCheckSum[16] = "";
	std::list<std::string> prevDlmFiles;

	if(DLM_MBR_FILE_ONLY == dlmFileFlag)
	{
		RetVal = WriteDiskInfoMapToFile(binaryFile.c_str(), srcDisksInfoMap);
		if(DLM_ERR_SUCCESS != RetVal)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write the disks info to the file(%s). Error Code - %u\n", binaryFile.c_str(), RetVal);
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully written the collected disks info to the file(%s).\n", binaryFile.c_str());
			
			if(DLM_ERR_SUCCESS != DlmMd5Checksum(srcDisksInfoMap, curCheckSum))
			{
				//need to write the necessary actions in case of failure
				DebugPrintf(SV_LOG_INFO,"New checksum calculation failed.\n");
				RetVal = DLM_ERR_FAILURE;
			}
			else
			{
				dlmMbrCheckSum = GetBytesInArrayAsHexString(curCheckSum, 16);

				if(false == WriteDlmMbrInfoInReg(binaryFile, dlmMbrCheckSum))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to update the registry for dlm mbr info.\n");
					RetVal = DLM_ERR_FAILURE;
				}
				else
				{
					outFileNames.push_back(binaryFile);
					RetVal = DLM_FILE_CREATED;
				}
			}
		}
	}
	else if(DLM_FILE_ALL == dlmFileFlag)
	{
		if(bPartitionExist)
		{
			std::string binaryFileName = binaryFile + DLM_DISK_PARTITION_FILE;
			RetVal = WritePartitionInfoToFile(binaryFileName.c_str(), srcPartitionInfoMap);
			if(DLM_ERR_SUCCESS != RetVal)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to write the partitions info to the file(%s). Error Code - %u\n", binaryFileName.c_str(), RetVal);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully written the collected partitions info to the file(%s).\n", binaryFileName.c_str());
				if(true == WritePartitionFileNameInReg(binaryFileName))
				{
					DebugPrintf(SV_LOG_INFO,"Successfully updated the registry for partition file name.\n");
					outFileNames.push_back(binaryFileName);
					RetVal = DLM_FILE_CREATED;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to update the registry for partition file name.\n");
					RetVal = DLM_ERR_FAILURE;
				}
			}
		}

		if((DLM_ERR_SUCCESS == RetVal) || (DLM_FILE_CREATED == RetVal))
		{
			RetVal = WriteDiskInfoMapToFile(binaryFile.c_str(), srcDisksInfoMap);
			if(DLM_ERR_SUCCESS != RetVal)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to write the disks info to the file(%s). Error Code - %u\n", binaryFile.c_str(), RetVal);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully written the collected disks info to the file(%s).\n", binaryFile.c_str());
				if(DLM_ERR_SUCCESS != DlmMd5Checksum(srcDisksInfoMap, curCheckSum))
				{
					//need to write the necessary actions in case of failure
					DebugPrintf(SV_LOG_INFO,"New checksum calculation failed.\n");
					RetVal = DLM_ERR_FAILURE;
				}
				else
				{
					dlmMbrCheckSum = GetBytesInArrayAsHexString(curCheckSum, 16);
					if(false == WriteDlmMbrInfoInReg(binaryFile, dlmMbrCheckSum))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to update the registry for dlm mbr info.\n");
						RetVal = DLM_ERR_FAILURE;
					}
					else
					{
						outFileNames.push_back(binaryFile);
						RetVal = DLM_FILE_CREATED;
					}
				}
			}
		}
	}
	else
	{
		//2. Calculate checksum of the discovered disks information
		if(IsDlmInfoChanged(srcDisksInfoMap, bCreateMbrFile, bCreatePartitionFile, dlmMbrCheckSum, prevDlmFiles))
		{
			if(bCreateMbrFile)
			{
				if(bPartitionExist && bCreatePartitionFile)
				{
					std::string binaryFileName = binaryFile + DLM_DISK_PARTITION_FILE;
					RetVal = WritePartitionInfoToFile(binaryFileName.c_str(), srcPartitionInfoMap);
					if(DLM_ERR_SUCCESS != RetVal)
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to write the partitions info to the file(%s). Error Code - %u\n", binaryFileName.c_str(), RetVal);
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Successfully written the collected partitions info to the file(%s).\n", binaryFileName.c_str());
						if(true == WritePartitionFileNameInReg(binaryFileName))
						{
							DebugPrintf(SV_LOG_INFO,"Successfully updated the registry for partition file name.\n");
							outFileNames.push_back(binaryFileName);
							RetVal = DLM_FILE_CREATED;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to update the registry for partition file name.\n");
							RetVal = DLM_ERR_FAILURE;
						}
					}
				}

				if((DLM_ERR_SUCCESS == RetVal) || (DLM_FILE_CREATED == RetVal))
				{
					//3. Write this structure into a file
					RetVal = WriteDiskInfoMapToFile(binaryFile.c_str(), srcDisksInfoMap);
					if(DLM_ERR_SUCCESS != RetVal)
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to write the disks info to the file(%s). Error Code - %u\n", binaryFile.c_str(), RetVal);
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Successfully written the collected disks info to the file(%s).\n", binaryFile.c_str());
						if(false == WriteDlmMbrInfoInReg(binaryFile, dlmMbrCheckSum))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to update the registry for dlm mbr info.\n");
							RetVal = DLM_ERR_FAILURE;
						}
						else
						{
							std::string PartitionFileName;
							if((!bCreatePartitionFile) && GetPartitionFileNameFromReg(PartitionFileName))
							{
								outFileNames.push_back(PartitionFileName);
							}
							outFileNames.push_back(binaryFile);
							RetVal = DLM_FILE_CREATED;
						}
					}
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"There is no difference in current to the earlier file\n");
				RetVal = DLM_FILE_NOT_CREATED;
				std::list<std::string>::iterator it = prevDlmFiles.begin();
				for(; it != prevDlmFiles.end(); it++)
					outFileNames.push_back(*it);
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"There is some problem in finding out the checksum\n");
			RetVal = DLM_ERR_FAILURE;
		}
	}

#else
	//2. Write this structure into a file
	RetVal = WriteDiskInfoMapToFile(binaryFile.c_str(), srcDisksInfoMap);
	if(DLM_ERR_SUCCESS != RetVal)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to write the disks info to the file(%s). Error Code - %u\n", binaryFile.c_str(), RetVal);    
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);     
		return RetVal;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully written the collected disks info to the file(%s).\n", binaryFile.c_str());
		outFileNames.push_back(binaryFile);
	}
#endif

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);     
    return RetVal;
}


//*************************************************************************************************
// This functions collects disk info map from BLOB file.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::GetDiskInfoMapFromBlob(const char * FileName, DisksInfoMap_t & DisksInfoMapFromBlob)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);     
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);     
    return ReadDiskInfoMapFromFile(FileName, DisksInfoMapFromBlob);
}

//*************************************************************************************************
// This function writes all disks info to a file in binary format
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::WriteDiskInfoMapToFile(const char * FileName, DisksInfoMap_t & d)
{   
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_DEBUG,"Writing file - %s.\n", FileName);

    //open a file in binary mode
    FILE *pFileWrite = fopen(FileName,"wb");
    if(!pFileWrite)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }          
    
	DLM_VERSION_INFO dlmVersion;
	dlmVersion.Major = 1;
	dlmVersion.Minor = 2;

	if(! fwrite(&(dlmVersion), sizeof(dlmVersion), 1, pFileWrite))
    {
        fclose(pFileWrite);
        DebugPrintf(SV_LOG_ERROR,"Failed to write DiskCount data to binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_WRITE;
    }

    //Write Number of disks first.
    SV_UINT DiskCount = d.size();
    if(! fwrite(&(DiskCount), sizeof(DiskCount), 1, pFileWrite))
    {
        fclose(pFileWrite);
        DebugPrintf(SV_LOG_ERROR,"Failed to write DiskCount data to binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_WRITE;
    }

    //Write all disks info one after one.
    for(DisksInfoMapIter_t iter = d.begin(); iter != d.end(); iter++)
    {
        //iter->first is disk name , iter->second is DISK_INFO structure
        if(! fwrite(&(iter->second.DiskInfoSub), sizeof(iter->second.DiskInfoSub), 1, pFileWrite))
        {
            fclose(pFileWrite);
            DebugPrintf(SV_LOG_ERROR,"Failed to write DISK_INFO_SUB data of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return DLM_ERR_FILE_WRITE;
        }

		if(! fwrite(&(iter->second.DiskDeviceId), sizeof(iter->second.DiskDeviceId), 1, pFileWrite))
        {
            fclose(pFileWrite);
            DebugPrintf(SV_LOG_ERROR,"Failed to write Disk device id of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return DLM_ERR_FILE_WRITE;
        }
		if(! fwrite(&(iter->second.DiskSignature), sizeof(iter->second.DiskSignature), 1, pFileWrite))
        {
            fclose(pFileWrite);
            DebugPrintf(SV_LOG_ERROR,"Failed to write Disk Signature of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return DLM_ERR_FILE_WRITE;
        }

        //discard remaining info if raw disk and go for next disk
        if(RAWDISK != iter->second.DiskInfoSub.FormatType) 
        {
            if(0 != iter->second.DiskInfoSub.VolumeCount)
            {
                //write vol info of all  volumes
                VolumesInfoVecIter_t iterVec = iter->second.VolumesInfo.begin();
                for( ; iterVec != iter->second.VolumesInfo.end(); iterVec++)
                {
                    if(! fwrite(&(*iterVec), sizeof(*iterVec), 1, pFileWrite))
                    {
                        fclose(pFileWrite);
                        DebugPrintf(SV_LOG_ERROR,"Failed to write VOLUME_INFO data of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);
                        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                        return DLM_ERR_FILE_WRITE;
                    }
                }
            }

			SV_UINT PartitionFileCount = iter->second.vecPartitionFilePath.size();
			DebugPrintf(SV_LOG_DEBUG,"Number of Partition files = %d\n",PartitionFileCount);
			if(! fwrite(&(PartitionFileCount), sizeof(PartitionFileCount), 1, pFileWrite))
			{
				fclose(pFileWrite);
				DebugPrintf(SV_LOG_ERROR,"Failed to write the number of aprtiton files count to binary file-%s\n", FileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_WRITE;
			}

			for(std::vector<PARTITION_FILE>::iterator iterVecP = iter->second.vecPartitionFilePath.begin(); iterVecP != iter->second.vecPartitionFilePath.end(); iterVecP++)
			{
				DebugPrintf(SV_LOG_DEBUG,"Size of PARTITON_FILE = %d\n",sizeof(iterVecP->Name));                        
				if(! fwrite(&(iterVecP->Name), sizeof(iterVecP->Name), 1, pFileWrite))
                {
                    fclose(pFileWrite);
                    DebugPrintf(SV_LOG_ERROR,"Failed to write partiton file name %s to binary file-%s\n", iterVecP->Name, FileName);                           
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_WRITE;
                }
			}

			if((MBR == iter->second.DiskInfoSub.FormatType) && (BASIC == iter->second.DiskInfoSub.Type))
            {
                if(! fwrite(&(iter->second.MbrSector), sizeof(iter->second.MbrSector), 1, pFileWrite))
                {
                    fclose(pFileWrite);
                    DebugPrintf(SV_LOG_ERROR,"Failed to write MBR data of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_WRITE;
                }
                if(0 != iter->second.DiskInfoSub.EbrCount)
                {
                    //write all EBRs
                    std::vector<EBR_SECTOR>::iterator iterVec = iter->second.EbrSectors.begin();
                    DebugPrintf(SV_LOG_DEBUG,"Number of EbrSectors = %d\n",(int)iter->second.EbrSectors.size());
                    for( ; iterVec != iter->second.EbrSectors.end(); iterVec++)
                    {                    
                        DebugPrintf(SV_LOG_DEBUG,"Size of EbrSector(iterVec->EbrSector) = %d\n",sizeof(iterVec->EbrSector));                        
                        if(! fwrite(&(iterVec->EbrSector), sizeof(iterVec->EbrSector), 1, pFileWrite))
                        {
                            fclose(pFileWrite);
                            DebugPrintf(SV_LOG_ERROR,"Failed to write EBR data of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);                           
                            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                            return DLM_ERR_FILE_WRITE;
                        }
                    }
                }
            }
            else if((GPT == iter->second.DiskInfoSub.FormatType) && (BASIC == iter->second.DiskInfoSub.Type))
            {
                if(! fwrite(&(iter->second.GptSector), sizeof(iter->second.GptSector), 1, pFileWrite))
                {
                    fclose(pFileWrite);
                    DebugPrintf(SV_LOG_ERROR,"Failed to write GPT data of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_WRITE;
                }
            }
			else if((MBR == iter->second.DiskInfoSub.FormatType) && (DYNAMIC == iter->second.DiskInfoSub.Type))
			{
				if(! fwrite(iter->second.MbrDynamicSector, sizeof(SV_UCHAR)*MBR_DYNAMIC_BOOT_SECTOR_LENGTH, 1, pFileWrite))
                {
                    fclose(pFileWrite);
                    DebugPrintf(SV_LOG_ERROR,"Failed to write MBR data of dynamic disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_WRITE;
                }
				if(! fwrite(iter->second.LdmDynamicSector, sizeof(SV_UCHAR)*LDM_DYNAMIC_BOOT_SECTOR_LENGTH, 1, pFileWrite))
                {
                    fclose(pFileWrite);
                    DebugPrintf(SV_LOG_ERROR,"Failed to write LDM data of dynamic disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_WRITE;
                }
			}
			else if((GPT == iter->second.DiskInfoSub.FormatType) && (DYNAMIC == iter->second.DiskInfoSub.Type))
			{
				if(! fwrite(iter->second.GptDynamicSector, sizeof(SV_UCHAR)*GPT_DYNAMIC_BOOT_SECTOR_LENGTH, 1, pFileWrite))
                {
                    fclose(pFileWrite);
                    DebugPrintf(SV_LOG_ERROR,"Failed to write GPT data of dynamic disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_WRITE;
                }
			}
        }
		fflush(pFileWrite);
    }
    fclose(pFileWrite);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

//*************************************************************************************************
// This function read all disks info to a file in binary format 
// and generate map of disknames to diskinfo
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::ReadDiskInfoMapFromFile(const char * FileName, DisksInfoMap_t & d)
{   
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);     
    d.clear();
	bool bOldFile = false;
	double version = 0;

	DebugPrintf(SV_LOG_DEBUG,"Reading file - %s.\n", FileName);

    //open the file in binary mode, read mode
    FILE *pFileRead = fopen(FileName,"rb");
    if(!pFileRead)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }

	//First read version information
	DLM_VERSION_INFO dlmVersion;
	if(! fread(&(dlmVersion), sizeof(dlmVersion), 1, pFileRead))
    {
        fclose(pFileRead);
        DebugPrintf(SV_LOG_INFO,"Failed to read version information from binary file-%s\n", FileName);
		DebugPrintf(SV_LOG_INFO,"Considering this as old structured dlm mbr file, Reopening the file to read again\n", FileName);
		bOldFile = true;

		pFileRead = fopen(FileName,"rb");
		if(!pFileRead)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_OPEN;
		}
    }
	DebugPrintf(SV_LOG_DEBUG,"DLM Version Major:Minor - %d:%d\n", dlmVersion.Major, dlmVersion.Minor);
	if(1 == dlmVersion.Major && 1 == dlmVersion.Minor)
	{
		bOldFile = false;
		version = 1.1;
	}
	else if(1 == dlmVersion.Major && 2 == dlmVersion.Minor)
	{
		bOldFile = false;
		version = 1.2;
	}
	else
	{
		fclose(pFileRead);
        DebugPrintf(SV_LOG_INFO,"Read old version information from binary file-%s\n", FileName);
		DebugPrintf(SV_LOG_INFO,"Considering this as old structured dlm mbr file, Reopening the file to read again\n", FileName);
		bOldFile = true;

		pFileRead = fopen(FileName,"rb");
		if(!pFileRead)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_OPEN;
		}
	}

    //Read Number of disks first.
    SV_UINT DiskCount = 0;
    if(! fread(&(DiskCount), sizeof(DiskCount), 1, pFileRead))
    {
        fclose(pFileRead);
        DebugPrintf(SV_LOG_ERROR,"Failed to read DiskCount data from binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_READ;
    }

   // printf("Disk Count = %u\n", DiskCount);
    DebugPrintf( SV_LOG_DEBUG, "Disk Count: %u\n", DiskCount ) ;

    //Read all disks info and generate the map.
    for(SV_UINT i = 0; i < DiskCount; i++)
    {
        DebugPrintf(SV_LOG_DEBUG,"--------------------------------------------------------------\n");
        DISK_INFO DiObj;
        DISK_INFO_SUB DiSubObj;
        
        if(! fread(&(DiSubObj), sizeof(DiSubObj), 1, pFileRead))
        {
            fclose(pFileRead);
            DebugPrintf(SV_LOG_ERROR,"Failed to read DISK_INFO_SUB data of a disk from binary file-%s\n", FileName);
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return DLM_ERR_FILE_READ;
        }       
        DiObj.DiskInfoSub = DiSubObj; //Add DiskInfoSub to DISK_INFO obj
		
		if(1.2 == version)
		{
			char DiskDeviceId[MAX_PATH_COMMON+1];
            if(! fread(&(DiskDeviceId), sizeof(DiskDeviceId), 1, pFileRead))
            {
                fclose(pFileRead);
                DebugPrintf(SV_LOG_ERROR,"Failed to read disksignature of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
            }
            //Add MbrSector to DISK_INFO obj
            for(int i=0; i<=MAX_PATH_COMMON; i++)
            {
				DiObj.DiskDeviceId[i] = DiskDeviceId[i];
            }

			char DiskSignature[MAX_DISK_SIGNATURE_LENGTH+1];
            if(! fread(&(DiskSignature), sizeof(DiskSignature), 1, pFileRead))
            {
                fclose(pFileRead);
                DebugPrintf(SV_LOG_ERROR,"Failed to read disksignature of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
            }
            //Add MbrSector to DISK_INFO obj
            for(int i=0; i<=MAX_DISK_SIGNATURE_LENGTH; i++)
            {
				DiObj.DiskSignature[i] = DiskSignature[i];
            }
		}

        DebugPrintf(SV_LOG_DEBUG,"Name            = %s\n", DiSubObj.Name);
		DebugPrintf(SV_LOG_DEBUG,"Disk Signature  = %s\n", DiObj.DiskSignature);
		DebugPrintf(SV_LOG_DEBUG,"Disk Device id  = %s\n", DiObj.DiskDeviceId);
        DebugPrintf(SV_LOG_DEBUG,"BytesPerSector  = %llu\n", DiSubObj.BytesPerSector);
        DebugPrintf(SV_LOG_DEBUG,"EbrCount        = %llu\n", DiSubObj.EbrCount);
        DebugPrintf(SV_LOG_DEBUG,"Flag            = %llu\n", DiSubObj.Flag);
        DebugPrintf(SV_LOG_DEBUG,"FormatType      = %u\n", DiSubObj.FormatType);
        DebugPrintf(SV_LOG_DEBUG,"ScsiId          = %u:%u:%u:%u\n", DiSubObj.ScsiId.Host, DiSubObj.ScsiId.Channel, DiSubObj.ScsiId.Target, DiSubObj.ScsiId.Lun );
        DebugPrintf(SV_LOG_DEBUG,"Size            = %lld\n", DiSubObj.Size);
        DebugPrintf(SV_LOG_DEBUG,"Type            = %u\n", DiSubObj.Type);
        DebugPrintf(SV_LOG_DEBUG,"VolumeCount     = %llu\n", DiSubObj.VolumeCount);

        if(RAWDISK != DiSubObj.FormatType)
        {
            if(0 != DiSubObj.VolumeCount)
            {
                //read vol info of all  volumes
                VolumesInfoVec_t VolumesInfo;
                for(SV_ULONGLONG i = 0; i < DiSubObj.VolumeCount; i++)
                {
                    VOLUME_INFO VolumeInfo;
                    if(! fread(&(VolumeInfo), sizeof(VolumeInfo), 1, pFileRead))
                    {
                        fclose(pFileRead);
                        DebugPrintf(SV_LOG_ERROR,"Failed to read VOLUME_INFO data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
                        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                        return DLM_ERR_FILE_READ;
                    }
                    VolumesInfo.push_back(VolumeInfo);
                    DebugPrintf(SV_LOG_DEBUG,"-----------------------------\n");                    
                    DebugPrintf(SV_LOG_DEBUG,"VolumeName      = %s\n",VolumeInfo.VolumeName);
                    DebugPrintf(SV_LOG_DEBUG,"VolumeLength    = %lld\n",VolumeInfo.VolumeLength);
                    DebugPrintf(SV_LOG_DEBUG,"StartingOffset  = %lld\n",VolumeInfo.StartingOffset);
                    DebugPrintf(SV_LOG_DEBUG,"EndingOffset    = %lld\n",VolumeInfo.EndingOffset);
                }
                DiObj.VolumesInfo = VolumesInfo; //Add VolumesInfo to DISK_INFO obj
            }

			if(!bOldFile)
			{
				SV_UINT PartitionFileCount = 0;
				if(! fread(&(PartitionFileCount), sizeof(PartitionFileCount), 1, pFileRead))
				{
					fclose(pFileRead);
					DebugPrintf(SV_LOG_ERROR,"Failed to read PartitionFileCount data from binary file-%s\n", FileName);
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return DLM_ERR_FILE_READ;
				}
				std::vector<PARTITION_FILE> vecPartitionFile;
				for(SV_ULONGLONG i = 0; i < PartitionFileCount; i++)
				{
					PARTITION_FILE PartnFile;
					if(! fread(&(PartnFile.Name), sizeof(PartnFile.Name), 1, pFileRead))
					{
						fclose(pFileRead);
						DebugPrintf(SV_LOG_ERROR,"Failed to read PartitionFileName data from binary file-%s\n", FileName);
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return DLM_ERR_FILE_READ;
					}
					DebugPrintf(SV_LOG_DEBUG,"%ld) PartitionFileName	= %s\n", i, PartnFile.Name);
					vecPartitionFile.push_back(PartnFile);
				}
				DiObj.vecPartitionFilePath = vecPartitionFile;
			}

            if((MBR == DiSubObj.FormatType) && (BASIC == DiSubObj.Type))
            {
                SV_UCHAR MbrSector[MBR_BOOT_SECTOR_LENGTH];
                if(! fread(&(MbrSector), sizeof(MbrSector), 1, pFileRead))
                {
                    fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to read MBR data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
                }
                //Add MbrSector to DISK_INFO obj
                for(int i=0; i<MBR_BOOT_SECTOR_LENGTH; i++)
                {
                    DiObj.MbrSector[i] = MbrSector[i];
                }

                if(0 != DiSubObj.EbrCount)
                {
                    //read all EBRs
                    std::vector<EBR_SECTOR> EbrSectors;                
                    for(SV_ULONGLONG i = 0; i < DiSubObj.EbrCount; i++)
                    {  
                        EBR_SECTOR es;
                        if(! fread(&(es.EbrSector), sizeof(es.EbrSector), 1, pFileRead))
                        {
                            fclose(pFileRead);
                            DebugPrintf(SV_LOG_ERROR,"Failed to read EBR data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
                            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                            return DLM_ERR_FILE_READ;
                        }
                        EbrSectors.push_back(es); 
                    }
                    DiObj.EbrSectors = EbrSectors; //Add EbrSectors to DISK_INFO obj
                }
            }
            else if((GPT == DiSubObj.FormatType) && (BASIC == DiSubObj.Type))
            {
                SV_UCHAR GptSector[GPT_BOOT_SECTOR_LENGTH];
                if(! fread(&(GptSector), sizeof(GptSector), 1, pFileRead))
                {
                    fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to read GPT data of disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
                }
                //Add GptSector to DISK_INFO obj
                for(int i=0; i<GPT_BOOT_SECTOR_LENGTH; i++)
                {
                    DiObj.GptSector[i] = GptSector[i];
                }
            }
			else if((MBR == DiSubObj.FormatType) && (DYNAMIC == DiSubObj.Type))
            {
                SV_UCHAR *MbrSector = new SV_UCHAR[MBR_DYNAMIC_BOOT_SECTOR_LENGTH];
				if(NULL == MbrSector)
				{
					fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for MBR Dynamic sector of disk %s\n", DiSubObj.Name);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
				}
                if(! fread(MbrSector, sizeof(SV_UCHAR)*MBR_DYNAMIC_BOOT_SECTOR_LENGTH, 1, pFileRead))
                {
                    fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to read MBR data of dynamic disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					delete [] MbrSector;
                    return DLM_ERR_FILE_READ;
                }
                //Add GptSector to DISK_INFO obj
                for(int i=0; i<MBR_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
                {
					DiObj.MbrDynamicSector[i] = MbrSector[i];
                }
				delete [] MbrSector;

				SV_UCHAR *LdmSector = new SV_UCHAR[LDM_DYNAMIC_BOOT_SECTOR_LENGTH];
				if(NULL == LdmSector)
				{
					fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for MBR Dynamic sector of disk %s\n", DiSubObj.Name);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
				}
                if(! fread(LdmSector, sizeof(SV_UCHAR)*LDM_DYNAMIC_BOOT_SECTOR_LENGTH, 1, pFileRead))
                {
                    fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to read MBR data of dynamic disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					delete [] LdmSector;
                    return DLM_ERR_FILE_READ;
                }
                //Add GptSector to DISK_INFO obj
                for(int i=0; i<LDM_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
                {
					DiObj.LdmDynamicSector[i] = LdmSector[i];
                }
				delete [] LdmSector;
            }
			else if((GPT == DiSubObj.FormatType) && (DYNAMIC == DiSubObj.Type))
			{
				SV_UCHAR *GptSector = new SV_UCHAR[GPT_DYNAMIC_BOOT_SECTOR_LENGTH];
				if(NULL == GptSector)
				{
					fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for MBR Dynamic sector of disk %s\n", DiSubObj.Name);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    return DLM_ERR_FILE_READ;
				}
                if(! fread(GptSector, sizeof(SV_UCHAR)*GPT_DYNAMIC_BOOT_SECTOR_LENGTH, 1, pFileRead))
                {
                    fclose(pFileRead);
                    DebugPrintf(SV_LOG_ERROR,"Failed to read MBR data of dynamic disk-%s from binary file-%s\n", DiSubObj.Name, FileName);
	                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					delete [] GptSector;
                    return DLM_ERR_FILE_READ;
                }
                //Add GptSector to DISK_INFO obj
                for(int i=0; i<GPT_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
                {
					DiObj.GptDynamicSector[i] = GptSector[i];
                }
				delete [] GptSector;
			}
        }

        //Add the diskinfo obj to map of disknames to diskinfo
        d[std::string(DiSubObj.Name)] = DiObj;
    }
    fclose(pFileRead);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}



//*************************************************************************************************
// This function writes all disks info to a file in binary format
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::DlmMd5Checksum(DisksInfoMap_t & d, unsigned char* curCheckSum)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DLM_ERROR_CODE dlmResult = DLM_ERR_SUCCESS;

	INM_MD5_CTX md5ctx;
	INM_MD5Init(&md5ctx);

	for(DisksInfoMapIter_t iter = d.begin(); iter != d.end(); iter++)
    {
		INM_MD5Update(&md5ctx, (unsigned char*)iter->second.DiskInfoSub.Name, MAX_PATH_COMMON+1);
		INM_MD5Update(&md5ctx, (unsigned char*)&(iter->second.DiskInfoSub.Size), sizeof(iter->second.DiskInfoSub.Size));
		INM_MD5Update(&md5ctx, (unsigned char*)&(iter->second.DiskInfoSub.Type), sizeof(iter->second.DiskInfoSub.Type));
		INM_MD5Update(&md5ctx, (unsigned char*)&(iter->second.DiskInfoSub.FormatType), sizeof(iter->second.DiskInfoSub.FormatType));
		INM_MD5Update(&md5ctx, (unsigned char*)&(iter->second.DiskInfoSub.EbrCount), sizeof(iter->second.DiskInfoSub.EbrCount));
		INM_MD5Update(&md5ctx, (unsigned char*)&(iter->second.DiskInfoSub.VolumeCount), sizeof(iter->second.DiskInfoSub.VolumeCount));

		if(0 != iter->second.DiskInfoSub.VolumeCount)
		{
			VolumesInfoVecIter_t volIter = iter->second.VolumesInfo.begin();
			for(;volIter != iter->second.VolumesInfo.end(); volIter++)
			{
				INM_MD5Update(&md5ctx, (unsigned char*)volIter->VolumeName, MAX_PATH_COMMON+1);
				INM_MD5Update(&md5ctx, (unsigned char*)volIter->VolumeGuid, MAX_PATH_COMMON+1);
				INM_MD5Update(&md5ctx, (unsigned char*)&(volIter->VolumeLength), sizeof(volIter->VolumeLength));
				INM_MD5Update(&md5ctx, (unsigned char*)&(volIter->StartingOffset), sizeof(volIter->StartingOffset));
			}
		}
		if(RAWDISK != iter->second.DiskInfoSub.FormatType)
		{
			const size_t BUFLEN = 4096;
			bool read = false;
			std::streamsize count;

			if(MBR == iter->second.DiskInfoSub.FormatType && BASIC == iter->second.DiskInfoSub.Type)
			{
				INM_MD5Update(&md5ctx, (unsigned char*)iter->second.MbrSector, MBR_BOOT_SECTOR_LENGTH);

				if(0 != iter->second.DiskInfoSub.EbrCount)
				{
					std::vector<EBR_SECTOR>::iterator ebrIter = iter->second.EbrSectors.begin();
					for(; ebrIter != iter->second.EbrSectors.end(); ebrIter++)
					{
						INM_MD5Update(&md5ctx, (unsigned char*)ebrIter->EbrSector, MBR_BOOT_SECTOR_LENGTH);
					}
				}
			}
			else if(GPT == iter->second.DiskInfoSub.FormatType && BASIC == iter->second.DiskInfoSub.Type)
			{
				std::stringstream gptbasic;
				gptbasic << iter->second.GptSector;
				while (!read)
				{
					char * buffer = new char[BUFLEN];
					gptbasic.read(buffer, BUFLEN);
					if (gptbasic.eof())
						read = true;
					count = gptbasic.gcount();
					if (count)
						INM_MD5Update(&md5ctx, (unsigned char*)buffer, count);
					delete[] buffer;
				}
			}
			else if(MBR == iter->second.DiskInfoSub.FormatType && DYNAMIC == iter->second.DiskInfoSub.Type)
			{
				std::stringstream mbrdyn;
				mbrdyn << *(iter->second.MbrDynamicSector);				
				while (!read)
				{
					char * buffer = new char[BUFLEN];
					mbrdyn.read(buffer, BUFLEN);
					if (mbrdyn.eof())
						read = true;
					count = mbrdyn.gcount();
					if (count)
						INM_MD5Update(&md5ctx, (unsigned char*)buffer, count);
					delete[] buffer;
				}

				read = false;
				std::stringstream ldmdyn;
				ldmdyn << *(iter->second.LdmDynamicSector);				
				while (!read)
				{
					char * buffer = new char[BUFLEN];
					ldmdyn.read(buffer, BUFLEN);
					if (ldmdyn.eof())
						read = true;
					count = ldmdyn.gcount();
					if (count)
						INM_MD5Update(&md5ctx, (unsigned char*)buffer, count);
					delete[] buffer;
				}
			}
			else if(GPT == iter->second.DiskInfoSub.FormatType && DYNAMIC == iter->second.DiskInfoSub.Type)
			{
				std::stringstream gptdyn;
				gptdyn << *(iter->second.GptDynamicSector);				
				while (!read)
				{
					char * buffer = new char[BUFLEN];
					gptdyn.read(buffer, BUFLEN);
					if(gptdyn.eof())
						read = true;
					count = gptdyn.gcount();
					if (count)
						INM_MD5Update(&md5ctx, (unsigned char*)buffer, count);
					delete[] buffer;
				}
			}
		}
	}
	INM_MD5Final(curCheckSum, &md5ctx);

	DebugPrintf(SV_LOG_DEBUG,"\nDLM Disk Metadata checksum calculated is : %s\n", GetBytesInArrayAsHexString(curCheckSum, 16).c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return dlmResult;
}


DLM_ERROR_CODE DLDiscover::WritePartitionInfoToFile(const char * FileName, DlmPartitionInfoSubMap_t & dp)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_DEBUG,"Writing file - %s.\n", FileName);

	//open a file in binary mode
    FILE *pFileWrite = fopen(FileName,"wb");
    if(!pFileWrite)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }

	//Writing versions information in file
	DLM_HEADER dlmHeader;
	inm_strcpy_s((char *)dlmHeader.MD5Checksum, ARRAYSIZE(dlmHeader.MD5Checksum), "");
	dlmHeader.Version.Major = 1;
	dlmHeader.Version.Minor = 2;

	if(! fwrite(&(dlmHeader), sizeof(dlmHeader), 1, pFileWrite))
    {
        fclose(pFileWrite);
        DebugPrintf(SV_LOG_ERROR,"Failed to write DiskCount data to binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_WRITE;
    }


	//Writing disk prefix information initially in file
	/*DLM_PREFIX dlmPrefix;
	dlmPrefix.strIdentifier = "Disk";
	dlmPrefix.numOfStructure = dp.size();*/

	DLM_PREFIX_U1 dlmPrefix;
	inm_strcpy_s(dlmPrefix.Identifier, ARRAYSIZE(dlmPrefix.Identifier), "Disk");
	dlmPrefix.numOfStructure = dp.size();

	DebugPrintf(SV_LOG_DEBUG,"Identifier : %s\n", dlmPrefix.Identifier);
	DebugPrintf(SV_LOG_DEBUG,"Number of disks carries partitions information are : %d\n", dlmPrefix.numOfStructure);

	if(! fwrite(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileWrite))
    {
        fclose(pFileWrite);
        DebugPrintf(SV_LOG_ERROR,"Failed to write Disk prefix information to binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_WRITE;
    }

	DlmPartitionInfoSubIterMap_t iter = dp.begin();
	for(; iter != dp.end(); iter++)
	{
		//Writing partition prefix information initially befre writing partiton info into file
		/*DLM_PREFIX dlmPrefix;
		dlmPrefix.strIdentifier = "Partition";*/
		dlmPrefix.clear();
		inm_strcpy_s(dlmPrefix.Identifier, ARRAYSIZE(dlmPrefix.Identifier), "Partition");
		dlmPrefix.numOfStructure = iter->second.size();

		DebugPrintf(SV_LOG_DEBUG,"Identifier : %s\n", dlmPrefix.Identifier);
		DebugPrintf(SV_LOG_DEBUG,"Number of Partitons information are : %d\n", dlmPrefix.numOfStructure);

		if(! fwrite(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileWrite))
		{
			fclose(pFileWrite);
			DebugPrintf(SV_LOG_ERROR,"Failed to write partition prefix information to binary file-%s\n", FileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_WRITE;
		}

		DebugPrintf(SV_LOG_INFO,"Writing partition information into file %s\n",FileName);
		DlmPartitionInfoSubIterVec_t iterVec = iter->second.begin();
		for(; iterVec != iter->second.end(); iterVec++)
		{
			DebugPrintf(SV_LOG_DEBUG,"Disk Name                   = %s\n", iterVec->DiskName);
			DebugPrintf(SV_LOG_DEBUG,"Partition Name              = %s\n", iterVec->PartitionName);
			DebugPrintf(SV_LOG_DEBUG,"Partition Number            = %d\n", iterVec->PartitionNum);
			DebugPrintf(SV_LOG_DEBUG,"Partition Type              = %s\n", iterVec->PartitionType);
			DebugPrintf(SV_LOG_DEBUG,"Partition Length            = %lld\n", iterVec->PartitionLength);
			DebugPrintf(SV_LOG_DEBUG,"Partition Starting offset   = %lld\n", iterVec->StartingOffset);

			if(! fwrite(&(*iterVec), sizeof(*iterVec), 1, pFileWrite))
            {
                fclose(pFileWrite);
                DebugPrintf(SV_LOG_ERROR,"Failed to write Partition info data of disk-%s to binary file-%s\n", iter->first.c_str(), FileName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_WRITE;
            }
			//first reading the partition information from the respective disk
			SV_UCHAR * PartitionSector = new SV_UCHAR[(unsigned int)iterVec->PartitionLength];
			if(NULL != PartitionSector)
			{
				if(false == ReadFromDisk(iterVec->DiskName, iterVec->StartingOffset, size_t(iterVec->PartitionLength), PartitionSector))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to read the data from disk - %s\n",iterVec->DiskName);
					delete[] PartitionSector;
					return DLM_ERR_DISK_READ;
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"Successfully collected the Partiton info of %s for disk %s\n", iterVec->PartitionName, iter->first.c_str());
				}
			}
			else
			{
				fclose(pFileWrite);
				DebugPrintf(SV_LOG_ERROR,"Failed to allocate dynamic memory for - PartitionSector for disk %s\n", iter->first.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_DISK_READ;
			}

			//Now updating the partition information in file
			if(! fwrite(PartitionSector, sizeof(SV_UCHAR)*(size_t(iterVec->PartitionLength)), 1, pFileWrite))
            {
                fclose(pFileWrite);
				delete [] PartitionSector;
                DebugPrintf(SV_LOG_ERROR,"Failed to write partition information dynamic disk-%s to binary file-%s\n", iter->first.c_str(), FileName);	                
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_WRITE;
            }
			delete [] PartitionSector;
		}
	}
	fclose(pFileWrite);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}


//*************************************************************************************************
// This function read all disks info to a file in binary format 
// and generate map of disknames to partitioninfo
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::ReadPartitionInfoFromFile(const char * FileName, DlmPartitionInfoMap_t & dp)
{   
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);     
    dp.clear();

	DebugPrintf(SV_LOG_DEBUG,"Reading file - %s.\n", FileName);

    //open the file in binary mode, read mode
    FILE *pFileRead = fopen(FileName,"rb");
    if(!pFileRead)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }      

    //Read Number of disks first.
    DLM_HEADER dlmHeader;
	DebugPrintf(SV_LOG_INFO,"Size of DLMHeader : %d\n", sizeof(dlmHeader));
    if(! fread(&(dlmHeader), sizeof(dlmHeader), 1, pFileRead))
    {
        fclose(pFileRead);
        DebugPrintf(SV_LOG_ERROR,"Failed to read header information from binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_READ;
    }

	DebugPrintf(SV_LOG_DEBUG,"Version Major: %d  and  Minor: %d\n", dlmHeader.Version.Major, dlmHeader.Version.Minor);
	DebugPrintf(SV_LOG_DEBUG,"Binary File Information checksum = %s\n", dlmHeader.MD5Checksum);

	// We had faced issue while parsing the file because there is string present in dlm_prefix struct.
	// due to which structure of size changes depends on compiler.
	// We had faced this issue in case of 64-bit bianry parsing when the file was created using a 32 bit binary
	DLM_PREFIX_U1 dlmPrefix_u1;
	DLM_PREFIX dlmPrefix;
	SV_UINT DiskCount;

	if((1 == dlmHeader.Version.Major) && (2 == dlmHeader.Version.Minor))
	{
		DebugPrintf(SV_LOG_INFO,"Size of dlmPrefix : %d\n", sizeof(dlmPrefix_u1));
		if(! fread(&(dlmPrefix_u1), sizeof(dlmPrefix_u1), 1, pFileRead))
		{
			fclose(pFileRead);
			DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
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
		if(! fread(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileRead))
		{
			fclose(pFileRead);
			DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_READ;
		}

		DebugPrintf(SV_LOG_DEBUG,"Identifier : %s\n", dlmPrefix.strIdentifier.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Number of disks informations are : %d\n", dlmPrefix.numOfStructure);
		DiskCount = dlmPrefix.numOfStructure;
	}
	if(0 == DiskCount)
	{
		fclose(pFileRead);
		DebugPrintf(SV_LOG_ERROR,"Found no disk information in partition info file: %s , Considering this as an error\n", FileName);
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
			if(! fread(&(dlmPrefix_u1), sizeof(dlmPrefix_u1), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
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
			if(! fread(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}
			DebugPrintf(SV_LOG_DEBUG,"Identifier : %s\n", dlmPrefix.strIdentifier.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Number of Partitons information are : %d\n", dlmPrefix.numOfStructure);
			NumOfPartition = dlmPrefix.numOfStructure;
		}

		DlmPartitionInfoVec_t dpVec;
		std::string DiskName;

		for(SV_UINT j = 0; j < NumOfPartition; j++)
		{
			DLM_PARTITION_INFO dlmPartitionInfo;
			DLM_PARTITION_INFO_SUB PartitionSub;
			DebugPrintf(SV_LOG_INFO,"Size of PartitionSub : %d\n", sizeof(PartitionSub));
			if(! fread(&(PartitionSub), sizeof(PartitionSub), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read DISK_INFO_SUB data of a disk from binary file-%s\n", FileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}

			DebugPrintf(SV_LOG_DEBUG,"Disk Name                   = %s\n", PartitionSub.DiskName);
			DebugPrintf(SV_LOG_DEBUG,"Partition Name              = %s\n", PartitionSub.PartitionName);
			DebugPrintf(SV_LOG_DEBUG,"Partition Number            = %d\n", PartitionSub.PartitionNum);
			DebugPrintf(SV_LOG_DEBUG,"Partition Type              = %s\n", PartitionSub.PartitionType);
			DebugPrintf(SV_LOG_DEBUG,"Partition Length            = %lld\n", PartitionSub.PartitionLength);
			DebugPrintf(SV_LOG_DEBUG,"Partition Starting offset   = %lld\n", PartitionSub.StartingOffset);

			DiskName = std::string(PartitionSub.DiskName);
			dlmPartitionInfo.PartitionInfoSub = PartitionSub;
			dlmPartitionInfo.PartitionSector = new SV_UCHAR[size_t(PartitionSub.PartitionLength)];
			if(NULL == dlmPartitionInfo.PartitionSector)
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for partition sector sector of disk %s\n", PartitionSub.DiskName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
			}
            if(! fread(dlmPartitionInfo.PartitionSector, sizeof(SV_UCHAR)*(size_t(PartitionSub.PartitionLength)), 1, pFileRead))
            {
                fclose(pFileRead);
                DebugPrintf(SV_LOG_ERROR,"Failed to read Partition data of disk-%s from binary file-%s\n", PartitionSub.DiskName, FileName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
            }
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Successfully read Partition data of disk-%s from binary file-%s\n", PartitionSub.DiskName, FileName);
			}
			dpVec.push_back(dlmPartitionInfo);
		}
		dp.insert(make_pair(DiskName, dpVec));
	}
    fclose(pFileRead);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}



//*************************************************************************************************
// This function read all disks info to a file in binary format 
// and generate map of disknames to partitioninfosub
// Returns DLM_ERR_SUCCESS on success.
//*************************************************************************************************
DLM_ERROR_CODE DLDiscover::ReadPartitionSubInfoFromFile(const char * FileName, DlmPartitionInfoSubMap_t & dp)
{   
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);     
    dp.clear();

    //open the file in binary mode, read mode
    FILE *pFileRead = fopen(FileName,"rb");
    if(!pFileRead)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }      

    //Read Number of disks first.
    DLM_HEADER dlmHeader;
    if(! fread(&(dlmHeader), sizeof(dlmHeader), 1, pFileRead))
    {
        fclose(pFileRead);
        DebugPrintf(SV_LOG_ERROR,"Failed to read header information from binary file-%s\n", FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_READ;
    }

	DebugPrintf(SV_LOG_DEBUG,"Version Major: %d  and  Minor: %d\n", dlmHeader.Version.Major, dlmHeader.Version.Minor);
	DebugPrintf(SV_LOG_DEBUG,"Binary File Information checksum = %s\n", dlmHeader.MD5Checksum);

	// We had faced issue while parsing the file because there is string present in dlm_prefix struct.
	// due to which structure of size changes depends on compiler.
	// We had faced this issue in case of 64-bit bianry parsing when the file was created using a 32 bit binary
	DLM_PREFIX_U1 dlmPrefix_u1;
	DLM_PREFIX dlmPrefix;
	SV_UINT DiskCount;
	if((1 == dlmHeader.Version.Major) && (2 == dlmHeader.Version.Minor))
	{
		DebugPrintf(SV_LOG_INFO,"Size of dlmPrefix : %d\n", sizeof(dlmPrefix_u1));
		if(! fread(&(dlmPrefix_u1), sizeof(dlmPrefix_u1), 1, pFileRead))
		{
			fclose(pFileRead);
			DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
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
		if(! fread(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileRead))
		{
			fclose(pFileRead);
			DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return DLM_ERR_FILE_READ;
		}
		DebugPrintf(SV_LOG_DEBUG,"Identifier : %s\n", dlmPrefix.strIdentifier.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Number of disks informations are : %d\n", dlmPrefix.numOfStructure);
		DiskCount = dlmPrefix.numOfStructure;
	}

	if(0 == DiskCount)
	{
		fclose(pFileRead);
		DebugPrintf(SV_LOG_ERROR,"Found no disk information in partition info file: %s , Considering this as an error\n", FileName);
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
			if(! fread(&(dlmPrefix_u1), sizeof(dlmPrefix_u1), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
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
			if(! fread(&(dlmPrefix), sizeof(dlmPrefix), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read the disk prefix information from binary file-%s\n", FileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}
			DebugPrintf(SV_LOG_DEBUG,"Identifier : %s\n", dlmPrefix.strIdentifier.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Number of Partitons information are : %d\n", dlmPrefix.numOfStructure);
			NumOfPartition = dlmPrefix.numOfStructure;
		}

		DlmPartitionInfoSubVec_t dpVec;
		std::string DiskName;

		for(SV_UINT j = 0; j < NumOfPartition; j++)
		{
			DLM_PARTITION_INFO_SUB PartitionSub;
			if(! fread(&(PartitionSub), sizeof(PartitionSub), 1, pFileRead))
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to read DISK_INFO_SUB data of a disk from binary file-%s\n", FileName);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return DLM_ERR_FILE_READ;
			}

			DebugPrintf(SV_LOG_DEBUG,"Disk Name                   = %s\n", PartitionSub.DiskName);
			DebugPrintf(SV_LOG_DEBUG,"Partition Name              = %s\n", PartitionSub.PartitionName);
			DebugPrintf(SV_LOG_DEBUG,"Partition Number            = %d\n", PartitionSub.PartitionNum);
			DebugPrintf(SV_LOG_DEBUG,"Partition Type              = %s\n", PartitionSub.PartitionType);
			DebugPrintf(SV_LOG_DEBUG,"Partition Length            = %lld\n", PartitionSub.PartitionLength);
			DebugPrintf(SV_LOG_DEBUG,"Partition Starting offset   = %lld\n", PartitionSub.StartingOffset);

			DiskName = std::string(PartitionSub.DiskName);
			SV_UCHAR * PartitionSector = new SV_UCHAR[size_t(PartitionSub.PartitionLength)];
			if(NULL == PartitionSector)
			{
				fclose(pFileRead);
				DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for partition sector sector of disk %s\n", PartitionSub.DiskName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
			}
            if(! fread(PartitionSector, sizeof(SV_UCHAR)*(size_t(PartitionSub.PartitionLength)), 1, pFileRead))
            {
                fclose(pFileRead);
				delete [] PartitionSector;
                DebugPrintf(SV_LOG_ERROR,"Failed to read Partition data of disk-%s from binary file-%s\n", PartitionSub.DiskName, FileName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return DLM_ERR_FILE_READ;
            }
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Successfully read Partition data of disk-%s from binary file-%s\n", PartitionSub.DiskName, FileName);
				delete [] PartitionSector;
			}
			dpVec.push_back(PartitionSub);
		}
		dp.insert(make_pair(DiskName, dpVec));
	}
    fclose(pFileRead);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

DLM_ERROR_CODE DLDiscover::GetDlmVersion(const char *FileName, double &dlmVersion)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);     
    dlmVersion = 0.0;

	DebugPrintf(SV_LOG_DEBUG,"Reading file - %s.\n", FileName);

    //open the file in binary mode, read mode
    FILE *pFileRead = fopen(FileName,"rb");
    if(!pFileRead)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n", FileName);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_FILE_OPEN;
    }

	DLM_VERSION_INFO DLMVersion;
	if(! fread(&(DLMVersion), sizeof(DLMVersion), 1, pFileRead))
    {
        
        DebugPrintf(SV_LOG_INFO,"Failed to read version information from binary file-%s\n", FileName);
		DebugPrintf(SV_LOG_INFO,"Considering this as old structured dlm mbr file\n");
		dlmVersion = 1.0; //Initial dlm version
    }
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"DLM Version Major:Minor - %d:%d\n", DLMVersion.Major, DLMVersion.Minor);
		double minorVer = DLMVersion.Minor;
		while(minorVer >= 1.0) 
			minorVer = minorVer/10.0;
		dlmVersion = DLMVersion.Major + minorVer;
	}

	fclose(pFileRead);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

#ifdef WIN32
//This will compare the checksum of current discovery information to the checksum of older disovery information
//If there is difference returns true else false
bool DLDiscover::IsDlmInfoChanged(DisksInfoMap_t & d, bool & bCreateMbrFile, bool & bCreatePartitionFile, std::string & strCurChecksum, std::list<std::string>& prevDlmFiles)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	std::string strPrevChecksum;
	std::string strFilePath;
	unsigned char curCheckSum[16] = "";

	do
	{
		if(false == SetDlmRegPrivileges(SE_BACKUP_NAME, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set SE_BACKUP_NAME privileges.\n");
			bRet = false;
			break;
		}
		if(false == SetDlmRegPrivileges(SE_RESTORE_NAME, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set SE_RESTORE_NAME privileges.\n");
			bRet = false;
			break;
		}

		//Need to enhance the logic by implementing checksum calculation for partition info also
		if(GetDlmInfoFromReg(DLM_PARTITION_FILE_REG, strFilePath))		//Finds out the Partition file path from registry
		{
			DebugPrintf(SV_LOG_INFO, "Found the previous partition file : %s\n", strFilePath.c_str());
			prevDlmFiles.push_back(strFilePath);
			bCreatePartitionFile = false;		
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find the previous partition file name from registry\n");
			bCreatePartitionFile = true;
		}

		//Finds out the previous DLM Mbr file
		strFilePath.clear();
		if(GetDlmInfoFromReg(DLM_MBR_FILE_REG, strFilePath))		//Finds out the Partition file path from registry
		{
			DebugPrintf(SV_LOG_INFO, "Found the previous Dlm Mbr file : %s\n", strFilePath.c_str());
			prevDlmFiles.push_back(strFilePath);		
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find the previous Dlm Mbr file name from registry\n");
		}

		//Logic for checksum calculation for dlm disk information
		if(GetDlmInfoFromReg(DLM_CHECKSUM_REG, strPrevChecksum))		//Finds out the previuos checksum from registry
		{
			DebugPrintf(SV_LOG_INFO, "found cached previous check sum for disk label information\n");
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find the cached previous checksum for disk label information\n");
		}

		if(DLM_ERR_SUCCESS != DlmMd5Checksum(d, curCheckSum))
		{
			//need to write the necessary actions in case of failure
			DebugPrintf(SV_LOG_INFO,"New checksum calculation failed.\n");
			bRet = false;
		}
		else
		{
			strCurChecksum = GetBytesInArrayAsHexString(curCheckSum, 16);
			if(strPrevChecksum.compare(strCurChecksum) != 0)
			{
				DebugPrintf(SV_LOG_INFO, "Checksum is changed.\n");
				bCreateMbrFile = true;
			}
			else
				DebugPrintf(SV_LOG_INFO,"Old and New checksum are equal.\n");
		}
		break;
	}while(0);

	SetDlmRegPrivileges(SE_BACKUP_NAME, false);
	SetDlmRegPrivileges(SE_RESTORE_NAME, false);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
#else

//This will compare the checksum of current discovery information to the checksum of older disovery information
//If there is difference returns true else false
bool DLDiscover::IsDlmInfoChanged(DisksInfoMap_t & d, bool & bCreateMbrFile, bool & bCreatePartitionFile, std::string & strCurChecksum, std::list<std::string>& prevDlmFiles)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	//Need to write code in case fo Linux 

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
#endif