//
// Copyright (c) 2008 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : cachemgr.h
//
// Description:
//


#ifndef CACHEMGR__H
#define CACHEMGR__H

#include <boost/shared_ptr.hpp>
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>

#include "localconfigurator.h"
#include "configurator2.h"
#include "volumegroupsettings.h"
#include "diffsortcriterion.h"
#include "cacheinfo.h"
#include "cxtransport.h"
#include "synchronize.h"
#include "cdplock.h"
#include "usecfs.h"
#include "LogCutter.h"

typedef boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > ACE_Shared_MQ;

class DiffGetTask;
typedef boost::shared_ptr<DiffGetTask> DiffGetTaskPtr;
typedef std::map<std::string,DiffGetTaskPtr> DiffGetTasks_t;
typedef std::pair<std::string,DiffGetTaskPtr> DiffGetTaskPair_t;


class NWTask;
typedef boost::shared_ptr<NWTask> NWTaskPtr;
typedef std::list<NWTaskPtr> NWTasks_t;

class IOTask;
typedef boost::shared_ptr<IOTask> IOTaskPtr;
typedef std::list<IOTaskPtr> IOTasks_t;

class RenameTask;
typedef boost::shared_ptr<RenameTask> RenameTaskPtr;
typedef std::list<RenameTaskPtr> RenameTasks_t;

class ReporterTask;
typedef boost::shared_ptr<ReporterTask> ReporterTaskPtr;


class CacheMgr : public sigslot::has_slots<>
{
public:

    CacheMgr(int argc, char ** argv);
    ~CacheMgr();
    bool run();
    SV_ULONGLONG CurrentDiskUsage(std::string &cachelocation);
    //bool RemoveDeadTask(std::string & volname);

    SV_ULONGLONG getMaxDiskUsagePerReplication() { return m_MaxAllowedDiskUsagePerPair ; }
    SV_ULONG MaxInMemoryCompressedFileSize() { return m_MaxInMemoryCompressedFileSize; }
    SV_ULONG MaxInMemoryUnCompressedFileSize() {  return m_MaxInMemoryUncompressedFileSize; }
    SV_ULONG MaxMemoryUsage() { return m_MaxMemoryUsage; }
    SV_UINT MaxNwTasks() { return m_maxnwTasks; }
    SV_UINT MaxIoTasks() { return m_maxioTasks; }
    SV_UINT getPendingDataReporterInterval() { return m_reporterInterval; }
    bool getCMVerifyDiff() { return m_bCMVerifyDiff; }
    bool getCMPreserveBadDiff() { return m_bCMPreserveBadDiff; }
    std::string getDiffTargetCacheDirectoryPrefix() { return m_difftgt_dir_prefix; }
    std::string getDiffTargetFilenamePrefix() { return m_difftgt_fname_prefix ; }
    std::string getDiffTargetSourceDirectoryPrefix() { return m_diffsrc_dir_prefix ; }
    std::string getHostId() { return m_hostid ; }
    SV_ULONG getMinCacheFreeDiskSpacePercent() { return m_mincache_free_pct; }
    SV_ULONGLONG getMinCacheFreeDiskSpace() { return m_mincache_free_bytes ; }

public:
    boost::shared_ptr<Logger::LogCutter> m_LogCutter;

protected:

    bool AnotherInstanceRunning();
    bool SetupLocalLogging();
    bool InitializeConfigurator();
    bool StopLocalLogging();
    bool CopyLocalLog();
    bool StopConfigurator();
    bool init();
    bool uninit();

    void help();
    void usage(char * progname, bool error=true);
    bool parseInput();

    void ConfigChangeCallback(InitialSettings);

private:

    CacheMgr(CacheMgr const & );
    CacheMgr& operator=(CacheMgr const & );

    bool initializeTal();

    bool SendChangeNotification(InitialSettings &);
    bool StartNewPairs(InitialSettings &);
    bool StartReporterTask();

    bool isTargetRestartResynced(InitialSettings & settings, const std::string & volname);
    bool isPairDeleted(InitialSettings & settings, const std::string & volname);

