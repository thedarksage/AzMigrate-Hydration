#ifndef _DATA_PROCESSOR_H_
#define _DATA_PROCESSOR_H_
#include "Logger.h"
#include "IPlatformAPIs.h"

#include <stdio.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <iterator>

#ifdef SV_WINDOWS
#include "InmFltInterface.h"
#endif
#include "svdparse.h"
#include "svtypes.h"

#include "IBlockDevice.h"
#include "CDiskDevice.h"
#include "SegStream.h"
#include "InmageTestException.h"
#include "Logger.h"

#include "boost/thread/mutex.hpp"
#include "boost/timer.hpp"
#include "boost/unordered_set.hpp"

#include <list>
#include <map>


#ifdef SV_WINDOWS
#ifdef DATAPROCESSOR_EXPORTS
#define DATAPROCESSOR_API __declspec(dllexport)
#else
#define DATAPROCESSOR_API __declspec(dllimport)
#endif
#else
#define DATAPROCESSOR_API
#endif


class DATAPROCESSOR_API IDataProcessor
{
public:
    virtual void ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length) = 0;
    virtual void ProcessTSO() = 0;
    virtual void ProcessTags(list<std::string> tags) = 0;
    virtual void WaitForTSO(SV_ULONG timeoutMs) = 0;
    virtual bool WaitForTag(std::string tagName, SV_ULONG timeout, bool dontApply) = 0;
#ifdef SV_UNIX
    virtual void WaitForAllowDraining() = 0;
    virtual void PauseDraining() = 0;
    virtual void AllowDraining() = 0;
    virtual void ResetTSO() = 0;
#endif
    virtual void InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset) = 0;
    virtual bool CreateDrainBarrierOnTag(std::string tag) = 0;
    virtual void ReleaseDrainBarrier() = 0;
};

class DATAPROCESSOR_API CDataApplier : public IDataProcessor
{
private:
    vector<char>                        m_buffer;
    int                                 m_bufferLength;
    IBlockDevice*                       m_pSrcDisk;
    IBlockDevice*                       m_targetDisk;
    IPlatformAPIs*                      m_platformApis;
    ILogger*                            m_pLogger;
    Event                               m_waitTSO;
    bool                                m_bInitialSyncCompleted;
    map<string, Event*>                 m_tagWaitQueue;;
    boost::unordered_set<std::string>   m_TagReceived;
    boost::mutex                        m_tagMutex;
    Event                               m_tagProcessed;
    bool                                m_DontApply;
    bool                                m_DropTag;
#ifdef SV_UNIX
    Event                               m_allowDraining;
#endif

public:
    CDataApplier(int srcDisk, int targetDisk, IPlatformAPIs *platform, ILogger *logger);
    CDataApplier(IBlockDevice *srcDevice,
        IBlockDevice *tgtDevice, IPlatformAPIs *platform, ILogger *logger);
    ~CDataApplier();
    void ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length);
    void ProcessTSO();
    void ProcessTags(list<std::string> tags);
    void WaitForTSO(SV_ULONG timeoutMs);
    bool WaitForTag(std::string tagName, SV_ULONG timeout, bool dontApply);
    void InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset);
    bool CreateDrainBarrierOnTag(std::string tag);
    void ReleaseDrainBarrier();
#ifdef SV_UNIX
    void ResetTSO();
    void WaitForAllowDraining();
    void PauseDraining();
    void AllowDraining();
#endif
};

