
#ifndef _INMAGE_IOCTL_CONTROLLER_H
#define _INMAGE_IOCTL_CONTROLLER_H

#ifdef SV_WINDOWS
#ifdef INMAGEIOCTLCONTROLLER_EXPORTS
#define INMAGEIOCTLCONTROLLER_API __declspec(dllexport)
#else
#define INMAGEIOCTLCONTROLLER_API __declspec(dllimport)
#endif
#endif

#include "IPlatformAPIs.h"
#include "Logger.h"
#include "InmageTestException.h"

#include "InmFltInterface.h"
#include "InmFltIOCTL.h"

#include "svtypes.h"

#include "boost/shared_ptr.hpp"

#include <string>
#include <list>
#include <map>
#include <set>

#define GUID_SIZE_IN_BYTES      72

class INMAGEIOCTLCONTROLLER_API CInmageIoctlController
{
private:
    void*                                            s_hInmageCtrlDevice;
    IPlatformAPIs*                                    m_platformApis;
    ILogger*                                        m_pLogger;
#ifdef SV_WINDOWS
    boost::shared_ptr<Event>                        m_shutdownNotify;
    boost::shared_ptr<Event>                        m_processNotify;
    map<std::string, boost::shared_ptr<Event>>        m_distCrashEvent;
#endif
    std::vector<char>   GetSingleNodeTagBuffer(string tagContext, list<string> taglist, SV_ULONG ulFlags, int& totalSize);
    std::vector<char>   GetTagBuffer(int numDisks, list<string> taglist, int& totalSize);
    std::vector<char>   GetTagBuffer(string tagContext, list<string> taglist, SV_ULONG ulFlags, SV_ULONG timeout, int& totalSize);

public:
    CInmageIoctlController(IPlatformAPIs *platformApis, ILogger *pLogger);
    ~CInmageIoctlController();

    void ServiceShutdownNotify(SV_ULONG ulFlags);
    void ProcessStartNotify(SV_ULONG ulFlags);

    bool LocalCrashConsistentTagIoctl(string tagContext, list<string> taglist, SV_ULONG ulFlags, SV_ULONG ulTimeoutInMS);

    void StartFiltering(std::string deviceId);
    void StopFiltering(std::string deviceId, bool delBitmapFile, bool bStopAll);
    void ResyncStartNotification(std::string deviceId);
    void ResyncEndNotification(std::string deviceId);
    void ClearDifferentials(std::string deviceId);
    void GetDriverVersion(void *lpOutBuffer, SV_ULONG nOutBufferSize);
    void GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans, PUDIRTY_BLOCK_V2  pDirtyBlock, bool bThowIfResync, std::string deviceId);
    void CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction);
    void RegisterForSetDirtyBlockNotify(std::string scDeviceId, const void*    hEvent);
    void HoldWrites(string tagContext, SV_ULONG ulFlags, SV_ULONG ulTimeoutInMS);
    void InsertDistTag(string tagContext, SV_ULONG ulFlags, list<string> taglist);
    void ReleaseWrites(string tagContext, SV_ULONG ulFlags);
    void CommitTag(string tagContext, SV_ULONG ulFlags);
    void GetLcn(string fileName, void* retBuff, int size, SV_LONGLONG   llStartingVcn);
    void SetDriverFlags(SV_ULONG ulFlags, etBitOperation flag);
    void SetDriverConfig(DRIVER_CONFIG DriverInConfig, DRIVER_CONFIG &DriverOutConfig);
    void SetReplicationState(PVOID buffer, int size);
    void GetReplicationStats(std::string deviceId, REPLICATION_STATS& stats);

    DWORD AppConsistentTagIoctl(std::list<std::string> diskIds, std::string tagContext, std::list<std::string> taglist, SV_ULONG ulFlags, SV_ULONG ulTimeoutInMS);
    DWORD AppConsistentTagCommitRevokeIoctl(std::string tagContext, SV_ULONG ulFlags, std::list<std::string> diskIds, bool commit, std::vector<SV_ULONG>& output);

    DWORD AppConsistentTagIoctl(void* input, SV_ULONG ulInSize, void *output, SV_ULONG ulOutSize, SV_ULONG& ulOutBytes);
    DWORD AppConsistentTagCommitRevokeIoctl(void* input, SV_ULONG ulInSize, void *output, SV_ULONG ulOutSize, SV_ULONG& ulOutBytes);

    std::vector<char>   GetAppTagBuffer(list<string> diskIds, string tagContext, list<string> taglist, SV_ULONG ulFlags, SV_ULONG timeout, SV_ULONG& totalSize);
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

        StringCchPrintfA(m_exceptionMsg, EXCEPTION_MSG_BUFFER_LENGTH, "Resync required flag is set \
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
