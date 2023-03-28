#define MAX_ACTIVE_IOS_PER_VOLUME 20000
#define INVALID_SEQ_NUM (MAX_ACTIVE_IOS_PER_VOLUME+1)

#define TAG_OR_START_OF_DISK_CHANGE 0
#define PART_OR_END_OF_SPLIT_DISK_CHANGE 1

#define UDIRTY_BLOCK_FLAG_SPLIT_CHANGE (UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE | \
										UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE | \
										UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE)

#define MAX_OF_2(a,b) (((a)>(b)) ? (a):(b))
#define MAX_OF_3(a,b,c) ((MAX_OF_2(a,b) > (c)) ? MAX_OF_2(a,b) : (c))

#define MIN_OF_2(a,b) (((a)<(b)) ? (a):(b))
#define MIN_OF_3(a,b,c) ((MIN_OF_2(a,b) < (c)) ? MIN_OF_2(a,b) : (c))

#define ERROR_LOG_FILE_NAME "c:\\results.txt"

typedef struct __PER_IO_INFO {
	ULONG		 ulIOSeqNum;
	ULONG		 ulThreadId;
	ULONG		 bTag;
	LONGLONG	 ullOffset;
	ULONG		 ulLen;
	ULONG		 ulPattern;
	ULONG		 bArrived;
} PER_IO_INFO, *PPER_IO_INFO;

typedef struct __PER_VOLUME_INFO {
    WCHAR        volumeGUID[GUID_SIZE_IN_CHARS+1];
	ULONG		 bReadStarted;
	ULONG		 ulNActiveIOs;
	ULONG		 ulStartActiveIOIndex;
	ULONG		 ulNextActiveIOIndex;
	PER_IO_INFO  arstActiveIOs[MAX_ACTIVE_IOS_PER_VOLUME];
	HANDLE		 hActiveIOsMutex;
	char		 DriveLetter;
    HANDLE       VolumeLock;
	ULONGLONG    BytesWritten;
	ULONGLONG    BytesRead;
    ULONGLONG    DataThresholdPerVolume;
	ULONG        TestStartTime;
	ULONG        LastModeChangeTime;
	ULONG		 InputDataRate;
	bool         bFrozen;

} PER_VOLUME_INFO, *PPER_VOLUME_INFO;

typedef struct __WRITE_THREAD_PARAMS {
	WCHAR				strVolumeSymbolicLinkName[100];
	ULONG				ulNWritesPerTag;
	int					ulThreadIndex;
	ULONG				ulNMaxIOs;
	ULONG				ulMinIOSz; // no. of blocks
	ULONG				ulMaxIOSz; // no. of blocks
	ULONG				bDataTest;
	PPER_VOLUME_INFO	pstPerVolumeInfo;
	ULONG				LastWrittenOffset;
	ULONG				Sequential;
} WRITE_THREAD_PARAMS, *PWRITE_THREAD_PARAMS;

typedef enum {
	THREAD_STATUS_NONE,
	THREAD_STATUS_RUNNING,
	THREAD_STATUS_FROZEN
} WRITE_THREAD_STATUS;

typedef enum __EXIT_REASON {
	EXIT_SUCCESFUL_COMPLETION=1,
	EXIT_GET_DBS_NOT_STARTED,
	EXIT_IO_OUT_OF_ORDER,
	EXIT_OFFSET_MISMATCH,
	EXIT_IO_DATA_CORRUPTED,
	EXIT_NOT_ENOUGH_BUFFERS_FOR_THE_SIZE,
	EXIT_EXPECTING_START_OF_CHANGE,
	EXIT_EXPECTING_PART_OR_END_OF_CHANGE,
	EXIT_MORE_DISK_CHANGES_IN_SPLIT_DB,
	EXIT_SPLIT_CHANGE_OVER_IN_FIRST_DB,
	EXIT_SPLIT_CHANGE_OVER_IN_PART_DB,
	EXIT_SPLIT_CHANGE_NOT_OVER_IN_END_DB,
	EXIT_CHANGE_NOT_COMPLETE,
	EXIT_TIMEOUT_WAITING_FOR_A_DB_WITH_CHANGES,
	EXIT_MODE_CHANGED_TO_BITMAP_OR_META_DATA,
	EXIT_FAILURE_GETTING_DB,
	EXIT_MAX_REASONS
} EXIT_REASON;

typedef struct __READ_THREAD_STATE_INFO {
	ULONG				ulState;
	ULONG				ulPattern;
	PPER_VOLUME_INFO	pstVolumeInfo;
	LONGLONG			ullByteOffset;
	ULONG				ulLen;
    ULONG               ulCurIOSeqNum;
	EXIT_REASON			ulExitReason;
} READ_THREAD_STATE_INFO, *PREAD_THREAD_STATE_INFO;

typedef struct __WRITE_ORDER_TEST_INFO {
    ULONG		        bInConsistent;
	ULONG				nWriteThreads;
    char                *strLogFileName;
	HANDLE				hFilterDriver;
	ULONG				arActiveVolumes[MAX_VOLUMES];
	ULONG				nVolumes;
	ULONG				bFrozen;
	PER_VOLUME_INFO		arstPerVolumeInfo[MAX_VOLUMES];
	WRITE_THREAD_PARAMS arstWriteThreadParams[MAX_WRITE_THREADS];
	WRITE_THREAD_STATUS	earrWriteThreadStatus[MAX_WRITE_THREADS];
	ULONGLONG			ullTotalData[MAX_WRITE_THREADS];
	HANDLE				hWriteThreadsMutex; // A thread has to acquire this to issue a tag
	char				TagBufferPrefix[500];
	ULONG				TagBufferPrefixLength;
    ULONG               ControlledTest;
    bool                ExitTest;
    CHAR                ChangeEventLogFileName[128];
    FILE                *ChangeEventLogFilePointer;
    ULONG               DataRateTuningStep;
    ULONG               DataThresholdForVolume;
	ULONG				InputDataRate;
} WRITE_ORDER_TEST_INFO,*PWRITE_ORDER_TEST_INFO;
