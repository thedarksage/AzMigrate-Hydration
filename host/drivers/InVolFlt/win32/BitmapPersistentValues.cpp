/*
 *This File contains the class for manipulating the persistent 
 *Values on to the bitmap.
*/

#include <global.h>
#include <misc.h>
#include "VContext.h"

BitmapPersistentValues::BitmapPersistentValues(PVOLUME_CONTEXT pVolume_Context) {
    m_pVolumeContext = pVolume_Context;
    m_bFilteringStateStopped = 0;
    m_GlobalTimeStamp = 0;
    m_GlobalSequenceNumber = 0;
    m_bResyncRequired =  false;
    m_ulOutOfSyncErrorCode = 0;
    m_ulOutOfSyncErrorStatus = 0;
    m_ullOutOfSyncTimeStamp = 0;
    m_ulOutOfSyncCount = 0;
    m_ulOutOfSyncIndicatedAtCount = 0 ;
    m_ullOutOfSyncIndicatedTimeStamp = 0;
    m_ullOutOfSyncResetTimeStamp = 0;
    m_PrevEndTimeStamp = 0;
    m_PrevEndSequenceNumber = 0;
    m_PrevSequenceId = 0;
}

ULONGLONG 
BitmapPersistentValues::GetGlobalSequenceNumber() //Return from DriverContext
{
    ULONGLONG SequenceNumber;
    KIRQL   OldIrql;

    KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);
	// LastSequenceNumber is of type ULONGLONG in Bitmap header, no effect on upgrade for 32-bit global sequence number support
    SequenceNumber = DriverContext.ullTimeStampSequenceNumber;
    InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Seq Number for Drive=%s Written to Bitmap =%#I64x \n", __FUNCTION__, m_pVolumeContext->DriveLetter, SequenceNumber));
    KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);

    return SequenceNumber;
}

ULONGLONG 
BitmapPersistentValues::GetGlobalTimeStamp() // Return from DriverContext
{
    ULONGLONG TimeStamp;
    KIRQL   OldIrql;

    KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);
    TimeStamp = DriverContext.ullLastTimeStamp;
    KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);
    InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Time Stamp for Drive=%s Written to Bitmap=%#I64x \n", __FUNCTION__, m_pVolumeContext->DriveLetter, TimeStamp));

    return TimeStamp;
}


bool 
BitmapPersistentValues::GetVolumeFilteringState(bool *IsVolumeOnCluster) // Return from VolumeContext
{
    bool FilteringState = false, IsVolOnClus = false;
    KIRQL   OldIrql;

    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    if (m_pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
        FilteringState = true;
    }
    if (m_pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK) {
        if (IsVolumeOnCluster)
            IsVolOnClus = true;
    }
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);

    if (IsVolumeOnCluster) 
        *IsVolumeOnCluster = IsVolOnClus;

    return FilteringState;
}


ULONGLONG
BitmapPersistentValues::GetGlobalSequenceNumberFromBitmap() // Return from member variable
{
    return m_GlobalSequenceNumber;
}

ULONGLONG 
BitmapPersistentValues::GetGlobalTimeStampFromBitmap() // Return from member variable
{
    return m_GlobalTimeStamp;
}

bool 
BitmapPersistentValues::GetVolumeFilteringStateFromBitmap() // Return from member variable
{
    return m_bFilteringStateStopped;
}

VOID
BitmapPersistentValues::SetGlobalSequenceNumber(ULONGLONG Global_Sequence_Number) // Save to member variable
{
    m_GlobalSequenceNumber = Global_Sequence_Number;
    return;
}

bool 
BitmapPersistentValues::SetVolumeFilteringState(bool bFilteringStopped) // Save to member variable
{
    KIRQL   OldIrql;
    bool    bSavedToVolumeContext = true;
	
    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    if (m_pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK) {
        if (m_pVolumeContext->ulFlags & VCF_SAVE_FILTERING_STATE_TO_BITMAP) {
            bSavedToVolumeContext = false;
        } else {
            m_bFilteringStateStopped = bFilteringStopped;
        }
    }
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);

    return bSavedToVolumeContext;
}

VOID 
BitmapPersistentValues::SetGlobalTimeStamp(ULONGLONG Global_Time_Stamp) // Save to member variable
{
    m_GlobalTimeStamp= Global_Time_Stamp;
    return;
}

