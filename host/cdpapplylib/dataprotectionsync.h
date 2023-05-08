//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : dataprotectionsync.h
//
// Description:
//

#ifndef DATAPROTECTIONSYNC_H
#define DATAPROTECTIONSYNC_H


#include <string>
#include <vector>
#include <ace/Mutex.h>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/atomic.hpp>

#include "svtypes.h"
#include "volumegroupsettings.h"
#include "transport_settings.h"
#include "cxtransport.h"
#include "compatibility.h"
#include "cdputil.h"
#include "svproactor.h"
#include "compressmode.h"
#include "cdpvolumeutil.h"
#include "usecfs.h"
#include "genericprofiler.h"
#include "datacacher.h"
#include "HealthCollator.h"

#ifdef DP_SYNC_RCM
#include "RcmConfigurator.h"
#endif

#include <ace/Task.h>
#define ENDIANTAG_COMPATIBILITY_NUMBER 510000
#define FSAWARE_RESYNC_COMPATIBILITY_NUMBER 650000

#define MSGQUEUE_NORMAL_PRIORITY  0x01
#define MSGQUEUE_HIGH_PRIORITY    0x10

typedef enum clearDiffsState
{ 
    DontKnowClearDiffsState,
    DontClearDiffs,
    ClearDiffs

} ClearDiffsState_t;

typedef struct transportOp
{
    typedef enum op
    {
        NoOp,
        DeleteFile
        /* TODO: add other ops later */
        
    } OP;
    OP m_op;
    unsigned int m_nRetries;
    unsigned int m_RetryDelay;
    std::string m_fileName;
    bool m_shouldReinit;

    transportOp()
        {
            m_op = NoOp;
            m_nRetries = 1;
            m_shouldReinit = false;
            m_RetryDelay = 0;
        }
     
    transportOp(const OP op, const unsigned int &retrycnt, const unsigned int &retrydelay,
                const std::string &filename, const bool &shouldreinit):
        m_op(op), m_nRetries(retrycnt), m_RetryDelay(retrydelay), 
        m_fileName(filename), m_shouldReinit(shouldreinit)
        {
        }

} TransportOp;

typedef std::vector<std::string> Files_t;
typedef ACE_Guard<ACE_Mutex> AutoGuard ;

class DataProtectionSync {
public:
    static bool const DP_BYTES_MATCH    = true;
    static bool const DP_BYTES_NO_MATCH = false;
    bool m_SyncThroughThread;

	DataCacher::VolumesCache m_VolumesCache; ///< vic cache; Public for now: to share with dpdrivercomm

    explicit DataProtectionSync(VOLUME_GROUP_SETTINGS const & volumeGrpSettings,const bool syncThroughThreads = false);


    virtual ~DataProtectionSync();
    //{
    //    //CDPUtil::UnInitQuitEvent();
    //    if( m_dumpHcdsHandle != ACE_INVALID_HANDLE )
    //    {
    //        ACE_OS::close(m_dumpHcdsHandle) ;
    //    }
    //}

    virtual void Start() = 0;
    virtual void Stop() {
        m_Protect = false;
		SignalQuit();
    }
    bool Protect() const { return m_Protect; }
    virtual void UpdateVolumeGroupSettings(VOLUME_GROUP_SETTINGS& volumeGroupSettings) = 0;

#ifdef DP_SYNC_RCM
    const RcmClientLib::RcmConfigurator& GetConfigurator() const;
#else
    ConfigureVxAgent& GetConfigurator() const;
#endif

    HTTP_CONNECTION_SETTINGS GetHttpSettings() const;

    bool QuitRequested(int seconds = 0);
    void Idle(int seconds);

    bool AsyncOpEnabled() { return m_asyncops; }
    bool useNewApplyAlgorithm() { return m_useNewApplyAlgorithm; }
    ACE_Mutex m_dumpHasheslock ;
    ACE_HANDLE m_dumpHcdsHandle ;
    void dumpHashesToFile(ACE_HANDLE handle, std::string str) ;
    SV_ULONGLONG GetSrcStartOffset(void) const {  return m_SrcStartOffset; }
    int GetGrpId(void) { return m_GrpId; };
    void FillClusterBitmapCustomizationInfos(const bool &isFilterDriverAvailable);
    std::string ProfilingPath() const { return m_profilingPath; }
    bool ProfileVerbose() const { return m_profileVerbose; }
protected:

    JOB_ID_OFFSET m_jobIdOffsetPair;

    typedef boost::shared_array<char> BoostArray_t;
    virtual void ThrottleSpace();

    void HandleDatapathThrottle(const ResponseData &rd);
    bool SendData(CxTransport::ptr cxTransport, std::string const & fileName, char const * buffer, int length, bool moreData, COMPRESS_MODE compressMode);
    bool SendAbort(CxTransport::ptr cxTransport, std::string const & fileName);
    bool SendFile(CxTransport::ptr cxTransportl, const std::string & LocalFile, const std::string & uploadFilename, COMPRESS_MODE compressMode);

    bool GetFileList(CxTransport::ptr cxTransport, std::string const & fileSpec, Files_t& files);

