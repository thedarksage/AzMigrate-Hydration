
#include <windows.h>
#include <ntddscsi.h>
#include <stdio.h>
#include <tchar.h>

#define SPT_SENSE_LENGTH 32
#define SPTWB_DATA_LENGTH 64
#define PARAM_DATA_LENGTH 24
#define RES_KEY_LENGTH 8

#define DISK     _T("-disk")

#define TUR      _T("tur")
#define REGISTER _T("register")
#define READALL  _T("readkeys")
#define READRES  _T("readres")
#define CLEAR    _T("scsi3clear")
#define USAGE    _T("/?")
#define DEBUG    _T("debug")
#define WRITABLE _T("writable")

typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS {
    SCSI_PASS_THROUGH spt;
    ULONG             Filler;      // realign buffers to double word boundary
    UCHAR             ucSenseBuf[SPT_SENSE_LENGTH];
    UCHAR             ucDataBuf[SPTWB_DATA_LENGTH];
    } SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;

VOID PrintStatusResults(BOOL, DWORD, PSCSI_PASS_THROUGH_WITH_BUFFERS, ULONG);
VOID PrintSenseInfo(PSCSI_PASS_THROUGH_WITH_BUFFERS);
VOID PrintDataBuffer(PUCHAR DataBuffer, ULONG BufferLength);
VOID GetLastErrToStr(DWORD err);

// TODO: reference additional headers your program requires here
