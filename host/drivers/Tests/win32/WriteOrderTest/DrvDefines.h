#ifndef _INMAGE_DRIVER_DEFINES_H_
#define _INMAGE_DRIVER_DEFINES_H_

#define GUID_SIZE_IN_CHARS                      36  // Does not include NULL.

typedef enum _etBitOperation {
    ecBitOpNotDefined = 0,
    ecBitOpSet = 1,
    ecBitOpReset = 2,
} etBitOperation;

// Win2K and Win2K3 differences collected here
#if(_WIN32_WINNT > 0x0500)
    typedef volatile long tInterlockedLong;
#else
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

#endif //_INMAGE_DRIVER_DEFINES_H_

