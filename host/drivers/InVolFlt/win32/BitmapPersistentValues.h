#ifndef BitmapPersistentValues_INCLUDED
#define BitmapPersistentValues_INCLUDED

#include "global.h"
#include "VContext.h"

class BitmapPersistentValues {

public:
    BitmapPersistentValues(PVOLUME_CONTEXT pVolume_Context);
    NTSTATUS ReadPersistentValues(PVOID pm_BitmapHeader);
    ULONGLONG GetGlobalSequenceNumber(); //Return from DriverContext
    ULONGLONG GetGlobalTimeStamp(); // Return from DriverContext
    // Returns true if Volume filtering is stopped.
    // Returns false if Volume filteirng is started.
    bool GetVolumeFilteringState(bool *IsVolumeOnCluster = NULL); // Return from VolumeContext
    ULONGLONG GetGlobalSequenceNumberFromBitmap(); // Return from member variable
    ULONGLONG GetGlobalTimeStampFromBitmap(); // Return from member variable
    bool GetVolumeFilteringStateFromBitmap(); // Return from member variable
    VOID SetGlobalSequenceNumber(ULONGLONG Global_Sequence_Number); // Save to member variable
    // SetVolumeFilteringState returns false, if fails to save the filtering state.
    // Filtering state is not saved and returned false, if volume context has a flag to save the state
    // to bitmap file, overriding the normal case of reading the state from bitmap file.
    bool SetVolumeFilteringState(bool bFilteringStopped); // Save to member variable
    VOID SetGlobalTimeStamp(ULONGLONG Global_Time_Stamp); // Save to member variable

    VOID SetResynRequiredValues(ULONG     ulOutOfSyncErrorCode,  // Save to member variable
                                ULONG     ulOutOfSyncErrorStatus, 
                                ULONG     Flags,
                                ULONGLONG ullOutOfSyncTimeStamp,
                                ULONG     ulOutOfSyncCount,
                                ULONG     ulOutOfSyncIndicatedAtCount,
                                ULONGLONG ullOutOfSyncIndicatedTimeStamp,
                                ULONGLONG ullOutOfSyncResetTimeStamp);

    bool GetResynRequiredValues(ULONG     &ulOutOfSyncErrorCode,  //return from member variable
                                ULONG     &ulOutOfSyncErrorStatus,
                                ULONGLONG &ullOutOfSyncTimeStamp,
                                ULONG     &ulOutOfSyncCount,
                                ULONG     &ulOutOfSyncIndicatedAtCount,
                                ULONGLONG &ullOutOfSyncIndicatedTimeStamp,
                                ULONGLONG &ullOutOfSyncResetTimeStamp);
    bool IsClusterVolume();
    ULONGLONG GetPrevEndTimeStamp(); // Return from VolumeContext

ULONGLONG
    GetPrevEndSequenceNumber(); // Return from VolumeContext
    ULONG GetPrevSequenceId();  // Return from VolumeContext 
    VOID SetPrevEndTimeStamp(ULONGLONG PrevEndTimeStamp);
    VOID SetPrevEndSequenceNumber(ULONGLONG PrevEndSequenceNumber);
    VOID SetPrevSequenceId(ULONG PrevSeqId);
    ULONGLONG GetPrevEndTimeStampFromBitmap();

ULONGLONG
    GetPrevEndSequenceNumberFromBitmap();

    ULONG GetPrevSequenceIdFromBitmap();

    PVOLUME_CONTEXT m_pVolumeContext;

protected:
    bool        m_bFilteringStateStopped;
    ULONGLONG   m_GlobalTimeStamp;
    ULONGLONG   m_GlobalSequenceNumber;
    ULONG       m_ulOutOfSyncErrorCode;
    ULONG       m_ulOutOfSyncErrorStatus;
    bool        m_bResyncRequired;
    ULONGLONG   m_ullOutOfSyncTimeStamp;
    ULONG       m_ulOutOfSyncCount;
    ULONG       m_ulOutOfSyncIndicatedAtCount;
    ULONGLONG   m_ullOutOfSyncIndicatedTimeStamp;
    ULONGLONG   m_ullOutOfSyncResetTimeStamp;
    ULONGLONG   m_PrevEndTimeStamp;
    ULONGLONG   m_PrevEndSequenceNumber;
    ULONG       m_PrevSequenceId;
};


#endif