class DATAPROCESSOR_API CDataWALProcessor : public IDataProcessor
{
private:
    char*           m_buffer;
    int             m_bufferLength;
    IBlockDevice*   m_pSrcDisk;
    IBlockDevice*   m_targetDisk;
    IPlatformAPIs*  m_platformApis;
    ILogger*        m_pLogger;
    SV_ULONGLONG    m_currentFileIndex;
    string          m_currentDirectory;
    string          m_testRoot;
    Event           m_waitTSO;
    boost::mutex    m_dirDiskMutex;
    int             m_currentDirIndex;
    void            CreateTagDirectory();
    bool            m_bInitialSyncCompleted;
    static bool     m_bRootDirectoryCreated;
    bool            m_bTagInProgress;
    Event           m_tagProcessed;
protected:
    static boost::mutex             m_dirGlobalMutex;
    static int                      m_currentGlobalDirIndex;
    volatile static SV_USHORT       s_numDisksInSet;
    volatile static SV_USHORT       s_numDisksReceivedTag;
    volatile static SV_USHORT       s_numDisksCommitedTag;
    static boost::shared_ptr<Event> s_allDisksReceivedTag;
    static boost::shared_ptr<Event> s_allDisksCommittedTag;
public:
    CDataWALProcessor(int srcDisk, std::string testRoot, IPlatformAPIs *platform, ILogger *logger);
    ~CDataWALProcessor();
    void ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length);
    void ProcessTSO();
    void ProcessTags(list<std::string> tags);
    void WaitForTSO(SV_ULONG timeoutMs);
    bool WaitForTag(std::string tagName, SV_ULONG timeout, bool dontApply);
    void InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset);
    bool CreateDrainBarrierOnTag(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void ReleaseDrainBarrier() { m_tagProcessed.Signal(true); }
#ifdef SV_UNIX
    void ResetTSO();
    void WaitForAllowDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void PauseDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void AllowDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
#endif
};

class DATAPROCESSOR_API CDataWALSingleProcessor : public IDataProcessor
{
private:
    char*           m_buffer;
    int             m_bufferLength;
    IBlockDevice*   m_pSrcDisk;
    IBlockDevice*   m_targetDisk;
    IPlatformAPIs*  m_platformApis;
    ILogger*            m_pLogger;
    SV_ULONGLONG          m_currentFileIndex;
    std::string          m_currentDirectory;
    std::string          m_testRoot;
    Event                m_waitTSO;
    boost::mutex           m_dirDiskMutex;
    int             m_currentDirIndex;
    void            CreateTagDirectory();
    bool            m_bInitialSyncCompleted;
    static bool     m_bRootDirectoryCreated;
    bool            m_bTagInProgress;
    Event           m_tagProcessed;
public:
    CDataWALSingleProcessor(int srcDisk,
        std::string testRoot, IPlatformAPIs *platform, ILogger *logger);
    ~CDataWALSingleProcessor();
    void ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length);
    void ProcessTSO();
    void ProcessTags(list<string> tags);
    void WaitForTSO(SV_ULONG timeoutMs);
    bool WaitForTag(string tagName, SV_ULONG timeout, bool dontApply);
    void InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset);
    void WriteDataFile(string filename, char* buffer, int length);
    bool CreateDrainBarrierOnTag(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }

    void ReleaseDrainBarrier() { m_tagProcessed.Signal(true); }
#ifdef SV_UNIX
    void WaitForAllowDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void PauseDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void AllowDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void ResetTSO();
#endif
};

class DATAPROCESSOR_API CNullDataProcessor : public IDataProcessor
{
private:
    ILogger*        m_pLogger;
    std::string     m_testRoot;
    IPlatformAPIs*  m_platformApis;
    Event           m_tagProcessed;

public:
    CNullDataProcessor(int srcDisk, std::string testRoot, IPlatformAPIs *platform, ILogger *logger);
    ~CNullDataProcessor();
    void ProcessChanges(PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length);
    void ProcessTSO();
    void ProcessTags(list<std::string> tags);
    void WaitForTSO(SV_ULONG timeoutMs);
    bool WaitForTag(std::string tagName, SV_ULONG timeout, bool dontApply);
    void InitialSync(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset);
    void WriteDataFile(std::string filename, char* buffer, int length);
    bool CreateDrainBarrierOnTag(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void ReleaseDrainBarrier() { m_tagProcessed.Signal(true); }
#ifdef SV_UNIX
    void WaitForAllowDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void PauseDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void AllowDraining(std::string tag)
    {
        throw CDataProcessorException("%s: Not Implemented", __FUNCTION__);
    }
    void ResetTSO();
#endif
};

#endif