    /** Fast Sync TBC - BSR
     *  JobId is added as part of Volume Group Settings, If there is any change in the
     *  settings dataprotection would be exited itself, so need to have separate calls for the same.
     **/
    /*bool GetRemoteJobId();
      bool GetRemoteJobId(std::string & jobId);   */
    void RequestQuit() {  CDPUtil::RequestQuit();}
    void SignalQuit() {  CDPUtil::SignalQuit();}
    bool ServiceRequestedQuit(int waitTimeSeconds);
    bool BuildRemoteSyncDir();
    bool GetSyncBytesToApplyThreshold();
    virtual bool GetSyncDir();
    virtual std::string GetDeviceName() const { return m_Settings.deviceName; }
    bool ExtractSyncDrives(boost::shared_array<char>& buffer, unsigned long * drives);

    bool SetCheckPointOffsetWithRename(std::string const & agent,
                                       SV_LONGLONG offset,
                                       SV_LONGLONG bytes,
                                       bool matched,
                                       std::string const & name,
                                       std::string const & newName);

    bool SetCheckPointOffsetWithDelete(std::string const & agent,
                                       SV_LONGLONG offset,
                                       SV_LONGLONG bytes,
                                       bool matched,
                                       std::string const & name);

    bool SetCheckPointOffsetWithDelete(std::string const & agent,
                                       SV_LONGLONG offset,
                                       SV_LONGLONG bytes,
                                       bool matched,
                                       std::string const & delete1,
                                       std::string const & delete2);

    bool StartProactor(SVProactorTask*& proactorTask, ACE_Thread_Manager& threadManager);
    bool StopProactor(SVProactorTask*& proactorTask, ACE_Thread_Manager& threadManager);

/*
 *  Added Following functions to fix overshoot issue using bitmap
 *  By Suresh
 */
    virtual bool SetFastSyncProgressBytes(const SV_LONGLONG &bytesApplied, const SV_ULONGLONG &bytesMatched,
        const std::string &resyncStatsJson);
    virtual bool SetFastSyncMatchedBytes( SV_ULONGLONG bytes, int during_resyncTransition=0) ;
    virtual bool SetFastSyncFullyUnusedBytes( SV_ULONGLONG bytesunused);

    virtual bool SetFastSyncFullSyncBytes(SV_LONGLONG fullSyncBytes);

    virtual bool IsNoDataTransferSync() const
    {
        /* Dummy impl - overloaded in FastSyncRcm */
        return false;
    }

    virtual void getResyncTimeSettingsForNoDataTransfer(ResyncTimeSettings& resyncTimeSettings) const
    {
        /* Dummy impl - overloaded in FastSyncRcm */
    }

    bool SendSetCheckPointOffset(std::string const & agent,
                                 const SV_LONGLONG offset,
                                 const  SV_LONGLONG bytes,
                                 bool matched,
                                 std::string const & name,
                                 std::string const & newName,
                                 std::string const & deleteName1,
                                 std::string const & deleteName2);

    /* Added by BSR for Fast Sync TBC */
    void getPairDetails( std::string& sourceHostId,
                         std::string& sourceDeviceName,
                         std::string& destHostId,
                         std::string& destDeviceName ) ;
    int setResyncTransitionStepOneToTwo( std::string const& syncNoMoreFile ) ;
    bool getVolumeCheckpoint( JOB_ID_OFFSET&) ;
    bool getResyncStartTimeStamp( ResyncTimeSettings& rts, std::string hostType = std::string( "source" ) ) ;
    bool getResyncEndTimeStamp( ResyncTimeSettings& rts, std::string hostType = std::string( "source" ) ) ;

    int sendResyncStartTimeStamp( const SV_ULONGLONG ts, const SV_ULONGLONG seq, std::string hostType = std::string( "source" ) ) ;
    int sendResyncEndTimeStamp( const SV_ULONGLONG ts, const SV_ULONGLONG seq, std::string hostType = std::string( "source" ) ) ;
    /* End of the change*/
    int setLastResyncOffsetForDirectSync(const SV_ULONGLONG &offset, const SV_ULONGLONG &filesystemunusedbytes);

    bool CreateFullLocalCacheDir(std::string const & cleanupPattern1, std::string const & cleanupPattern2 = std::string());

    bool isCompressedFile(const std::string &filename);
    bool UncompressIfNeeded(std::string& localFile);

    // uncompress the input buffer
    // upon success the input buffer (buffer) would contain the
    // uncompressed data and buflen would contain the uncompressed length
    bool UncompressToMemory(BoostArray_t & buffer, SV_ULONG & buflen);

    // uncompress the input buffer to disk
    bool UncompressToDisk(BoostArray_t & buffer, const SV_ULONG & buflen, std::string &localFile);

    bool Quit() const { return CDPUtil::Quit(); }
    bool Source() const { return m_Source; }
    bool Secure() const { return m_Secure; }    
    
    void RemoveLocalFile(std::string const & name);