    bool StopAllTasks();
    bool NotifyTimeOut();
    bool AllTasksExited();
    bool ReapDeadTasks();

    bool FetchConfigurationValues();


    int m_argc;
    char ** m_argv;
    LocalConfigurator m_lc;

    Configurator* m_configurator;
    bool m_bconfig;
    bool m_blocalLog;
    std::string m_LogPath;

    SV_ULONGLONG m_MaxAllowedDiskUsagePerPair;
    SV_ULONG m_MaxInMemoryCompressedFileSize;
    SV_ULONG m_MaxInMemoryUncompressedFileSize;
    SV_ULONG m_MaxMemoryUsage;
    SV_UINT m_maxnwTasks;
    SV_UINT m_maxioTasks;
    SV_UINT m_reporterInterval;
    bool m_bCMVerifyDiff;
    bool m_bCMPreserveBadDiff;
    std::string m_difftgt_fname_prefix ;
    std::string m_diffsrc_dir_prefix;
    std::string m_hostid;
    std::string m_difftgt_dir_prefix;
    SV_ULONG m_mincache_free_pct;
    SV_ULONGLONG m_mincache_free_bytes;
    std::string m_uncompress_exe ;

    ACE_Thread_Manager m_ThreadManager;

    DiffGetTasks_t m_gettasks;
    ReporterTaskPtr m_reporterTaskPtr;
    mutable ACE_Mutex m_lockTasks;
    typedef ACE_Guard<ACE_Mutex> AutoGuard;
    CDPLock::Ptr m_cdplock;
};

class DiffGetTask : public ACE_Task<ACE_MT_SYNCH>
{
public:

    DiffGetTask(CacheMgr* cachemgr, Configurator* configurator,
		const std::string &volname,
		ACE_Thread_Manager* threadManager = 0,
		const bool& launchedForCleanup = false)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
          m_launchedForCleanup(launchedForCleanup),
          m_cachemgr(cachemgr),
          m_configurator(configurator),
          m_volname(volname),
          m_transport(TRANSPORT_PROTOCOL_UNKNOWN),
          m_mode(VOLUME_SETTINGS::SECURE_MODE_NONE),
          m_throttled(false),
        m_paused(false),
        m_cxLocation(""),
        m_cacheLocation(""),
        m_tmpCacheLocation(""),
        m_uncompressLocation(""),
        m_Diskcapacity(0),
        m_MinUnusedDiskSpace(0),
        m_MemoryUsage(0),
        m_bDead(false),
        m_downloadindex(0),
        m_writeindex(0),
        m_renameindex(0),
    //Bug# 8149
        m_transportInit(false)
	{ }

    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
    bool isDead() { return m_bDead ;}

    void PostMsg(int msgId, int priority);

    TRANSPORT_PROTOCOL  Protocol(){ return m_transport; }
    bool Secure() { return (VOLUME_SETTINGS::SECURE_MODE_SECURE  == m_mode); }
    std::string  CxLocation() { return m_cxLocation; }
    std::string  CacheLocation() { return m_cacheLocation;}

    //Bug: 17800
    std::string  TransportIP() { return m_transportSettings.ipAddress; }
    //Bug #8025
    std::string TmpCacheLocation() { return m_tmpCacheLocation; }
    std::string UncompressLocation() { return m_uncompressLocation; }
    SV_ULONGLONG  MinUnusedDiskSpace() { return m_MinUnusedDiskSpace; }

    bool Throttle();
    bool Pause();

    SV_ULONG CurrentMemoryUsage() { return m_MemoryUsage ; }
    bool FileFitsInMemory(CacheInfo::CacheInfoPtr cacheinfoPtr);
    void AdjustMemoryUsage(bool decrement, SV_ULONG size);

    bool DiskSpaceAvailable(SV_ULONG size, bool sendalert);

    CacheInfo::CacheInfoPtr FetchNextDiffForDownload(bool & done);
    CacheInfo::CacheInfoPtr FetchNextDiffForWrite(bool & done);
    CacheInfo::CacheInfoPtr FetchNextDiffForRename(bool & done);

protected:

    bool FetchInitialSettings();
    bool ConfigChange();
    bool CleanTmpFiles();
    bool CleanCache();
    bool CreateWorkerThreads();
    bool StopWorkerThreads();
    int ScheduleDownload();
    bool AbortSchedule(int &reasonForAbort, int seconds = 0);
    bool FetchPendingFileList();
    bool TrimToSize(SV_ULONGLONG size);
    bool BuildOrderedList();
    bool TrimListOnTimeOut();

    //Bug# 8149
    bool SkipAlreadyDownloadedFiles();
    SV_ULONGLONG MaxAllowedDiskUsage() { return m_cachemgr -> getMaxDiskUsagePerReplication(); }
    SV_ULONGLONG AllowedDiskUsage();

    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    bool FetchNextMessage(ACE_Message_Block **);
    void MarkDead() { m_bDead = true; }

private:
    CacheMgr * m_cachemgr;
    Configurator* m_configurator;
    std::string m_volname;

    TRANSPORT_PROTOCOL m_transport;
    VOLUME_SETTINGS::SECURE_MODE m_mode;
    //Bug #8149
    TRANSPORT_CONNECTION_SETTINGS m_transportSettings;

    bool m_throttled;
    bool m_paused;
    std::string m_cxLocation, m_cacheLocation, m_tmpCacheLocation;
    std::string m_uncompressLocation;

    SV_ULONG m_MaxInMemoryCompressedFileSize;
    SV_ULONG m_MaxInMemoryUncompressedFileSize;
    SV_ULONG m_MaxMemoryUsage;
    SV_UINT m_maxnwTasks;
    SV_UINT m_maxioTasks;

    SV_ULONGLONG m_Diskcapacity;
    SV_ULONGLONG m_MinUnusedDiskSpace;

    Lockable m_SettingsLock;
    Lockable m_MemoryUsageLock;
    SV_ULONG m_MemoryUsage;

    ACE_Thread_Manager m_workerThreadMgr;
    NWTasks_t m_nmtasks;
    IOTasks_t m_iotasks;
    RenameTasks_t m_renametasks;

    bool m_bDead;

    bool m_launchedForCleanup;

    CacheInfo::CacheInfosOrderedEndTime_t m_FileList;
    CacheInfo::CacheInfosOrderedListPtr_t m_pendingfiles;

    Lockable m_nextdownloadLock;
    Lockable m_nextwriteLock;
    Lockable m_nextrenameLock;

    size_t m_downloadindex;
    size_t m_writeindex;
    size_t m_renameindex;

    bool m_transportInit;
    CxTransport::ptr m_cxTransport;

    cfs::CfsPsData m_cfsPsData;
};


class NWTask : public ACE_Task<ACE_MT_SYNCH>
{
public:

    NWTask( CacheMgr* cachemgr, DiffGetTask* gettask,
            Configurator * configurator,
            const std::string &volname,
            ACE_Thread_Manager* threadManager,
            cfs::CfsPsData cfsPsData
            )
        : m_cachemgr(cachemgr),m_gettask(gettask),
          m_configurator(configurator),
          m_volname(volname),
          m_bytesDownloaded(0),
          m_bCMVerifyDiff(false),
          m_bCMPreserveBadDiff(false),
          ACE_Task<ACE_MT_SYNCH>(threadManager),
          m_cfsPsData(cfsPsData)
	{}

    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();

    void PostMsg(int msgId, int priority);

protected:
    //Bug #8149
    bool DownloadFile(CacheInfo::CacheInfoPtr cacheinfoPtr);
    bool DownloadRequestedFiles();
    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    bool FetchNextMessage(ACE_Message_Block **);

    bool RenameRemoteFile(const std::string& oldpath, const std::string& newpath);


private:

    CacheMgr * m_cachemgr;
    DiffGetTask * m_gettask;
    Configurator * m_configurator;
    std::string m_volname;
    SV_ULONGLONG m_bytesDownloaded;
    bool m_bCMVerifyDiff;
    bool m_bCMPreserveBadDiff;

