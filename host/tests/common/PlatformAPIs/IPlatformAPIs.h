///
///  \file  IPlatformAPIs.h
///
///  \brief contains PlatformAPIs interface
///


#ifndef IPLATFORMAPIS_H
#define IPLATFORMAPIS_H

#include "svtypes.h"

#ifdef SV_UNIX
#include "boost/thread.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/condition.hpp"
#endif

#include "common.h"
#include "PlatformAPIsMajor.h"

#include "inmsafecapis.h"

#include <string>
#ifdef SV_WINDOWS
#define TIMEOUT_INFINITE (-1)
#else
#define TIMEOUT_INFINITE (0xFFFFFFFF)
#endif

#define MAX_STRING_SIZE         255

#define ROUND_OFF_UPPER(_a,_b)  ((((_a)+((_b)-1))/(_b))*(_b))
#define ROUND_OFF(_a,_b)    (((_a)/(_b))*(_b))

#define SafeDelete(x)   if (NULL != x) {delete x; x = NULL;}
#define SafeFree(x)     if (NULL != x) {free(x); x = NULL;}

#define TEST_FLAG(_a, _f)   ((_f) == ((_a) & (_f)))

enum WAIT_STATE
{
    WAIT_SUCCESS = 0,
    WAIT_TIMED_OUT = 1,
    WAIT_FAILURE = 2
};

class ExtentInfo{
public:
    SV_LONGLONG     m_liStartOffset;
    SV_LONGLONG     m_liEndOffset;

    ExtentInfo(SV_LONGLONG liStartOffset, SV_LONGLONG liEndOffset) :
        m_liStartOffset(liStartOffset),
        m_liEndOffset(liEndOffset)
    {}

};

class PLATFORMAPIS_API IPlatformAPIs
{
public:
    virtual bool DeviceIoControlSync(
        HANDLE     hDevice,
        SV_ULONG   ulIoControlCode,
        void*      lpInBuffer,
        SV_ULONG   nInBufferSize,
        void*      lpOutBuffer,
        SV_ULONG   nOutBufferSize,
        SV_ULONG*  lpBytesReturned) = 0;

    virtual bool DeviceIoControl(
        HANDLE     hDevice,
        SV_ULONG   ulIoControlCode,
        void*      lpInBuffer,
        SV_ULONG   nInBufferSize,
        void*      lpOutBuffer,
        SV_ULONG   nOutBufferSize,
        SV_ULONG*  lpBytesReturned,
        void*      overlapped) = 0;

    virtual bool Read(HANDLE hDevice,
        void* buffer, SV_ULONG ulBytesToRead, SV_ULONG& ulBytesRead) = 0;
    virtual bool Write(HANDLE hDevice,
        const void* buffer, SV_ULONG ulBytesToBytes, SV_ULONG& ulBytesWritten) = 0;
    virtual bool SeekFile(HANDLE hDevice,
        FileMoveMethod dwMoveMethod, SV_ULONGLONG offset) = 0;
    virtual HANDLE Open(std::string szDevice, SV_ULONG access,
        SV_ULONG mode, SV_ULONG creationDeposition, SV_ULONG flags) = 0;
    virtual HANDLE OpenDefault(std::string szDevice) = 0;
    virtual bool Close(HANDLE hDevice) = 0;
    virtual bool SafeClose(HANDLE hDevice) = 0;
    virtual SV_ULONG GetLastError() = 0;
    virtual bool PathExists(const char* path) = 0;
    virtual bool IsValidHandle(HANDLE handle) = 0;
#ifdef SV_WINDOWS
    virtual void GetVolumeMappings(HANDLE hDevice, SV_LONGLONG llStartingVcn,
        void* retBuffer, SV_ULONG ulBufferSize) = 0;
#else
    virtual bool Fsync(HANDLE hDevice) = 0;
#endif
};

class PLATFORMAPIS_API Event
{
public:
    Event(bool bInitialState, bool bAutoReset, const std::string& sName = "");
    Event(bool bInitialState, bool bAutoReset, const char *name, int iEventType);
    ~Event();
    bool operator()(int secs);
    void Signal(bool);
    const bool IsSignalled() const;
    const void* Self() const;
    const std::string& GetEventName() const;
    void SetEventName(const std::string&);
    WAIT_STATE Wait(long int, long int liMilliSeconds = 0);

#ifdef SV_UNIX
    void ResetEvent();
#endif
private:
    Event();
    void Close();
private:
    void* m_hEvent;
    bool m_bSignalled;
    bool m_bAutoReset;
    bool m_bInitialState;
    std::string m_sEventName;
#ifdef SV_UNIX
    boost::mutex m_mutex;
    boost::condition m_condition;
#endif
};

#endif /* IPLATFORMAPIS_H */

