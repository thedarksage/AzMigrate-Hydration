#define MAX_CMDS 200
#define MAX_VOLUMES 26
#define MAX_WRITE_THREADS 50

#define MAX_TAG_SIZE 512

#define DEFAULT_NWRITES_PER_TAG 200000

#define COMMAND_SUCCESS 1

#define IS_PARAM(s) (((s)[0])=='/')
#define IS_MAIN_CMD(s) ((s[0] == '-') && (s[1] == '-'))
#define IS_SUB_CMD(s) ((s[0]== '-') && (s[1] != '-'))
#define IS_DRIVE_LETTER(s) ((((s[0]>='A') && (s[0] <= 'Z')) || ((s[0]>='a') && (s[0] <= 'z'))) && (s[1]==':')&&(s[2]=='\0'))

#define KILO_BYTES 1024
#define MEGA_BYTES (1024*1024)
#define MAX_IO_SIZE (10*MEGA_BYTES)
#define TEST_SECTOR_SIZE 512

typedef enum __MAIN_CMD_TYPE {
	MAIN_CMD_SIMULATED_TEST,
	MAIN_CMD_RAPID_TEST,
	MAIN_CMD_WRITE_DATA,
	MAIN_CMD_WRITE_TAG,
	MAIN_CMD_PRINT_NEXT_DB,
	NMAIN_CMD_TYPES
} MAIN_CMD_TYPE;

typedef struct __WRITE_STREAM_PARAMS {
	ULONG ulNMaxIOs;
	ULONG ulMinIOSz; // no. of bytes
	ULONG ulMaxIOSz; // no. of bytes
	char  DriveLetter;
	ULONG	Random;
} WRITE_STREAM_PARAMS, *PWRITE_STREAM_PARAMS;

typedef struct __SIMULATED_TEST_CMD {
	ULONG				ulNWritesPerTag;
	ULONG				ulNWriteThreads;
	ULONG				bDataTest;
	WRITE_STREAM_PARAMS arstWriteStreamParams[MAX_WRITE_THREADS];
} SIMULATED_TEST_CMD, *PSIMULATED_TEST_CMD;

typedef struct __RAPID_TEST_CMD {
	WRITE_STREAM_PARAMS stWriteStreamParams;
} RAPID_TEST_CMD, *PRAPID_TEST_CMD;

typedef struct __WRITE_DATA_CMD {
	ULONG ulOffset;
	ULONG ulSize;
	ULONG ulPattern;
	char DriveLetter;
} WRITE_DATA_CMD, *PWRITE_DATA_CMD;

typedef struct __WRITE_TAG_CMD {
	ULONG DrivesBitmap;
	ULONG bAtomic;
	char  TagName[40];
} WRITE_TAG_CMD, *PWRITE_TAG_CMD;

typedef struct __PRINT_NEXT_DB_CMD {
	ULONG bCommit;
	ULONG bPrintTagIOs;
	ULONG bPrintDataIOs;
	ULONG bPrintPatterns;
	char DriveLetter;
} PRINT_NEXT_DB_CMD, *PPRINT_NEXT_DB_CMD;

typedef struct __MAIN_CMD {
	int type;
	union {
		SIMULATED_TEST_CMD stSimulatedTestCmd;
		RAPID_TEST_CMD	   stRapidTestCmd;
		WRITE_DATA_CMD	   stWriteDataCmd;
		WRITE_TAG_CMD	   stWriteTagCmd;
		PRINT_NEXT_DB_CMD  stPrintNextDBCmd;
	} subCmd;
} MAIN_CMD, *PMAIN_CMD;

typedef enum __PARAM_PARSE_ERROR {
	PARAM_CORRECT,
	VALUE_NOT_SPECIFIED,
	INVALID_VALUE_SPECIFIED,
	VALUE_OUT_OF_RANGE_SPECIFIED,
	SIZE_NOT_ALIGNED,
	MAX_PARAM_PARSE_ERROR
} PARAM_PARSE_ERROR;

typedef enum __PARAM_TYPE {
	PARAM_TYPE_INT,
	PARAM_TYPE_MEMORY,
	PARAM_TYPE_STRING,
	PARAM_TYPE_DRIVE_LETTER
} PARAM_TYPE;

int GetVolumeGUIDFromDriveLetter(char DriveLetter,WCHAR* volumeGUID,ULONG ulBufLen);
ULONG SendShutdownNotify(HANDLE hDriver,LPOVERLAPPED povl);
ULONG SendProcessStartNotify(HANDLE hDriver,LPOVERLAPPED povl);
ULONG GetNextDB(HANDLE hDriver,ULONGLONG ulTransId,WCHAR *volGuid,PUDIRTY_BLOCK pstDirtyBlock);
ULONG CommitDb(HANDLE hDriver,ULONGLONG ulTransId,WCHAR *volGuid);
ULONG ClearExistingData(HANDLE hDriver,WCHAR *volGUID);

int SimulatedTest(HANDLE hDriver, PSIMULATED_TEST_CMD pstWriteOrderTestParams);
int RapidTest(HANDLE hDriver, PRAPID_TEST_CMD pstRapidTestParams);
BOOLEAN FillDirtyBlockFromFile(PUDIRTY_BLOCK DirtyBlock);
void ClearDifferentials(HANDLE hDriver,WCHAR *volGUID);

void GenerateTagPrefixBuffer(char *buf,
							 ULONG *ulLen,
							 ULONG bAtomic,
							 int nVolumes,
							 WCHAR *volGuids);

int PrintNextDB(HANDLE hDriver,PPRINT_NEXT_DB_CMD pstPrintNextDBCmd);
int WriteData(HANDLE hDriver,PWRITE_DATA_CMD pstWriteDataCmd);
int WriteTag(HANDLE hDriver,PWRITE_TAG_CMD pstWriteTagCmd);