    CxTransport::ptr m_cxTransport;

    cfs::CfsPsData m_cfsPsData;
};

class IOTask : public ACE_Task<ACE_MT_SYNCH>
{
public:

    IOTask( CacheMgr* cachemgr, DiffGetTask* gettask,
            Configurator * configurator,
            const std::string &volname,
            ACE_Thread_Manager* threadManager
            )
        : m_cachemgr(cachemgr),m_gettask(gettask),
          m_configurator(configurator),
          m_volname(volname),
          m_bytesWritten(0),
          m_bCMVerifyDiff(false),
          m_bCMPreserveBadDiff(false),
          ACE_Task<ACE_MT_SYNCH>(threadManager)
	{}

    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();

    void PostMsg(int msgId, int priority);

protected:
    bool WriteFile(CacheInfo::CacheInfoPtr cacheinfoPtr);
    bool VerifyFile(CacheInfo::CacheInfoPtr cacheinfoPtr);
    bool WriteRequestedFiles();
    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    bool FetchNextMessage(ACE_Message_Block **);
    bool VerifyFileContentsIfEnabled(const std::string & localFileName);


private:

    CacheMgr * m_cachemgr;
    DiffGetTask * m_gettask;
    Configurator * m_configurator;
    std::string m_volname;
    SV_ULONGLONG m_bytesWritten;
    bool m_bCMVerifyDiff;
    bool m_bCMPreserveBadDiff;
    //Bug# 8025
    typedef ACE_Guard<ACE_File_Lock> AutoGuard;
};

class RenameTask : public ACE_Task<ACE_MT_SYNCH>
{
public:

    RenameTask( CacheMgr* cachemgr,DiffGetTask* gettask,
		Configurator * configurator,
		const std::string &volname,
		ACE_Thread_Manager* threadManager,
                cfs::CfsPsData cfsPsData
		)
        : m_cachemgr(cachemgr),
          m_gettask(gettask),
          m_configurator(configurator),
          m_volname(volname),
          m_sentAlertToCx(false),
          ACE_Task<ACE_MT_SYNCH>(threadManager),
          m_cfsPsData(cfsPsData)
	{}

    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();

    void PostMsg(int msgId, int priority);

protected:
    bool RenameandDeleteFile(CacheInfo::CacheInfoPtr cacheinfoPtr);
    //Bug #8025
    bool RenameFile(const std::string& oldpath, const std::string& newpath);
    bool RetryableRenameFile(CacheInfo::CacheInfoPtr cacheinfoPtr);

    //Bug #8149
    bool DeleteRemoteFile(CacheInfo::CacheInfoPtr cacheinfoPtr);
    bool DeleteRemoteFile(const std::string& filename, bool addentry = true);

    bool RenameRequestedFiles();
    bool isTsoFile(const std::string & filePath);
    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    bool FetchNextMessage(ACE_Message_Block **);


private:

    CacheMgr * m_cachemgr;
    DiffGetTask * m_gettask;
    Configurator * m_configurator;
    std::string m_volname;

    bool m_sentAlertToCx;

    CxTransport::ptr m_cxTransport;
    //Bug #8149
    void AddPendingFile(const std::string& filename);
    bool DeletePendingFile();
    //Bug #8025
    typedef ACE_Guard<ACE_File_Lock> AutoGuard;

    cfs::CfsPsData m_cfsPsData;
};

class ReporterTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    ReporterTask(CacheMgr* cachemgr, Configurator* configurator, ACE_Thread_Manager* threadManager)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
          m_cachemgr(cachemgr),
          m_configurator(configurator),
          m_bDead(false)
	{
	}

    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();
    bool isDead() { return m_bDead ;}

    void PostMsg(int msgId, int priority);

protected:
    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    void MarkDead() { m_bDead = true; }

private:
    bool SendPendingDataInfo();

    bool m_bDead;

    CacheMgr * m_cachemgr;
    Configurator * m_configurator;
};

#endif
