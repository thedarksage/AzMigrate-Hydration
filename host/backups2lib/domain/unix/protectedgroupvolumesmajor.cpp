#include <cstring>
#include <boost/algorithm/string/replace.hpp>

#include "devicestream.h"
#include "devicefilter.h"
#include "filterdrvifmajor.h"
#include "filterdrvifminor.h"
#include "runnable.h"
#include "volumegroupsettings.h"
#include "protectedgroupvolumes.h"
#include "cxmediator.h"
#include "volumechangedata.h"
#include "volumereporter.h"

#include "inmsafecapis.h"

int ProtectedGroupVolumes::StartFiltering(void)
{
    unsigned long int startfilterioctlcode = IOCTL_INMAGE_START_FILTERING_DEVICE;
    int iStatus = SV_SUCCESS;
    void *pvInParamToStartFilter = NULL;
    long int liInParamLen = 0;
    char VolumeGUID[GUID_SIZE_IN_CHARS];
    inm_dev_info_t devInfo;
         
    if (!m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
		pvInParamToStartFilter = const_cast<char *>(m_SourceNameForDriverInput.c_str());
		liInParamLen = m_SourceNameForDriverInput.length();
    }
    else
    {
        /* driver having diff order support */
        startfilterioctlcode = IOCTL_INMAGE_START_FILTERING_DEVICE_V2;
        FilterDrvIfMajor drvifmajor;
        bool bfilleddevinfo = drvifmajor.FillInmDevInfo(devInfo, m_VolumeReport);
        if (bfilleddevinfo)
        {
            // persistent name for /dev/sda is _dev_sda
            std::string pname = GetSourceDiskId();
            boost::replace_all(pname, "/", "_");
            inm_strncpy_s(devInfo.d_pname, ARRAYSIZE(devInfo.d_pname), pname.c_str(), INM_GUID_LEN_MAX - 1);
            drvifmajor.PrintInmDevInfo(devInfo);
        }
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "For source device %s, could not fill start filtering input\n", GetSourceVolumeName().c_str());
        }
        pvInParamToStartFilter = &devInfo;
        liInParamLen = sizeof devInfo;
    }

    if (SV_SUCCESS == iStatus)
    {
        FilterDrvIfMinor drvifminor;
		iStatus = drvifminor.PassMaxTransferSizeIfNeeded(m_pDeviceFilterFeatures, m_SourceNameForDriverInput, *m_pDeviceStream, m_VolumeReport);
        if (SV_FAILURE == iStatus)
        {
            DebugPrintf(SV_LOG_ERROR, "could not pass max transfer size for volume %s to involflt\n", GetSourceVolumeName().c_str());
        }
    }

    if (SV_SUCCESS == iStatus)
    {
        /*
        Issue the START_FILTERING_IOCTL to start the filtering.
        */
        while(!IsStopRequested())
        {
            iStatus = m_pDeviceStream->IoControl(startfilterioctlcode, pvInParamToStartFilter, liInParamLen);
            if(iStatus != SV_SUCCESS)
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED: start filtering ioctl: %d for volume %s. \n", iStatus, GetSourceVolumeName().c_str());
                WaitOnQuitEvent(30);
            }
            else
            {
                break;
            }
        }
    }

    return iStatus;
}