VOID
BitmapPersistentValues::SetResynRequiredValues(ULONG     ulOutOfSyncErrorCode,  // Save to member variable
                                               ULONG     ulOutOfSyncErrorStatus, 
                                               ULONG     Flags,
                                               ULONGLONG ullOutOfSyncTimeStamp,
                                               ULONG     ulOutOfSyncCount,
                                               ULONG     ulOutOfSyncIndicatedAtCount,
                                               ULONGLONG ullOutOfSyncIndicatedTimeStamp,
                                               ULONGLONG ullOutOfSyncResetTimeStamp)
{
    InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("%s : Called\n", __FUNCTION__));
    m_ulOutOfSyncErrorCode = ulOutOfSyncErrorCode;
    m_ulOutOfSyncErrorStatus = ulOutOfSyncErrorStatus;
    m_ullOutOfSyncTimeStamp = ullOutOfSyncTimeStamp;
    m_ulOutOfSyncCount = ulOutOfSyncCount;
    m_ulOutOfSyncIndicatedAtCount = ulOutOfSyncIndicatedAtCount ;
    m_ullOutOfSyncIndicatedTimeStamp = ullOutOfSyncIndicatedTimeStamp;
    m_ullOutOfSyncResetTimeStamp = ullOutOfSyncResetTimeStamp;

    if (Flags & BITMAP_RESYNC_REQUIRED)
        m_bResyncRequired =  true;

    return;
}

bool
BitmapPersistentValues::GetResynRequiredValues(ULONG     &ulOutOfSyncErrorCode, // Return from member variable
                                               ULONG     &ulOutOfSyncErrorStatus,
                                               ULONGLONG &ullOutOfSyncTimeStamp,
                                               ULONG     &ulOutOfSyncCount,
                                               ULONG     &ulOutOfSyncIndicatedAtCount,
                                               ULONGLONG &ullOutOfSyncIndicatedTimeStamp,
                                               ULONGLONG &ullOutOfSyncResetTimeStamp)

{
    ulOutOfSyncErrorCode = m_ulOutOfSyncErrorCode;
    ulOutOfSyncErrorStatus = m_ulOutOfSyncErrorStatus;
    ullOutOfSyncTimeStamp = m_ullOutOfSyncTimeStamp;
    ulOutOfSyncCount = m_ulOutOfSyncCount;
    ulOutOfSyncIndicatedAtCount = m_ulOutOfSyncIndicatedAtCount;
    ullOutOfSyncIndicatedTimeStamp = m_ullOutOfSyncIndicatedTimeStamp;
    ullOutOfSyncResetTimeStamp = m_ullOutOfSyncResetTimeStamp;

    InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("%s : Called returned resync required flag %d\n", __FUNCTION__, m_bResyncRequired));

    return m_bResyncRequired;

}

bool BitmapPersistentValues::IsClusterVolume()
{
    bool IsVolOnClus = false;
    if (m_pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK)
        IsVolOnClus = true;
    return IsVolOnClus;
}

ULONGLONG
BitmapPersistentValues::GetPrevEndTimeStamp()
{
    KIRQL OldIrql;
    ULONGLONG PrevEndTimeStamp;
    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    PrevEndTimeStamp = m_pVolumeContext->LastCommittedTimeStamp;
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);
    return PrevEndTimeStamp;
}

ULONGLONG
BitmapPersistentValues::GetPrevEndSequenceNumber()
{
    KIRQL OldIrql;
    ULONGLONG PrevEndSequenceNumber;

    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    PrevEndSequenceNumber = m_pVolumeContext->LastCommittedSequenceNumber;
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);
    return PrevEndSequenceNumber;
}

ULONG 
BitmapPersistentValues::GetPrevSequenceId()
{
    KIRQL OldIrql;
    ULONG PrevSequenceId;
    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    PrevSequenceId = m_pVolumeContext->LastCommittedSplitIoSeqId;
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);
    return PrevSequenceId;
}

VOID
BitmapPersistentValues::SetPrevEndTimeStamp(ULONGLONG PrevEndTimeStamp)
{
    KIRQL OldIrql;
    m_PrevEndTimeStamp = PrevEndTimeStamp;
    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    m_pVolumeContext->LastCommittedTimeStampReadFromStore = PrevEndTimeStamp;
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);
}

VOID
BitmapPersistentValues::SetPrevEndSequenceNumber(ULONGLONG PrevEndSequenceNumber)
{
    KIRQL OldIrql;
    m_PrevEndSequenceNumber = PrevEndSequenceNumber;
    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    m_pVolumeContext->LastCommittedSequenceNumberReadFromStore = (ULONGLONG)PrevEndSequenceNumber;
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);
}

VOID
BitmapPersistentValues::SetPrevSequenceId(ULONG PrevSeqId)
{
    KIRQL OldIrql;
    m_PrevSequenceId = PrevSeqId;
    KeAcquireSpinLock(&m_pVolumeContext->Lock, &OldIrql);
    m_pVolumeContext->LastCommittedSplitIoSeqIdReadFromStore = PrevSeqId;
    KeReleaseSpinLock(&m_pVolumeContext->Lock, OldIrql);
}

ULONGLONG
BitmapPersistentValues::GetPrevEndTimeStampFromBitmap()
{
    return m_PrevEndTimeStamp;
}

ULONGLONG
BitmapPersistentValues::GetPrevEndSequenceNumberFromBitmap()
{
    return m_PrevEndSequenceNumber;
}

ULONG
BitmapPersistentValues::GetPrevSequenceIdFromBitmap()
{
    return m_PrevSequenceId;
}

