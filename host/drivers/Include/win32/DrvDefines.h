#ifndef _INMAGE_DRIVER_DEFINES_H_
#define _INMAGE_DRIVER_DEFINES_H_

#define GUID_SIZE_IN_CHARS                      36  // Does not include NULL.
#define GUID_SIZE_IN_BYTES                      0x48 // 72 bytes

#define GUID_OFFSET                             11 
#define MAX_NUM_DISKS_SUPPORTED                 256

typedef enum _etBitOperation {
    ecBitOpNotDefined = 0,
    ecBitOpSet = 1,
    ecBitOpReset = 2,
} etBitOperation;

// Win2K and Win2K3 differences collected here
#if(_WIN32_WINNT > 0x0500)
    typedef volatile long tInterlockedLong;
#else
// Win2K only
// 0x0500 = Win2K
// 0x0501 = WinXP
// 0x0502 = Win2K3
// 0x0600 = Vista
    typedef long tInterlockedLong;

#define RtlInitEmptyUnicodeString(_ucStr,_buf,_bufSize) \
    ((_ucStr)->Buffer = (_buf), \
     (_ucStr)->Length = 0, \
     (_ucStr)->MaximumLength = (USHORT)(_bufSize))

#endif

#ifndef  STATUS_CONTINUE_COMPLETION //required to build driver in Win2K and XP build environment
    #define STATUS_CONTINUE_COMPLETION      STATUS_SUCCESS
#endif

#define WIDEN2(x)       L ## x
#define CHARSTRING_TO_WCHARSTRING(x) WIDEN2(x)
#define __WFILE__ CHARSTRING_TO_WCHARSTRING(__FILE__)
//wchar_t *gs_pwszFile = __WFILE__;


// calculate system time units from value expressed in different time units
#define MICROSECONDS    ((LONGLONG)(10))
#define MILLISECONDS    ((LONGLONG)(1000 * MICROSECONDS))
#define SECONDS         ((LONGLONG)(1000 * MILLISECONDS))
#define MINUTES         ((LONGLONG)(60   * SECONDS))
#define HOURS           ((LONGLONG)(60   * MINUTES))

#define MICROSECOND_RELATIVE(_interval)     ((LONGLONG)(_interval * ((LONGLONG)-1)))
#define MILLISECONDS_RELATIVE(_interval)    ((LONGLONG)(_interval) * MILLISECONDS * ((LONGLONG)-1))
#define SECONDS_RELATIVE(_interval)         ((LONGLONG)(_interval) * SECONDS * ((LONGLONG)-1))
#define MINUTES_RELATIVE(_interval)         ((LONGLONG)(_interval) * MINUTES * ((LONGLONG)-1))
#define HOURS_RELATIVE(_interval)           ((LONGLONG)(_interval) * HOURS * ((LONGLONG)-1))

#endif //_INMAGE_DRIVER_DEFINES_H_