/*
 * FUNCTION NAME :  ComputeChangesToSend
 *
 * DESCRIPTION :    If the change offset is greater than volume size(formatted size) then the offset is adjusted to the size of the volume,
 and the length of the that disk change is set to zero.
 *                  If the sum of offset and length of change is greater than the volume size(formatted size) then the length is adjusted to the size of the volume,
 *
 * INPUT PARAMETERS :   vgData is the instance of volume change data containing the dirty block.
 *                      uliSent is the size of data from already uploaded to cx, in the current dirty block
 *                      uliChanges is the size of the changes to be uploaded to cx, in the current dirty block
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
void ProtectedGroupVolumes::ComputeChangesToSend(VolumeChangeData& vgData,
                                                 const unsigned long int& uliSent,
                                                 unsigned long int& uliChanges,
                                                 SV_LONGLONG & SizeOfChanges)
{
    unsigned long int DIRTY_BLOCK_CHANGES;
    unsigned long long *ChangeOffsetArray ;
    unsigned int *ChangeLengthArray ;
    unsigned long int prefixSize ;
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
        UDIRTY_BLOCK_V2* DirtyBlocks =  ( UDIRTY_BLOCK_V2* ) vgData.GetDirtyBlock() ;
        DIRTY_BLOCK_CHANGES = DirtyBlocks->uHdr.Hdr.cChanges;
        ChangeOffsetArray = DirtyBlocks->ChangeOffsetArray ;
        ChangeLengthArray = DirtyBlocks->ChangeLengthArray ;
        prefixSize = PREFIX_SIZE_V2 ;
    }
    else
    {
        UDIRTY_BLOCK* DirtyBlocks =  ( UDIRTY_BLOCK* ) vgData.GetDirtyBlock() ;
        DIRTY_BLOCK_CHANGES = DirtyBlocks->uHdr.Hdr.cChanges;
        ChangeOffsetArray = (unsigned long long*) DirtyBlocks->ChangeOffsetArray ;
        ChangeLengthArray = DirtyBlocks->ChangeLengthArray ;
        prefixSize = PREFIX_SIZE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "============ number of changes in bitmap / metadata capture mode = %lu =========\n", DIRTY_BLOCK_CHANGES);

    unsigned long int uliIndex = uliSent;
    SV_LONGLONG Size=0;
    SV_LONGLONG     DataSize=0;

    while( uliIndex < DIRTY_BLOCK_CHANGES)
    {
        std::stringstream ssofflen;
        ssofflen << ChangeOffsetArray[uliIndex] << ", " << ChangeLengthArray[uliIndex];
        DebugPrintf(SV_LOG_DEBUG, "%s\n", ssofflen.str().c_str());

        if ( ChangeOffsetArray[uliIndex] >= m_VolumeSize )
        {
            std::stringstream ss;
            ss << "SEEING METADATA/BITMAP OFFSET " << ChangeOffsetArray[uliIndex]
               << ", WHICH IS BEYOND OR EQUAL TO VOLUME SIZE " << m_VolumeSize
               << ". NOT SENDING THIS CHANGE";
            DebugPrintf(SV_LOG_ERROR, "For volume %s, %s\n", GetSourceVolumeName().c_str(), ss.str().c_str());
            ChangeOffsetArray[uliIndex] = m_VolumeSize;
            ChangeLengthArray[uliIndex] = 0;
        }

        if ( ChangeOffsetArray[uliIndex] + ChangeLengthArray[uliIndex] > m_VolumeSize)
        {
            SV_LONGLONG TempLength=0;
            TempLength  = m_VolumeSize - ChangeOffsetArray[uliIndex];
            std::stringstream ss;
            ss << "SEEING SUM OF METADATA/BITMAP OFFSET " << ChangeOffsetArray[uliIndex]
               << " AND LENGTH " << ChangeLengthArray[uliIndex]
               << ", WHICH IS BEYOND VOLUME SIZE " << m_VolumeSize
               << ". ADJUSTING LENGTH TO " << TempLength << ", SO THAT END OFFSET IS VOLUME SIZE ";
            DebugPrintf(SV_LOG_ERROR, "For volume %s, %s\n", GetSourceVolumeName().c_str(), ss.str().c_str());
            ChangeLengthArray[uliIndex] = TempLength;
        }

        if ( ChangeLengthArray[uliIndex] > 0 )
        {
            Size = Size + ( prefixSize + ChangeLengthArray[uliIndex]);
            DataSize += ChangeLengthArray[uliIndex];
        }
        ++uliIndex;
    }

    SizeOfChanges = Size;
    uliChanges  = uliIndex;
}


bool ProtectedGroupVolumes::ShouldWaitForDataOnLessChanges(VolumeChangeData &Changes)
{
	return true;
}
