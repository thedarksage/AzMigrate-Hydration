#define MAX_VOLUMES 26
#define MAX_WRITE_THREADS 50
#define MAX_ACTIVE_IOS_PER_VOLUME 2048
#define INVALID_SEQ_NUM (MAX_ACTIVE_IOS_PER_VOLUME+1)

#define TAG_OR_START_OF_DISK_CHANGE 0
#define PART_OR_END_OF_SPLIT_DISK_CHANGE 1

#define UDIRTY_BLOCK_FLAG_SPLIT_CHANGE (UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE | \
                         UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE | \
                         UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE)               

#define DEFAULT_MAX_NIOS (32*1024)
#define DEFAULT_MIN_IO_SIZE 512
#define DEFAULT_MAX_IO_SIZE (2*1024*1024) //2mb
#define DEFAULT_NSECS_TO_RUN (10*60) // 5 minutes
#define DEFAULT_NWRITES_PER_TAG 2

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
	ULONG		 arstIOsWaitingForCommit[MAX_ACTIVE_IOS_PER_VOLUME];
	ULONG		 ulNIOsWaitingForCommit;
	HANDLE		 hActiveIOsMutex;
	char		 DriveLetter;
} PER_VOLUME_INFO, *PPER_VOLUME_INFO;

typedef struct _WRITE_STREAM_PARAMS {
	ULONG ulNMaxIOs;
	ULONG ulNSecsToRun;
	ULONG ulMinIOSz; // no. of bytes
	ULONG ulMaxIOSz; // no. of bytes
	char  DriveLetter;
} WRITE_STREAM_PARAMS, *PWRITE_STREAM_PARAMS;

typedef struct __WRITE_THREAD_PARAMS {
	WCHAR				strVolumeSymbolicLinkName[100];
	ULONG				ulNWritesPerTag;
	int					ulThreadIndex;
	ULONG				ulNMaxIOs;
	ULONG				ulNSecsToRun;
	ULONG				ulMinIOSz; // no. of blocks
	ULONG				ulMaxIOSz; // no. of blocks
	PPER_VOLUME_INFO	pstPerVolumeInfo;
} WRITE_THREAD_PARAMS, *PWRITE_THREAD_PARAMS;

typedef enum {
	THREAD_STATUS_NONE,
	THREAD_STATUS_RUNNING,
	THREAD_STATUS_FROZEN
} WRITE_THREAD_STATUS;

typedef struct __WRITE_ORDER_TEST_PARAMS {
	HANDLE				hFilterDriver;
	ULONG				ulNWriteThreads;
	WRITE_STREAM_PARAMS arstWriteStreamParams[MAX_WRITE_THREADS];
} WRITE_ORDER_TEST_PARAMS, *PWRITE_ORDER_TEST_PARAMS;

typedef struct __READ_THREAD_STATE_INFO {
	ULONG				ulState;
	ULONG				ulPattern;
	PPER_VOLUME_INFO	pstVolumeInfo;
	LONGLONG			ullByteOffset;
	ULONG				ulLen;
    ULONG               ulCurIOSeqNum;
} READ_THREAD_STATE_INFO, *PREAD_THREAD_STATE_INFO;

typedef struct __WRITE_ORDER_TEST_INFO {
    ULONG		        bInConsistent;
    char                *strLogFileName;
	HANDLE				hFilterDriver;
	ULONG				arActiveVolumes[MAX_VOLUMES];
	ULONG				nVolumes;
	ULONG				bFrozen;
	PER_VOLUME_INFO		arstPerVolumeInfo[MAX_VOLUMES];
	WRITE_THREAD_PARAMS arstWriteThreadParams[MAX_WRITE_THREADS];
	WRITE_THREAD_STATUS	earrWriteThreadStatus[MAX_WRITE_THREADS];
	HANDLE				hWriteThreadsMutex; // A thread has to acquire this to issue a tag
	char				TagBufferPrefix[500];
	ULONG				TagBufferPrefixLength;
} WRITE_ORDER_TEST_INFO,*PWRITE_ORDER_TEST_INFO;

typedef struct _SHUTDOWN_NOTIFY_DATA {
#define SND_FLAGS_COMMAND_SENT      0x0001
#define SND_FLAGS_COMMAND_COMPLETED 0x0002
    HANDLE          hVFCtrlDevice;    // This is the handle of the driver, passed to SendShutdownNotify.
    HANDLE          hEvent;     // This is the event passed in overlapped IO for completion notification.
    ULONG           ulFlags;    // Flags to tack the state.
    OVERLAPPED      Overlapped; // This is used to send overlapped IO to driver for shtudown notification.
} SHUTDOWN_NOTIFY_DATA, *PSHUTDOWN_NOTIFY_DATA;

typedef struct _SERVICE_START_NOTIFY_DATA {
#define SERVICE_START_ND_FLAGS_COMMAND_SENT         0x0001
#define SERVICE_START_ND_FLAGS_COMMAND_COMPLETED    0x0002
    HANDLE          hVFCtrlDevice;      // This is the handle of the driver, passed to SendServiceStartNotify
    HANDLE          hEvent;             // This is the event passed in overlapped IO for completion notification.
    ULONG           ulFlags;            // Flags to tracks state
    OVERLAPPED      Overlapped;         // This is used to send overlapped IO to driver for service start notification.
} SERVICE_START_NOTIFY_DATA, *PSERVICE_START_NOTIFY_DATA;


LONGLONG GenRandOffset(LONGLONG max_offset);
ULONG GenRandSize(ULONG max_size);
ULONG GenRandPattern();