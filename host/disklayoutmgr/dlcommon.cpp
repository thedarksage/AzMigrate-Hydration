#include "dlcommon.h"


//*************************************************************************************************
// This function reads the data from disk directly (like dd)
// Output   - Data read from disk
//*************************************************************************************************
bool DLCommon::ReadFromDisk(const char *DiskName, SV_LONGLONG BytesToSkip, size_t BytesToRead, SV_UCHAR *DiskData)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    ACE_HANDLE hdl;    
    size_t BytesRead = 0;


    hdl = ACE_OS::open(DiskName, O_RDONLY, ACE_DEFAULT_OPEN_PERMS);
    if(ACE_INVALID_HANDLE == hdl)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::open failed: disk %s, err = %d\n ", DiskName, ACE_OS::last_error());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    if (ACE_OS::llseek(hdl, (ACE_LOFF_T)BytesToSkip, SEEK_SET) < 0)
    {   
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::lseek failed: disk %s, offset 0, err = %d\n", DiskName, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    BytesRead = ACE_OS::read(hdl, DiskData, BytesToRead);
	if(BytesRead != BytesToRead)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::read failed: disk %s, err = %d\n", DiskName, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}   
    ACE_OS::close(hdl);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//*************************************************************************************************
// This function writes the data from disk directly (like dd)
// Output   - Data read from disk
//*************************************************************************************************
bool DLCommon::WriteToDisk(const char *DiskName, SV_LONGLONG BytesToSkip, size_t BytesToWrite, const SV_UCHAR *DiskData)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    ACE_HANDLE hdl;    
    size_t BytesWrite = 0;

    hdl = ACE_OS::open(DiskName,O_RDWR,ACE_DEFAULT_OPEN_PERMS);
    if(ACE_INVALID_HANDLE == hdl)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::open failed: disk %s, err = %d\n ", DiskName, ACE_OS::last_error());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    if (ACE_OS::llseek(hdl, (ACE_LOFF_T)BytesToSkip, SEEK_SET) < 0)
    {   
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::lseek failed: disk %s, offset 0, err = %d\n", DiskName, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    BytesWrite = ACE_OS::write(hdl, DiskData, BytesToWrite);
	if(BytesWrite != BytesToWrite)
    {
        DebugPrintf(SV_LOG_ERROR,"Error: ACE_OS::write failed: disk %s, err = %d\n", DiskName, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}   
    ACE_OS::close(hdl);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//*************************************************************************************************
// This function read the data from the file in binary format.
//*************************************************************************************************
bool DLCommon::ReadFromBinaryFile(SV_UCHAR *FileData, size_t BytesToRead, const char *FileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    FILE *fp;

	fp = fopen(FileName,"rb");
	if(!fp)
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fopen failed: file %s\n",FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
   
    if(! fread(FileData, BytesToRead, 1, fp))
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fread failed: file %s\n",FileName);
        fclose(fp);        
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}
    fclose(fp);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//*************************************************************************************************
// This function writes the data to the file in binary format.
//*************************************************************************************************
bool DLCommon::WriteToBinaryFile(const SV_UCHAR *FileData, size_t BytesToWrite, const char *FileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    FILE *fp;

	fp = fopen(FileName,"wb");
	if(!fp)
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fopen failed: file %s\n",FileName);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    
    if(! fwrite(FileData, BytesToWrite, 1, fp))
	{
        DebugPrintf(SV_LOG_ERROR,"Error: fwrite failed: file %s\n",FileName);
        fclose(fp);        
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}
    fclose(fp);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//*************************************************************************************************
// This function returns specified contigous set of bytes from given input mbr in 
// proper format i.e. MBRs will be in little endian format. so consider that too.
// Example - get 4 bytes starting from 474(i.e 474-477) bytes from a string of size 512 bytes.
// MBR is in Little Endian format - so store in reverse order.
//*************************************************************************************************
void DLCommon::GetBytesFromMBR(const SV_UCHAR *MbrArr, size_t StartIndex, size_t BytesToFetch, SV_UCHAR *OutData)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    for(size_t i=0; i<BytesToFetch; i++)
    {
        OutData[i] = MbrArr[StartIndex + BytesToFetch - 1 - i]; //ex: 474 + 4 - 1 - 0(i)
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//*************************************************************************************************
// Convert the hex array to decimal number
// Format of Hex Array - a[0]=1A , a[1]=2B, a[2]=3C, a[3]=4D
// 1. Merge it and make it a hex number 1A2B3C4D
// 2. Convert to Decimal number (= 439041101)
//*************************************************************************************************
void DLCommon::ConvertArrayFromHexToDec(SV_UCHAR *HexArray, size_t NoOfBytes, SV_LONGLONG & DecNumber)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DecNumber = 0;
    for(size_t i=0; i<NoOfBytes; i++)
    {
        DecNumber = DecNumber<<8;
        DecNumber = DecNumber | (HexArray[i] & 0xFF); 
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//*************************************************************************************************
// Get the vector of volumes info for the given disk from the multimap
//*************************************************************************************************
void DLCommon::GetVolumesPerGivenDisk(const char * DiskName,
                                      VolumesInfoMultiMap_t mmapDiskToVolInfo, 
                                      VolumesInfoVec_t & vecVolsInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
    VolumesInfoMultiMapIter_t iter = mmapDiskToVolInfo.find(std::string(DiskName));
    if(iter == mmapDiskToVolInfo.end())
    {
        DebugPrintf(SV_LOG_INFO, "No volumes are found for the disk - %s\n", DiskName);
        return;
    }
    
    VolumesInfoMultiMapIter_t iterLast  = mmapDiskToVolInfo.upper_bound(std::string(DiskName));    
    for(; iter != iterLast; iter++)
    {
        DebugPrintf(SV_LOG_INFO,"%s ==> %s\n", iter->first.c_str(), iter->second.VolumeName);        
        vecVolsInfo.push_back(iter->second);        
    }       

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//*************************************************************************************************
// Check if MBR is valid, and return true if valid.
// To do this, check last two bytes of MBR for mbr signature 0xAA55 (little endian)
// check if     a[510] = 0x55   and
//              a[511] = 0xAA
//*************************************************************************************************
bool DLCommon::isValidMBR(SV_UCHAR * Sector)
{    
    if( ( MBR_SIGNATURE_FIRST_BYTE == Sector[MBR_BOOT_SECTOR_LENGTH - 1] ) &&
        ( MBR_SIGNATURE_SECOND_BYTE == Sector[MBR_BOOT_SECTOR_LENGTH - 2] ) )
        return true;
    else
        return false;
}

bool DLCommon::isGptDisk(SV_UCHAR * Sector)
{    
    if( GPT_PARTITION_TYPE == Sector[MBR_PARTITION_1_PARTITIONTYPE] )
        return true;
    else
        return false;
}

bool DLCommon::isGptDiskDynamic(SV_UCHAR * Sector)
{    
    GPT_GUID PartitionTypeGuid;
        
    GetGUIDFromGPT(Sector, GPT_PARTITION_1_STARTINGSECTOR, PartitionTypeGuid);    
    
    return !memcmp(&PartitionTypeGuid, &PARTITION_LDM_METADATA_GUID, sizeof(GPT_GUID));    
}

//*************************************************************************************************
// Get GUID from GPT, starting from the given offset
//*************************************************************************************************
void DLCommon::GetGUIDFromGPT(SV_UCHAR * Sector, size_t Offset, GPT_GUID & PartitionTypeGuid)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    SV_UCHAR TempData1[4];
    SV_UCHAR TempData2[2];
    SV_UCHAR TempData3[2];

    SV_LONGLONG TempNumber = 0; 

    GetBytesFromMBR(Sector, Offset, sizeof(TempData1), TempData1);
    ConvertArrayFromHexToDec(TempData1, sizeof(TempData1), TempNumber);
    PartitionTypeGuid.Data1 = (SV_ULONG)TempNumber;
    
    Offset += sizeof(TempData1);
    TempNumber = 0;
    GetBytesFromMBR(Sector, Offset, sizeof(TempData2), TempData2);
    ConvertArrayFromHexToDec(TempData2, sizeof(TempData2), TempNumber);
    PartitionTypeGuid.Data2 = (SV_USHORT)TempNumber;
    
    Offset += sizeof(TempData2);
    TempNumber = 0;
    GetBytesFromMBR(Sector, Offset, sizeof(TempData3), TempData3);
    ConvertArrayFromHexToDec(TempData3, sizeof(TempData3), TempNumber);
    PartitionTypeGuid.Data3 = (SV_USHORT)TempNumber;

    Offset += sizeof(TempData3);
    for(int i=0; i<8; i++)
    {        
        PartitionTypeGuid.Data4[i] = Sector[Offset + i] & 0xFF;
    }

    DebugPrintf(SV_LOG_INFO,"GUID of GPT Disk  - %X %X %X %X %X %X %X %X %X %X %X\n", 
                                PartitionTypeGuid.Data1, 
                                PartitionTypeGuid.Data2, 
                                PartitionTypeGuid.Data3,
                                (SV_USHORT)PartitionTypeGuid.Data4[0],
                                (SV_USHORT)PartitionTypeGuid.Data4[1],
                                (SV_USHORT)PartitionTypeGuid.Data4[2],
                                (SV_USHORT)PartitionTypeGuid.Data4[3],
                                (SV_USHORT)PartitionTypeGuid.Data4[4],
                                (SV_USHORT)PartitionTypeGuid.Data4[5],
                                (SV_USHORT)PartitionTypeGuid.Data4[6],
                                (SV_USHORT)PartitionTypeGuid.Data4[7]
                                );   
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool DLCommon::isMbrDiskDynamic(SV_UCHAR * Sector)
{    
    if( MBR_DYNAMIC_DISK == Sector[MBR_PARTITION_1_PARTITIONTYPE] )
        return true;
    else
        return false;
}

//*************************************************************************************************
// Check if disk signature is zero. If it is then disk is uninitialized
// Note in case of GPT disks, disk signature will be null too.
// But this issue will not come as GPT disk check is done first. So call this only if disk is not GPT.
//*************************************************************************************************
bool DLCommon::isUninitializedDisk(SV_UCHAR * Sector)
{ 
    SV_UCHAR DiskSignatureArr[MBR_MAGIC_COUNT];
    GetBytesFromMBR(Sector,
                    MBR_DISK_SIGNATURE_BEGIN,
                    sizeof(DiskSignatureArr),
                    DiskSignatureArr);
    SV_LONGLONG DiskSignature = 0;
    ConvertArrayFromHexToDec(DiskSignatureArr, sizeof(DiskSignatureArr), DiskSignature);

    if(0 == DiskSignature)
        return true;
    else
        return false;
}

bool DLCommon::isExtendedPartition(SV_UCHAR * Sector, size_t index)
{       
    if( ( EXTENDED_PARTITION_TYPE1 == Sector[index] ) ||
        ( EXTENDED_PARTITION_TYPE2 == Sector[index] ) ) 
        return true;    
    else
        return false;
}

void DLCommon::GetEBRStartingOffset(SV_UCHAR * BootRecord, size_t BootRecordOffset, SV_LONGLONG & Offset)
{    
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    SV_UCHAR TempSector[MBR_MAGIC_COUNT];
    
    GetBytesFromMBR(BootRecord,
                    BootRecordOffset,
                    MBR_MAGIC_COUNT,
                    TempSector
                    );
    
    ConvertArrayFromHexToDec(TempSector, MBR_MAGIC_COUNT, Offset);
    DebugPrintf(SV_LOG_DEBUG,"EBR Starting Offset - %lld\n", Offset);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//*************************************************************************************************
// 1. Read EBR - (First EBR resides at starting offset of extended partition)
// 2. Parse EBR and get next EBR's starting offset (Bytes 474-477 in little endian format)
// 3. if next EBR's starting offset is not zero, then collect it and repeat from step 2.
// 4. if next EBR's starting offset is zero, then return.
//*************************************************************************************************
DLM_ERROR_CODE DLCommon::CollectEBRs(const char * DiskName, SV_LONGLONG PartitionStartOffset, SV_ULONGLONG BytesPerSector, std::vector<EBR_SECTOR> & vecEBRs)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__); 
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;
    SV_LONGLONG BytesToSkip = PartitionStartOffset;    

    while(1)
    {           
        DebugPrintf(SV_LOG_INFO,"Bytes to skip = %lld\n", BytesToSkip);
        EBR_SECTOR es;
        if(false == ReadFromDisk(DiskName, BytesToSkip, MBR_BOOT_SECTOR_LENGTH, es.EbrSector))
        {            
            DebugPrintf(SV_LOG_ERROR,"Failed to read the EBR sector from disk - %s\n",DiskName);
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    RetVal = DLM_ERR_DISK_READ;
            break;
        }         
        vecEBRs.push_back(es);

        SV_UCHAR EbrSecondEntry[MBR_MAGIC_COUNT];
        GetBytesFromMBR(es.EbrSector, EBR_SECOND_ENTRY_STARTING_INDEX, MBR_MAGIC_COUNT, EbrSecondEntry);
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
    DebugPrintf(SV_LOG_INFO,"Total Number of EBRs = %d\n",vecEBRs.size());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

DLM_ERROR_CODE DLCommon::ProcessMbrDisk(DISK_INFO & d, const char * DiskName, SV_UCHAR * MbrSector, std::string& strDiskType)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;       

    if(strDiskType.compare("dynamic") == 0)
    {
        DebugPrintf(SV_LOG_INFO,"%s is Dynamic disk.\n",DiskName);
        SetDiskType(d, DYNAMIC);
        SetDiskEbrCount(d, 0);

		SV_UCHAR *MbrDynamicSector = new SV_UCHAR[MBR_DYNAMIC_BOOT_SECTOR_LENGTH];
		if(NULL != MbrDynamicSector)
		{
			if(false == ReadFromDisk(DiskName, 0, MBR_DYNAMIC_BOOT_SECTOR_LENGTH, MbrDynamicSector))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to read the data from disk - %s\n",DiskName);
				SetDiskFlag(d, FLAG_NO_BOOTRECORD);
				delete[] MbrDynamicSector;
				return DLM_ERR_DISK_READ;
			}
			SetDynamicDiskMbrSector(d, MbrDynamicSector);

			SV_UCHAR *LdmDynamicSector = new SV_UCHAR[LDM_DYNAMIC_BOOT_SECTOR_LENGTH];
			if(NULL != LdmDynamicSector)
			{
				SV_LONGLONG bytesToSkip = d.DiskInfoSub.Size - LDM_DYNAMIC_BOOT_SECTOR_LENGTH;
				if(false == ReadFromDisk(DiskName, bytesToSkip, LDM_DYNAMIC_BOOT_SECTOR_LENGTH, LdmDynamicSector))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to read the data from disk - %s\n",DiskName);
					SetDiskFlag(d, FLAG_NO_BOOTRECORD);
					delete[] LdmDynamicSector;
					delete[] MbrDynamicSector;
					return DLM_ERR_DISK_READ;
				}
				SetDynamicDiskLdmSector(d, LdmDynamicSector);
				delete[] LdmDynamicSector;
				delete[] MbrDynamicSector;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to allocate dynamic memory for - LdmDynamicSector\n");
				delete[] MbrDynamicSector;
				return DLM_ERR_DISK_READ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to allocate dynamic memory for - MbrDynamicSector\n");
			return DLM_ERR_DISK_READ;
		}

	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return DLM_ERR_DYNAMICDISK;
    }
	else		        
        SetDiskMbrSector(d, MbrSector);

    //Read each partition and check for extended.
    size_t PartitionTypeOffset = MBR_PARTITION_1_PARTITIONTYPE;
    for(int i = 0; i < MBR_MAGIC_COUNT; i++)
    {
        if(isExtendedPartition(MbrSector, PartitionTypeOffset))
        {            
            size_t BootRecordOffset = MBR_PARTITION_1_STARTINGSECTOR + i * 16;
            DebugPrintf(SV_LOG_INFO,"Partition-%d is an Extended Partition. MBR Offset for starting sector of this - %u.\n",i+1,BootRecordOffset);
            SV_LONGLONG ExtPartitionStartOffsetValue = 0;
            GetEBRStartingOffset(MbrSector, BootRecordOffset, ExtPartitionStartOffsetValue);
            ExtPartitionStartOffsetValue *= d.DiskInfoSub.BytesPerSector;            
            if(0 != ExtPartitionStartOffsetValue)
            {
                std::vector<EBR_SECTOR> vecEBRs;
                if(DLM_ERR_SUCCESS == CollectEBRs(DiskName, ExtPartitionStartOffsetValue, d.DiskInfoSub.BytesPerSector, vecEBRs))
                {
                    SetDiskEbrCount(d, vecEBRs.size());
                    SetDiskEbrSectors(d, vecEBRs);
                }
                else
                {
                    SetDiskFlag(d, FLAG_NO_EBR);
                    SetDiskEbrCount(d, 0);
                    RetVal = DLM_ERR_FETCH_EBR;
                }            
            }
            break; //as only one extended partition can exist per disk
        }
        PartitionTypeOffset += 16;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

DLM_ERROR_CODE DLCommon::ProcessGptDisk(DISK_INFO & d, const char * DiskName, std::string& strDiskType)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS;

    do
    {
        SV_UCHAR *GptSector = new SV_UCHAR[GPT_BOOT_SECTOR_LENGTH];
		if(NULL != GptSector)
		{
			if(false == ReadFromDisk(DiskName, 0, GPT_BOOT_SECTOR_LENGTH, GptSector))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to read the data from disk - %s\n",DiskName);
				SetDiskFlag(d, FLAG_NO_BOOTRECORD);
				RetVal = DLM_ERR_DISK_READ;
				delete [] GptSector;
				break;
			}
			SetDiskGptSector(d, GptSector);

			if(strDiskType.compare("dynamic") == 0)
			{
				DebugPrintf(SV_LOG_INFO,"%s is Dynamic disk.\n",DiskName);
				SetDiskType(d, DYNAMIC);            
				RetVal = DLM_ERR_DYNAMICDISK;
				SV_UCHAR *GptDynamicSector = new SV_UCHAR[GPT_DYNAMIC_BOOT_SECTOR_LENGTH];
				if(NULL != GptDynamicSector)
				{
					if(false == ReadFromDisk(DiskName, 0, GPT_DYNAMIC_BOOT_SECTOR_LENGTH, GptDynamicSector))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to read the data from disk - %s\n",DiskName);
						SetDiskFlag(d, FLAG_NO_BOOTRECORD);
						RetVal = DLM_ERR_DISK_READ;
						delete [] GptSector;
						delete [] GptDynamicSector;
						break;
					}
					SetDynamicDiskGptSector(d, GptDynamicSector);
					delete [] GptDynamicSector;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to allocate dynamic memory for - GptDynamicSector\n");
					RetVal = DLM_ERR_DISK_READ;
				}
				delete [] GptSector;
				break;
			}
            delete[] GptSector;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to allocate dynamic memory for - GptSector\n");
			RetVal = DLM_ERR_DISK_READ;
		}
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return RetVal;
}

DLM_ERROR_CODE DLCommon::ProcessDiskAndUpdateDiskInfo(DISK_INFO & d, const char * DiskName, std::string& strDiskType)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    SV_UCHAR * MbrSector = new SV_UCHAR[MBR_BOOT_SECTOR_LENGTH];
	//DLM_ERROR_CODE RetVal = DLM_ERR_SUCCESS; 
	if(NULL == MbrSector)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for - MbrSector\n");
		DebugPrintf(SV_LOG_ERROR,"Failed to collect the MBR from the disk - %s\n",DiskName);
		SetDiskFlag(d, FLAG_NO_BOOTRECORD);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return DLM_ERR_DISK_READ;
	}

    if(false == ReadFromDisk(DiskName, 0, MBR_BOOT_SECTOR_LENGTH, MbrSector))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to read the MBR from the disk - %s\n",DiskName);        
        SetDiskFlag(d, FLAG_NO_BOOTRECORD);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		delete [] MbrSector;
        return DLM_ERR_DISK_READ;
    }

    if(! isValidMBR(MbrSector) )
    {        
        DebugPrintf(SV_LOG_ERROR,"%s does not have valid MBR. Hence ignoring this disk.\n",DiskName);
        //May be we can consider this as raw disk. Check once and change if needed.
        SetDiskFormatType(d, RAWDISK);        
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		delete [] MbrSector;
        return DLM_ERR_RAWDISK;
    }

    if(isGptDisk(MbrSector))
    {
        DebugPrintf(SV_LOG_INFO,"%s is GPT disk.\n",DiskName);        
        SetDiskFormatType(d, GPT);        
        ProcessGptDisk(d, DiskName, strDiskType);
    }
    else 
    {
        if(isUninitializedDisk(MbrSector)) //Treat this as raw disk
        {
            DebugPrintf(SV_LOG_INFO,"%s is Uninitialized disk.\n",DiskName);
            SetDiskFormatType(d, RAWDISK);            
	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			delete [] MbrSector;
            return DLM_ERR_RAWDISK;
        }

        DebugPrintf(SV_LOG_INFO,"%s is MBR disk.\n",DiskName);        
        SetDiskFormatType(d, MBR);
        ProcessMbrDisk(d, DiskName, MbrSector, strDiskType);
    }
	delete [] MbrSector;
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return DLM_ERR_SUCCESS;
}

void DLCommon::SetDiskFlag(DISK_INFO & d, SV_ULONGLONG f)
{
    d.DiskInfoSub.Flag |= f;    
}

void DLCommon::SetDiskName(DISK_INFO & d, const char * DiskName)
{    
    inm_strncpy_s(d.DiskInfoSub.Name, ARRAYSIZE(d.DiskInfoSub.Name), DiskName, MAX_PATH_COMMON);
    d.DiskInfoSub.Name[MAX_PATH_COMMON] = '\0'; //since size is MAX_PATH_COMMON+1
}

void DLCommon::SetDiskScsiId(DISK_INFO & d, SCSIID s)
{
    d.DiskInfoSub.ScsiId = s;
}

void DLCommon::SetDiskSize(DISK_INFO & d, SV_LONGLONG ds)
{    
    d.DiskInfoSub.Size = ds;
}

void DLCommon::SetDiskSignature(DISK_INFO & d, const char * DiskSignature)
{
	inm_strncpy_s(d.DiskSignature, ARRAYSIZE(d.DiskSignature), DiskSignature, MAX_DISK_SIGNATURE_LENGTH);
	d.DiskSignature[MAX_DISK_SIGNATURE_LENGTH] = '\0'; //since size is MAX_DISK_SIGNATURE_LENGTH+1
}

void DLCommon::SetDiskDeviceId(DISK_INFO & d, const char * DeviceId)
{
	inm_strncpy_s(d.DiskDeviceId, ARRAYSIZE(d.DiskDeviceId), DeviceId, MAX_PATH_COMMON);
	d.DiskDeviceId[MAX_DISK_SIGNATURE_LENGTH] = '\0'; //since size is MAX_DISK_SIGNATURE_LENGTH+1
}

void DLCommon::SetDiskVolInfo(DISK_INFO & d, const char * DiskName, VolumesInfoMultiMap_t mmapDiskToVolInfo)
{
    VolumesInfoVec_t vecVolsInfo;

    GetVolumesPerGivenDisk(DiskName, mmapDiskToVolInfo, vecVolsInfo);
    if(0 == vecVolsInfo.size())
        SetDiskFlag(d, FLAG_NO_VOLUMESINFO); 
    else
    {
        d.VolumesInfo = vecVolsInfo;
        d.DiskInfoSub.VolumeCount = vecVolsInfo.size();
    }
}

void DLCommon::SetDiskBytesPerSector(DISK_INFO & d, SV_ULONGLONG byterPerSector)
{
	d.DiskInfoSub.BytesPerSector = byterPerSector;
}

void DLCommon::SetDiskType(DISK_INFO & d, DISK_TYPE t)
{
    d.DiskInfoSub.Type = t;
}

void DLCommon::SetDiskFormatType(DISK_INFO & d, DISK_FORMAT_TYPE ft)
{
    d.DiskInfoSub.FormatType = ft;
}

void DLCommon::SetDiskEbrCount(DISK_INFO & d, SV_ULONGLONG count)
{
    d.DiskInfoSub.EbrCount = count;
}

void DLCommon::SetDiskEbrSectors(DISK_INFO & d, std::vector<EBR_SECTOR> vecEBRs)
{
    d.EbrSectors = vecEBRs;
}

void DLCommon::SetDiskMbrSector(DISK_INFO & d, SV_UCHAR MbrSector[])
{
    for(int i=0; i<MBR_BOOT_SECTOR_LENGTH; i++)
    {
        d.MbrSector[i] = MbrSector[i];
    }
}

void DLCommon::SetDiskGptSector(DISK_INFO & d, SV_UCHAR GptSector[])
{
    for(int i=0; i<GPT_BOOT_SECTOR_LENGTH; i++)
    {
        d.GptSector[i] = GptSector[i];
    }
}

void DLCommon::SetDynamicDiskMbrSector(DISK_INFO & d, SV_UCHAR MbrSector[])
{
	if(d.MbrDynamicSector == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for MbrDynamicSector \n");
		return;
	}
    for(int i=0; i<MBR_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
    {
		d.MbrDynamicSector[i] = MbrSector[i];
    }
}

void DLCommon::SetDynamicDiskGptSector(DISK_INFO & d, SV_UCHAR GptSector[])
{
	if(d.GptDynamicSector == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for GptDynamicSector \n");
		return;
	}
    for(int i=0; i<GPT_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
    {
		d.GptDynamicSector[i] = GptSector[i];
    }
}

void DLCommon::SetDynamicDiskLdmSector(DISK_INFO & d, SV_UCHAR LdmSector[])
{
	if(d.LdmDynamicSector == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for LdmDynamicSector \n");
		return;
	}
    for(int i=0; i<LDM_DYNAMIC_BOOT_SECTOR_LENGTH; i++)
    {
		d.LdmDynamicSector[i] = LdmSector[i];
    }
}
