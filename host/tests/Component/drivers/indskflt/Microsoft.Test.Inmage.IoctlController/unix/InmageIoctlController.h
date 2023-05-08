// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the INMAGEAGENT_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// INMAGEAGENT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifndef _INMAGE_IOCTL_CONTROLLER_H
#define _INMAGE_IOCTL_CONTROLLER_H

#ifdef SV_WINDOWS
#ifdef INMAGEIOCTLCONTROLLER_EXPORTS
#define INMAGEIOCTLCONTROLLER_API __declspec(dllexport)
#else
#define INMAGEIOCTLCONTROLLER_API __declspec(dllimport)
#endif
#else
#define INMAGEIOCTLCONTROLLER_API
#endif


#include <string>
#include <list>

#include "involflt.h"
#include "boost/thread/mutex.hpp"

#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "event.h"

#include <ace/Process_Manager.h>
#include "PlatformAPIs.h"
#include "Logger.h"

typedef char CHAR;
typedef unsigned long ULONG;

typedef long LONG;
typedef CHAR *PCHAR, *LPCH, *PCH;



#define SAFE_DELETE(x)  if (NULL != x) {delete x; x = NULL;}

class INMAGEIOCTLCONTROLLER_API  CInmageIoctlController
{
private:
    bool                        m_instanceFlag;
    CInmageIoctlController*     s_instance;

    HANDLE                      s_hInmageCtrlDevice;
    IPlatformAPIs*              m_platformApis;
    ILogger*                    m_pLogger;
    volatile static LONG        s_numInstances;
    boost::shared_ptr<Event>    m_shutdownNotify;
    boost::mutex                s_InstaceLock;
    boost::thread               m_startNotifyThread;
    PCHAR GetSingleNodeTagBuffer(string tagContext,
        list<string> taglist, ULONG ulFlags, int& totalSize);
    PCHAR GetTagBuffer(int numDisks, list<string> taglist, int& totalSize);
    void  IssueStopFilteringIoctl(std::string deviceId, bool delBitmapFile, bool bStopAll);
    int   StopFilteringOnAllProtectedDevices();
    void  TrimNewLineChars(const std::string &strDevs, std::vector<std::string>& vDevices);

public: 
    CInmageIoctlController(IPlatformAPIs *platformApis, ILogger *pLogger);
    SV_ULONG GetDataBufferAllocationLength(SV_ULONG ulDataLength);
    char *BuildStreamRecordHeader(char *dataBuffer,
        SV_ULONG &ulPendingBufferLength, SV_UINT usTag, SV_ULONG ulDataLength);
    char *BuildStreamRecordData(char *dataBuffer,
        SV_ULONG &ulPendingBufferLength, void *pData, SV_ULONG ulDataLength);
    char* GetTagBuffer(std::string tagContext,
        list<string> taglist, unsigned long uiFlags, int timeout);

    void Release();
    CInmageIoctlController* GetInstance(IPlatformAPIs *platformApis, ILogger *pLogger);
    void ServiceShutdownNotify(ULONG ulFlags);

    bool LocalCrashConsistentTagIoctl(std::string tagContext,
        list<string> taglist, unsigned long uiFlags, int timeout);

    void StartFiltering(std::string deviceId, unsigned long long nBlocks, unsigned int blockSize);
    void StopFiltering();
    int StopFiltering(std::string deviceId, bool delBitmapFile, bool bStopAll);
    void ResyncStartNotification(std::string deviceId);
    void ResyncEndNotification(std::string deviceId);
    void ClearDifferentials(std::string deviceId);
    void GetDriverVersion();
    int WaitDB(std::string deviceId);
    void GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans,
        PUDIRTY_BLOCK_V2  pDirtyBlock, bool bThowIfResync, std::string deviceId);
    void CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction);
    void HoldWrites(std::string tagContext, unsigned long ulFlags, unsigned long ulTimeoutInMS);
    void ReleaseWrites(std::string tagContext, unsigned long ulFlags);
    void CommitTag(std::string tagContext, unsigned long ulFlags);
    void InsertDistTag(std::string tagContext, unsigned long  ulFlags, list<string> taglist);
    bool LocalStandAloneCrashConsistentTagIoctl(std::string tagContext,
        list<std::string> taglist, unsigned long uiFlags, int inTimeoutInMS);
    bool GetVolumeStats(std::string &volGuid, std::string &volumeStat);
};
#endif

