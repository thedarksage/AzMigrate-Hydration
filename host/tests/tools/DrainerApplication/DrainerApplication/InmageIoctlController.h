
#ifndef _INMAGE_IOCTL_CONTROLLER_H
#define _INMAGE_IOCTL_CONTROLLER_H

#ifdef WINDOWS
#ifdef INMAGEIOCTLCONTROLLER_EXPORTS
#define INMAGEIOCTLCONTROLLER_API __declspec(dllexport)
#else
#define INMAGEIOCTLCONTROLLER_API __declspec(dllimport)
#endif
#endif


#include "InmFltInterface.h"
#include "InmFltIOCTL.h"

#include <Windows.h>
#include <strsafe.h>

#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <exception>

#define GUID_SIZE_IN_BYTES      72

class  CInmageTestException : public std::exception
{
protected:
    char	m_exceptionMsg[2048];

public:
    virtual const char* what() const throw()
    {
        return m_exceptionMsg;
}

};
class CAgentException : public CInmageTestException
{
public:
    CAgentException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        vsnprintf_s(m_exceptionMsg, sizeof(m_exceptionMsg), 2048, format, argptr);
        va_end(argptr);
    }
};

class __declspec(dllexport) CInmageIoctlController
{
private:
    void*       s_hInmageCtrlDevice;
    HANDLE      m_processNotify;
    HANDLE      m_shutdownNotify;
#ifdef WINDOWS
    boost::shared_ptr<Event>                        m_shutdownNotify;
    boost::shared_ptr<Event>                        m_processNotify;
    map<std::string, boost::shared_ptr<Event>>        m_distCrashEvent;
#endif
    std::vector<char>   GetSingleNodeTagBuffer(std::string tagContext, 
                                               std::list<std::string> tagslist,
                                               ULONG ulFlags, 
                                                int& totalSize);
    std::vector<char>   GetTagBuffer(int numDisks, std::list<std::string> tagslist, int& totalSize);
    std::vector<char>   GetTagBuffer(std::string tagContext, std::list<std::string> tagslist, ULONG ulFlags, ULONG timeout, int& totalSize);

public:
    CInmageIoctlController();
    ~CInmageIoctlController();

    void ServiceShutdownNotify(ULONG ulFlags);
    void ProcessStartNotity(ULONG ulFlags);

    bool LocalCrashConsistentTagIoctl(std::string tagContext, std::list<std::string> tagslist, ULONG ulFlags, ULONG ulTimeoutInMS);

    void StartFiltering(std::string deviceId);
    void StopFiltering(std::string deviceId, bool delBitmapFile, bool bStopAll);
    void ResyncStartNotification(std::string deviceId);
    void ResyncEndNotification(std::string deviceId);
    void ClearDifferentials(std::string deviceId);
    void GetDriverVersion(void *lpOutBuffer, ULONG nOutBufferSize);
    void GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans, PUDIRTY_BLOCK_V2  pDirtyBlock, bool bThowIfResync, std::string deviceId);
    void CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction);
    void RegisterForSetDirtyBlockNotify(std::string scDeviceId, const void*    hEvent);
    //void HoldWrites(std::string tagContext, ULONG ulFlags, ULONG ulTimeoutInMS);
    void InsertDistTag(std::string tagContext, ULONG ulFlags, std::list<std::string> tagslist);
    void ReleaseWrites(std::string tagContext, ULONG ulFlags);
    void CommitTag(std::string tagContext, ULONG ulFlags);
    void GetLcn(std::string fileName, void* retBuff, int size);
    void SetDriverFlags(ULONG ulFlags, etBitOperation flag);
    void SetDriverConfig(DRIVER_CONFIG DriverInConfig, DRIVER_CONFIG &DriverOutConfig);
    void SetReplicationState(PVOID buffer, int size);
    void GetReplicationStats(std::string deviceId, REPLICATION_STATS& stats);

    DWORD AppConsistentTagIoctl(std::list<std::string> diskIds, std::string tagContext, std::list<std::string> tagslist, ULONG ulFlags, ULONG ulTimeoutInMS);
    DWORD AppConsistentTagCommitRevokeIoctl(std::string tagContext, ULONG ulFlags, std::list<std::string> diskIds, bool commit, std::vector<ULONG>& output);

    DWORD AppConsistentTagIoctl(void* input, ULONG ulInSize, void *output, ULONG ulOutSize, ULONG& ulOutBytes);
    DWORD AppConsistentTagCommitRevokeIoctl(void* input, ULONG ulInSize, void *output, ULONG ulOutSize, ULONG& ulOutBytes);

    std::vector<char>   GetAppTagBuffer(std::list<std::string> diskIds, std::string tagContext, std::list<std::string> tagslist, ULONG ulFlags, ULONG timeout, ULONG& totalSize);
};

class CAgentResyncRequiredException : public CInmageTestException
{
public:
    CAgentResyncRequiredException(PUDIRTY_BLOCK_V2 pDirtyBlock)
    {
        TIME_ZONE_INFORMATION   TimeZone;
        SYSTEMTIME              SystemTime;
        SYSTEMTIME  OutOfSyncTime;
        GetTimeZoneInformation(&TimeZone);
        FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uHdr.Hdr.liOutOfSyncTimeStamp, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &OutOfSyncTime);

        StringCchPrintfA(m_exceptionMsg, 2048, "Resync required flag is set \
                                                                                                                          Resync count is 0x%#x \
                                                                                                                                                                              Resync Error Code = %#x \
                                                                                                                                                                                                                                  TimeStamp of resync request : %02d/%02d/%04d %02d:%02d:%02d:%04d \
                                                                                                                                                                                                                                                                                      Error String is \"%s\"",
            pDirtyBlock->uHdr.Hdr.ulOutOfSyncCount,
            pDirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode,
            OutOfSyncTime.wMonth, OutOfSyncTime.wDay, OutOfSyncTime.wYear,
            OutOfSyncTime.wHour, OutOfSyncTime.wMinute, OutOfSyncTime.wSecond, OutOfSyncTime.wMilliseconds,
            pDirtyBlock->uHdr.Hdr.ErrorStringForResync);


    }
};


#endif