    std::string const & CacheDir() const { return m_CacheDir; }
    std::string const & RemoteSyncDir() const { return m_RemoteSyncDir; }
    void SetRemoteSyncDir(const std::string & remoteSyncDir)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: RemoteSyncDir=%s.\n", FUNCTION_NAME, remoteSyncDir.c_str());
        m_RemoteSyncDir = remoteSyncDir;
    }
    std::string const & JobId() const { return m_JobId; }

    VOLUME_SETTINGS const & Settings() const { return m_Settings; }

	cdp_volume_t::CDP_VOLUME_TYPE DeviceType() const { return m_deviceType; }
    cfs::CfsPsData getCfsPsData() const { return m_cfsPsData; }
    
    HTTP_CONNECTION_SETTINGS const & Http() const { return m_Http; }

    TRANSPORT_CONNECTION_SETTINGS const & TransportSettings() const { return m_TransportSettings; }

    TRANSPORT_PROTOCOL Protocol() const { return m_Protocol; };

    bool DoTransportOp(const TransportOp &top, CxTransport::ptr &cxTransport);
    void SetShouldCheckForCacheDirSpace();
    bool ShouldCheckForCacheDirSpace();
    bool ResyncStartNotify();
    bool ResyncEndNotify();
    virtual ClearDiffsState_t CanClearDifferentials();
    SV_ULONG RequiredCompatibility(int operation = 0) { return RequiredCompatibilityNum();}
    inline SV_ULONG RequireCompatibilityNumForBitMapProcessing() { return 430000; }
    bool IsFsAwareResync(void) const { return m_IsFsAwareResync; };
    VOLUME_SETTINGS::RESYNC_COPYMODE GetResyncCopyMode(void) const { return m_ResyncCopyMode; }

    void openCheckumsFile() ;
    typedef std::list<std::string> VolumeList;
     
    unsigned int GetEndianTagSize(void);

    //Added for making pre allocation configurable. See Bug#6984
    bool m_PreAllocate;
    unsigned int m_endianTagSize;

    unsigned int m_SourceReadRetries;
    bool m_bZerosForSourceReadFailures;
    unsigned int m_SourceReadRetriesInterval;

    unsigned int SourceReadRetries(void) const
        {
            return m_SourceReadRetries;
        }

    bool ZerosForSourceReadFailures(void) const
        {
            return m_bZerosForSourceReadFailures;
        }

    unsigned int SourceReadRetriesInterval(void) const
        {
            return m_SourceReadRetriesInterval;
        }

    cdp_volume_util::ClusterBitmapCustomizationInfos_t * m_pClusterBitmapCustomizationInfos;
    cdp_volume_util::ClusterBitmapCustomizationInfos_t * GetClusterBitmapCustomizationInfos(void)
        {
            return m_pClusterBitmapCustomizationInfos;
        }

	void SetDeviceType(void);

	/// \brief sets volume cache
	void SetVolumesCache(void);

    bool IsTransportProtocolAzureBlob() const;

    virtual bool IsResyncStartTimeStampAlreadySent(ResyncTimeSettings &rts);

    virtual bool NotifyResyncStartToControlPath(const ResyncTimeSettings &rts, const bool canClearDiffs);

    virtual bool IsResyncEndTimeStampAlreadySent(ResyncTimeSettings &rts);

    virtual bool NotifyResyncEndToControlPath(const ResyncTimeSettings &rts);

    virtual void SendAlert(SV_ALERT_TYPE alertType, SV_ALERT_MODULE alertingModule, const InmAlert &alert) const;

protected:
        Profiler m_dataPathThrottleProfiler;
        HealthCollatorPtr m_healthCollatorPtr;

private:
    // no copying DataProtectionSync
    DataProtectionSync(const DataProtectionSync& dpSync);
    DataProtectionSync& operator=(const DataProtectionSync& dpSync);
    bool GenerateResyncTimeIfReq(ResyncTimeSettings &rts);
    void CreateHealthCollator();
    void SetThrottleHealthIssue(const std::string& healthIssueCode);
    void ResetThrottleHealthIssue();


    bool m_Protect;
    bool m_Source;
    bool m_Secure;

    std::string m_CacheDir;
    std::string m_RemoteSyncDir;
    std::string m_JobId;
    int m_GrpId;

	cdp_volume_t::CDP_VOLUME_TYPE m_deviceType;

    VOLUME_SETTINGS m_Settings;

    HTTP_CONNECTION_SETTINGS m_Http;

    TRANSPORT_CONNECTION_SETTINGS m_TransportSettings;

    TRANSPORT_PROTOCOL m_Protocol;


    // 0 - uninitialized
    // 1 - should check
    // 2 - need not check, using file tal and cachedir and remotedir on same partition
    int m_bCheckCacheDirSpace;

    bool m_asyncops;
    bool m_useNewApplyAlgorithm;
    SV_ULONGLONG m_SrcStartOffset;
    bool m_IsFsAwareResync;
    VOLUME_SETTINGS::RESYNC_COPYMODE m_ResyncCopyMode;
    cfs::CfsPsData m_cfsPsData;
    std::string m_profilingPath;
    bool m_profileVerbose;
    boost::atomic<bool> m_bThrottlePersisted;
};


#endif // ifndef DATAPROTECTIONSYNC_H
