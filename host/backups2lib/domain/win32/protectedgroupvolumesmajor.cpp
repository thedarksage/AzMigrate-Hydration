#include <cstring>
#include <iostream>
#include <string>
#include <list>
#include <sstream>
#include <exception>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <fstream>
#include <windows.h>
#include <winioctl.h>
#include "devicefilter.h"
#include "runnable.h"
#include "volumegroupsettings.h"
#include "protectedgroupvolumes.h"
#include "cxmediator.h"
#include "devicestream.h"
#include "volumechangedata.h"

#include "filterdrvifmajor.h"

int ProtectedGroupVolumes::ProcessLastIOATLUNRequest(void)
{
    return SV_FAILURE;
}


int ProtectedGroupVolumes::StartFiltering(void)
{
	void *pInput;
	long inputLen;
	START_FILTERING_INPUT sfi;

	bool issueIoctl;
	//Fill input to start filtering ioctl
	if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == m_pDeviceStream->GetName()) {
		//For disk driver, input is START_FILTERING_INPUT
		memset(&sfi, 0, sizeof sfi);
		FilterDrvIfMajor drvifmajor;
        issueIoctl = drvifmajor.FillStartFilteringInput(m_SourceNameForDriverInput, m_VolumeReport, sfi);
		if (issueIoctl) {
			drvifmajor.PrintStartFilteringInput(sfi);
			pInput = &sfi;
			inputLen = sizeof sfi;
		}
		else
			DebugPrintf(SV_LOG_ERROR, "For source device %s, could not fill start filtering input.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), __LINE__, __FILE__);
	}
	else {
		//For volume driver, it is direct volume name
		pInput = const_cast<wchar_t *>(m_SourceNameForDriverInput.c_str());
		inputLen = sizeof(wchar_t) * m_SourceNameForDriverInput.length();
		issueIoctl = true;
	}

	//Issue ioctl
	int iStatus = SV_FAILURE;
	if (issueIoctl) {
		iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_START_FILTERING_DEVICE, pInput, inputLen, NULL, 0);
		if (iStatus != SV_SUCCESS)
			DebugPrintf(SV_LOG_ERROR, "For device %s, start filtering ioctl failed with return value: %d, error code: %d.@LINE %d in FILE %s\n", 
			GetSourceVolumeName().c_str(), iStatus, m_pDeviceStream->GetErrorCode(), __LINE__, __FILE__);
	}

    return iStatus;
}


void ProtectedGroupVolumes::ComputeChangesToSend(VolumeChangeData& vgData,
                                                 const unsigned long int& uliSent,
                                                 unsigned long int& uliChanges,
                                                 SV_LONGLONG& SizeOfChanges)
{
    unsigned long int DIRTY_BLOCK_CHANGES ;
    SV_ULONGLONG *ChangeOffsetArray ;
    unsigned long int prefixSize ;
    ULONG *ChangeLengthArray ;
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
    {
        UDIRTY_BLOCK_V2* DirtyBlocks = ( ( UDIRTY_BLOCK_V2* ) vgData.GetDirtyBlock() ) ;
        ChangeOffsetArray = DirtyBlocks->ChangeOffsetArray ;
        ChangeLengthArray = DirtyBlocks->ChangeLengthArray ;
        DIRTY_BLOCK_CHANGES = DirtyBlocks->uHdr.Hdr.cChanges ;
        prefixSize = PREFIX_SIZE_V2 ;
    }
    else
    {
        UDIRTY_BLOCK* DirtyBlocks = ( ( UDIRTY_BLOCK* ) vgData.GetDirtyBlock() ) ;
        ChangeOffsetArray = (SV_ULONGLONG *)DirtyBlocks->ChangeOffsetArray ;
        ChangeLengthArray = DirtyBlocks->ChangeLengthArray ;
        DIRTY_BLOCK_CHANGES = DirtyBlocks->uHdr.Hdr.cChanges ;
        prefixSize = PREFIX_SIZE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "============ number of changes in bitmap / metadata capture mode = %lu =========\n", DIRTY_BLOCK_CHANGES);

    unsigned long int uliIndex = uliSent;
    SV_ULARGE_INTEGER Size;
    SV_ULARGE_INTEGER DataSize;
    Size.QuadPart = 0;
    DataSize.QuadPart = 0;

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
            SV_LARGE_INTEGER TempLength;
            TempLength.QuadPart = m_VolumeSize - ChangeOffsetArray[uliIndex];
            std::stringstream ss;
            ss << "SEEING SUM OF METADATA/BITMAP OFFSET " << ChangeOffsetArray[uliIndex]
               << " AND LENGTH " << ChangeLengthArray[uliIndex]
               << ", WHICH IS BEYOND VOLUME SIZE " << m_VolumeSize
               << ". ADJUSTING LENGTH TO " << TempLength.u.LowPart << ", SO THAT END OFFSET IS VOLUME SIZE ";
            DebugPrintf(SV_LOG_ERROR, "For volume %s, %s\n", GetSourceVolumeName().c_str(), ss.str().c_str());
            ChangeLengthArray[uliIndex] = TempLength.u.LowPart ;
        }

		if ( ChangeLengthArray[uliIndex] > 0 )
        {
            Size.QuadPart = Size.QuadPart + ( prefixSize + ChangeLengthArray[uliIndex]);
            DataSize.QuadPart += ChangeLengthArray[uliIndex];
        }
        ++uliIndex;
    }
    SizeOfChanges = Size.QuadPart;
    uliChanges  = uliIndex;
}


bool ProtectedGroupVolumes::ShouldWaitForDataOnLessChanges(VolumeChangeData &Changes)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with bytes to drain without wait " LLSPEC "\n", FUNCTION_NAME, m_BytesToDrainWithoutWait);
	SV_LONGLONG totalchangespending = Changes.GetTotalChangesPending();
    SV_LONGLONG currentchanges = Changes.GetCurrentChanges();

	if (m_BytesToDrainWithoutWait) {
		if (Changes.IsWriteOrderStateData()) {
			DebugPrintf(SV_LOG_DEBUG, "Write order state is data. Hence no need to drain immediately.\n");
			m_BytesToDrainWithoutWait = 0;
		} else
			m_BytesToDrainWithoutWait = (m_BytesToDrainWithoutWait>=currentchanges) ? (m_BytesToDrainWithoutWait-currentchanges) : 0;
	} else if ( ((SOURCE_FILE==Changes.GetDataSource())||(SOURCE_STREAM==Changes.GetDataSource()))
                && (!Changes.IsWriteOrderStateData()) ) {
		m_BytesToDrainWithoutWait = totalchangespending;
		DebugPrintf(SV_LOG_DEBUG, "bytes to drain without wait initialized as " LLSPEC "\n", m_BytesToDrainWithoutWait);
	} else {
		DebugPrintf(SV_LOG_DEBUG, "No need to drain immediately.\n");
		m_BytesToDrainWithoutWait = 0;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s with bytes to drain without wait " LLSPEC "\n", FUNCTION_NAME, m_BytesToDrainWithoutWait);
	return (!m_BytesToDrainWithoutWait);
}
