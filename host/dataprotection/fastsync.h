//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : fastsync.h
//
// Description:
//

#ifndef FASTSYNC_H
#define FASTSYNC_H

#include <iostream>
#include <string>
#include <fstream>
#include <list>
#include <cstdlib>
#include <set>

#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <ace/Atomic_Op.h>
#include <ace/Mutex.h>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include "svtypes.h"
#include "programargs.h"
#include "cdpvolume.h"
#include "volumegroupsettings.h"
#include "transport_settings.h"
#include "cxtransport.h"
#include "dataprotectionsync.h"
#include "compatibility.h"
#include "bitmap.h"
#include "compress.h"
#include "compressmode.h"

#include "hashconvertor.h"
#include "fsclusterinfo.h"
#include "readinfo.h"
#include "inmpipeline.h"
#include "inmpipelineimp.h"
#include "processcluster.h"
#include "hcdgen.h"
#include "cdpvolumeutil.h"
#include "cdpv2writer.h"

#include "azureagent.h"

#include "irprofiling.h"

#include "json_writer.h"

#define NREADBUFSFORHCDINPIPE 2
#define NBUFSFORHCDINPIPE NREADBUFSFORHCDINPIPE

#define RESYNC_STATS_UPDATE_INTERVAL_IN_SEC 300
#define RESYNC_TARGET_TO_AZURE_HEARTBEAT_INTERVAL_IN_SEC 300
#define RESYNC_SOURCE_MONITOR_TARGET_TO_AZURE_HEARTBEAT_INTERVAL_IN_SEC 300
#define RESYNC_TARGET_TO_AZURE_NO_HEARTBEAT_RAISE_HEALTH_ISSUE_INTERVAL_IN_SEC 1800

// how long to wait for a windows message
int const WindowsWaitTimeInSeconds = 5;

// how long a task should wait for a message in its queue
int const TaskWaitTimeInSeconds = 5;

int const FileNameWaitInMilli = 15 ;

int const NO_PROCESS_LIMIT = 0;
int const PROCESS_LIMIT    = 16;  // number of HashCompareDatas to generate per GenerateHashComapreData calls
// this is to limit the build up of these files on the server

std::string const CleanupFilePattern("completed_sync_*.dat*");
std::string const PreUnderscore(PREUNDERSCORE);
std::string const TmpUnderscore("tmp_");
std::string const BootstrapAgent("BootstrapAgent");
std::string const SyncAgent("ResyncAgent");
std::string const DatExtension(".dat");
std::string const PreHcd(PreUnderscore + "completed_hcd_");
std::string const PreCluster(PreUnderscore + "completed_cluster_");
std::string const HcdPrefix("completed_hcd_*");
std::string const ClusterBitmapPrefix("completed_cluster_*");
std::string const PreSync(PreUnderscore + "completed_sync_");
std::string const SyncDataPrefix("completed_sync_*");
std::string const HcdNoMoreFileName("completed_hcd_nomore.dat");
std::string const ClusterNoMoreFileName("completed_cluster_nomore.dat");
/*
 * The following string constants are used for fixing overshoot problem by Bitmaps
 * By Suresh
 */
std::string const missingSuffix("_missing.dat");
std::string const missingHcdFileSpec("*_hcd_*missing.dat");
std::string const missingChunkFileSpec("*_sync_*missing.dat");

std::string const mapHcdsTransferred("HcdTransfer");
std::string const mapClusterTransferred("ClusterTransfer");
std::string const mapHcdsProcessed("HcdProcess");
std::string const mapSyncApplied("SyncsApplied");
std::string const mapMissingHcds("HcdsMissing");
std::string const mapMissingSyncs("SyncsMissing");
std::string const TmpHcd(TmpUnderscore + "completed_hcd_");
std::string const TmpSync(TmpUnderscore + "completed_sync_");

std::string const UnProcessed("unprocessed");
std::string const Processed("processed");
std::string const Completed("completed");
std::string const Cluster("cluster");

/*End of Change*/
//Added by BSR for TBC Fast Sync
//std::string const syncNoMoreFileName( "completed_sync_nomore.dat" ) ;
std::string const gzipExtension( ".gz" ) ;

std::string const noMore( "_nomore.dat" ) ;
std::string const completedHcdPrefix( "completed_hcd_" ) ;
std::string const completedClusterPrefix( "completed_cluster_" ) ;
std::string const completedSyncPrefix( "completed_sync_" ) ;

//end of the change
std::string const NoMoreFileSpec("*nomore.dat");

// message types
int const SYNC_DONE      = 0x2000;
int const SYNC_EXCEPTION = 0x2001;

// message priority
int const SYNC_NORMAL_PRIORITY = MSGQUEUE_NORMAL_PRIORITY;
int const SYNC_HIGH_PRIORITY   = MSGQUEUE_HIGH_PRIORITY;

// transport retry count
unsigned int const TRANSPORT_RETRY_COUNT   = 3;
unsigned int const TRANSPORT_RETRY_DELAY   = 0;

SV_UINT const HashSize = 16;

typedef std::set<SV_ULONGLONG> Offsets_t;

typedef enum eHCDMatchStatus
{
    E_STATUSPENDING,
    E_NOTMATCHED, 
    E_MATCHED

} E_HCD_MATCHEDSTATUS;


#define HASH_LEN 16

struct VolumeDataAndHCD
{
    char *volData;                         /* address of a single volume read data */
    HashCompareNode hcNode;                /* address of a single hcd node */
    E_HCD_MATCHEDSTATUS eHcdMatchStatus;   /* enum indicating "not yet compared (pending)", "not matched", "matched". 
                                            * Serve as an acknowledgement from secondary check sums thread 
                                            * of hcd match status */
    unsigned int hcdIndex;                 /* Indicates the hcd index being processed */
    unsigned char hash[HASH_LEN];          /* To record for DI check */

    VolumeDataAndHCD()
	{
            volData = NULL;
            eHcdMatchStatus = E_STATUSPENDING;
	}

    ~VolumeDataAndHCD()
	{
	}

    bool allocateReadBuffer(unsigned int length)
	{
#ifdef SV_WINDOWS
            volData = new (std::nothrow) char[length];
#else
            volData = (char *)valloc(length);
#endif /* SV_WINDOWS */

            bool bretval = volData?true:false;
            return bretval;
	}

    void deallocateReadBuffer(void)
	{
            if (volData)
            {
#ifdef SV_WINDOWS
                delete [] volData;
#else
                free(volData);
#endif /* SV_WINDOWS */
            }
	}
	//To get size of hash 
	int getHashsize()
	{
		return sizeof(hash);
	}

};

typedef struct VolumeDataAndHCD VolumeDataAndHCD_t;

/**
 *
 * Data structure for all communication
 *
 */
struct VolumeDataAndHCDBatch
{
    unsigned int countOfHCD;                  /* How many HCDs */
    VolumeDataAndHCD_t *volDataAndHcd;  /* base address of starting hcd */
    std::string hcdName;                      /* Name of HCD to delete on rename of sync file */
    unsigned int totalHCD;
    unsigned int allocatedReadLen;

    VolumeDataAndHCDBatch()
	{
            countOfHCD = totalHCD = allocatedReadLen = 0;
            volDataAndHcd = NULL;
	}

    bool allocateVolDataAndHcd(unsigned int nhcdsinbatch)
	{
            volDataAndHcd = new (std::nothrow) VolumeDataAndHCD_t [nhcdsinbatch];
            bool bretval = volDataAndHcd?true:false;
            return bretval;
	}

    bool allocateReadBuffer(unsigned int length)
	{
            bool bretval = volDataAndHcd->allocateReadBuffer(length);
            if (bretval)
            {
                allocatedReadLen = length;
            }
            return bretval;
	}

    void deallocateReadBuffer(void)
	{
            volDataAndHcd->deallocateReadBuffer();
            allocatedReadLen = 0;
	}

    void deallcateVolDataAndHcd(void)
	{
            if (volDataAndHcd)
            {
                deallocateReadBuffer();
                delete [] volDataAndHcd; 
            }
	}
};

typedef VolumeDataAndHCDBatch VolumeDataAndHCDBatch_t;

#define NUM_READ_BUFFERS 3

struct VolumeReadBuffers
{
    VolumeDataAndHCDBatch_t hcdbatch[NUM_READ_BUFFERS];
    ACE_Message_Block *mb[NUM_READ_BUFFERS];
    unsigned int numreadbufs; 
    bool breuse;

    VolumeReadBuffers()
	{
            breuse = false;
            numreadbufs = NUM_READ_BUFFERS;
            for (unsigned int i = 0; i < numreadbufs;i++)
            {
                mb[i] = NULL;
            }
	}

    ~VolumeReadBuffers()
	{
            for (unsigned int i = 0; i < numreadbufs;i++)
            {
                hcdbatch[i].deallcateVolDataAndHcd();
                /**
                 * Kept it for further debugging
                 if (mb[i])
                 {
                 DebugPrintf(SV_LOG_ERROR, "Before crash at line %d in file %s\n", __LINE__, __FILE__);
                 delete mb[i];
                 } 
                */
            }     
	}

    bool allocateHCDBatch(unsigned int nhcdsinbatch)
	{
            bool bretval = true;    
            for (unsigned int i = 0; i < numreadbufs; i++)
            {
                if (!(hcdbatch[i].allocateVolDataAndHcd(nhcdsinbatch))) 
                {
                    bretval = false;
                    break;
                }
            }
            return bretval;
	}

    bool allocateReadBufAndMsgBlks(unsigned int length)
	{
            bool bretval = true;
            for (unsigned int i = 0; i < numreadbufs; i++)
            {
                if (!(hcdbatch[i].allocateReadBuffer(length)))
                {
                    bretval = false;
                    break;
                }    
                mb[i] = new ACE_Message_Block((char *) (hcdbatch + i), sizeof (VolumeDataAndHCDBatch_t));
                if (!mb[i])
                {
                    bretval = false;
                    break;
                }
                mb[i]->msg_priority(MSGQUEUE_NORMAL_PRIORITY);
            }
            return bretval;
	}

};

typedef VolumeReadBuffers VolumeReadBuffers_t;

typedef std::vector<ACE_Message_Block *> MessageBlocks_t;

class HashCompareData;
class SyncDataNeededInfo;

typedef boost::shared_ptr<SharedBitMap> SharedBitMapPtr;

enum FASTSYNC_MSG_TYPE { MSG_QUIT, MSG_PROCESS };
struct FastSyncMsg
{
    FASTSYNC_MSG_TYPE type;
    FileInfosPtr fileInfos;
    size_t index; // index into the file list
    size_t skip; // no. of files to skip
    size_t maxIndex;
};
typedef struct FastSyncMsg FastSyncMsg_t;

struct ChecksumFile
{
    std::string fileName;
    boost::shared_ptr<HashCompareData> hashCompareData;
};
typedef struct ChecksumFile ChecksumFile_t;

struct ResyncStats
{
    ResyncStats() : m_ResyncProcessedBytes(-1),
        m_ResyncTotalTransferredBytes(-1),
        m_ResyncLast15MinutesTransferredBytes(-1),
        m_MatchedBytesRatio(-1){}

    uint64_t m_ResyncProcessedBytes;
    uint64_t m_ResyncTotalTransferredBytes;
    uint64_t m_ResyncLast15MinutesTransferredBytes;
    uint32_t m_MatchedBytesRatio;
    std::string m_ResyncLastDataTransferTimeUtcPtimeIsoStr;

public:
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "ResyncStats", false);
        JSON_E(adapter, m_ResyncProcessedBytes);
        JSON_E(adapter, m_ResyncTotalTransferredBytes);
        JSON_E(adapter, m_ResyncLast15MinutesTransferredBytes);
        JSON_E(adapter, m_MatchedBytesRatio);
        JSON_T(adapter, m_ResyncLastDataTransferTimeUtcPtimeIsoStr);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, m_ResyncProcessedBytes);
        JSON_P(node, m_ResyncTotalTransferredBytes);
        JSON_P(node, m_ResyncLast15MinutesTransferredBytes);
        JSON_P(node, m_MatchedBytesRatio);
        JSON_P(node, m_ResyncLastDataTransferTimeUtcPtimeIsoStr);
    }
};

class ResyncStatsForCSLegacy
{
public:
    uint64_t ResyncProcessedBytes;
    std::string ResyncPercentage;
    uint64_t ResyncTotalTransferredBytes;
    uint64_t ResyncLast15MinutesTransferredBytes;
    std::string ResyncLastDataTransferTimeUTC;
    std::string ResyncStartTimeUTC;

    ResyncStatsForCSLegacy() : ResyncProcessedBytes(-1),
        ResyncTotalTransferredBytes(-1),
        ResyncLast15MinutesTransferredBytes(-1) {}

public:
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "ResyncStatsForCSLegacy", false);
        JSON_E(adapter, ResyncProcessedBytes);
        JSON_E(adapter, ResyncPercentage);
        JSON_E(adapter, ResyncTotalTransferredBytes);
        JSON_E(adapter, ResyncLast15MinutesTransferredBytes);
        JSON_E(adapter, ResyncLastDataTransferTimeUTC);
        JSON_T(adapter, ResyncStartTimeUTC);
    }
};

/**
 * CxTransport:
 * 1. All classes in fast sync have their own CxTransport objects
 */

struct ResyncProgressCounters
{
    SharedBitMapPtr m_ChecksumProcessedMap, m_SyncDataAppliedMap;

    uint64_t GetFullyMatchedBytesFromProcessHcdTask() const
    {
        if (!m_ChecksumProcessedMap)
        {
            throw INMAGE_EX("ResyncProgressCounters not initialized - m_ChecksumProcessedMap is null.");
        }
        return m_ChecksumProcessedMap->getBytesProcessed();
    }

    uint64_t GetAppliedBytesFromProcessSyncDataTask() const
    {
        if (!m_SyncDataAppliedMap)
        {
            throw INMAGE_EX("ResyncProgressCounters not initialized - m_SyncDataAppliedMap is null.");
        }
        return m_SyncDataAppliedMap->getBytesProcessed();
    }

    uint64_t GetPartialMatchedBytesFromProcessSyncDataTask() const
    {
        if (!m_SyncDataAppliedMap)
        {
            throw INMAGE_EX("ResyncProgressCounters not initialized - m_SyncDataAppliedMap is null.");
        }
        return m_SyncDataAppliedMap->getBytesNotProcessed();
    }

    uint64_t GetResyncProcessedBytes() const;
};

class FastSync : public DataProtectionSync {
public:
    explicit FastSync(VOLUME_GROUP_SETTINGS const & volumeGrpSettings, const AGENT_RESTART_STATUS agentRestartStatus);

    void Start();
    void UpdateVolumeGroupSettings(VOLUME_GROUP_SETTINGS& volumeGroupSettings) { } // TODO: implement

    //Added by BSR for FAST SYNC TBC
    void setVolumeSize( SV_ULONGLONG volSize );
    //end of the change
protected:

    typedef boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > ACE_Shared_MQ;
    typedef ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONG> SynchronousCount;

    class ResyncProgressInfo
    {
    public:
        explicit ResyncProgressInfo(FastSync* fastSync) : m_FastSync(fastSync),
            m_CxTransport(boost::make_shared<CxTransport>(m_FastSync->Protocol(),
                m_FastSync->TransportSettings(), m_FastSync->Secure(),
                m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
            m_PrevHeartBeatTime(boost::chrono::steady_clock::now()){}

        void Heartbeat();

        bool ListFile(const std::string& fileSpec, FileInfosPtr &fileInfos, std::string &errMsg);

        bool DeleteFile(const std::string& remoteFileName, std::string &errMsg);

        bool GetResyncProgressCounters(ResyncProgressCounters& resyncProgressCounters, std::string &errmsg);

    private:
        ResyncProgressCounters m_ResyncProgressCounters;
        FastSync* m_FastSync;
        CxTransport::ptr m_CxTransport;
        mutable boost::mutex m_CxTransportLock;
        boost::chrono::steady_clock::time_point m_PrevHeartBeatTime;
    };

    class GenerateHCDWorker;
    class GenerateHcdTask: public ACE_Task<ACE_MT_SYNCH> {
    public:
        GenerateHcdTask(FastSync* fastSync, ACE_Thread_Manager* threadManager)
            : ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_FastSync(fastSync),
              m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
              m_volume( m_FastSync->createVolume() )  { }

        GenerateHcdTask(FastSync* fastSync, SharedBitMapPtr checksumTransferMap, ACE_Thread_Manager* threadManager)
            : ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_FastSync(fastSync),
              m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
              m_checksumTransferMap( checksumTransferMap ),
              m_volume( m_FastSync->createVolume() )  { }
        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);

        void setPendingChunkCount( const SV_ULONG& count )
            {
                m_pendingChunkCount = count;
            }

        SV_ULONG pendingChunkCount() const
            {
                return m_pendingChunkCount.value();
            }

        SharedBitMap* checksumTransferMap()
            {
                return m_checksumTransferMap.get();
            }

        void setprevFullSyncBytesSent( const SV_ULONGLONG& value )
            {
                m_prevFullSyncBytesSent = value;
            }

    protected:
        bool Quit();
        void GenerateHcdFiles();
        bool canSendHCDs();

    private:
        FastSync* m_FastSync;
        SynchronousCount m_pendingChunkCount;
        CxTransport::ptr m_cxTransport;
        cdp_volume_t::Ptr m_volume;
        SharedBitMapPtr  m_checksumTransferMap;
        typedef boost::shared_ptr<GenerateHCDWorker> GenerateHCDWorkerPtr;
        typedef std::list<GenerateHCDWorkerPtr> GenerateHCDWorkerTasks_t;
        GenerateHCDWorkerTasks_t m_genTasks;
        ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> m_prevFullSyncBytesSent;
    };

    class GenerateHCDWorker: public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        GenerateHCDWorker( FastSync* fastSync,
                           ACE_Thread_Manager* threadManager,
                           ACE_Shared_MQ chunkQueue,
                           SynchronousCount& pendingChunkCount ) :
            m_FastSync( fastSync),
            ACE_Task<ACE_MT_SYNCH>( threadManager ),
            m_chunkQueue( chunkQueue ),
            m_pendingChunkCount( pendingChunkCount ) ,
            m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
            m_volume( m_FastSync->createVolume() ),
            m_pHcdFileUpload(NEWPROFILER(HCDFILEUPLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), false))
        {
            if (m_FastSync->Settings().isAzureDataPlane()){
                m_pGetHcdAzureAgent = NEWPROFILER_WITHTSBKT(GETHCDAZUREAGENT, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
            }
            else
            {
                m_pDiskRead = NEWPROFILER_WITHTSBKT(DISKREAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
                m_pHcdCalc = NEWPROFILER(HCDCALC, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
            }
        }

        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);

    protected:
        void GenerateHashCompareData( FastSyncMsg_t* msg );

    private:
        FastSync* m_FastSync;
        ACE_Shared_MQ m_chunkQueue;
        SynchronousCount &m_pendingChunkCount;
        CxTransport::ptr m_cxTransport;
        cdp_volume_t::Ptr m_volume;
        Profiler m_pHcdFileUpload;
        Profiler m_pGetHcdAzureAgent;
        Profiler m_pDiskRead;
        Profiler m_pHcdCalc;
    };

    typedef boost::shared_ptr<GenerateHCDWorker> GenerateHCDWorkerPtr;
    typedef std::list<GenerateHCDWorkerPtr> GenerateHCDWorkerTasks_t;

    class ProcessHcdTask: public ACE_Task<ACE_MT_SYNCH> {
    public:
        ProcessHcdTask(FastSync* fastSync, SharedBitMapPtr checksumProcessedMap, ACE_Thread_Manager* threadManager)
            : ACE_Task<ACE_MT_SYNCH>(threadManager),
            m_FastSync(fastSync),
            m_checksumProcessedMap(checksumProcessedMap),
            m_volume(m_FastSync->createVolume()),
            m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
            m_readThreadToPcstQ(new ACE_Message_Queue<ACE_MT_SYNCH>()),
            m_sendToReadThreadQ(new ACE_Message_Queue<ACE_MT_SYNCH>()),
            m_pcstToSendThreadQ(new ACE_Message_Queue<ACE_MT_SYNCH>()),
            m_clusterSize(0),
            m_pendingFileCount(0),
            m_pDiskRead(NEWPROFILER_WITHTSBKT(DISKREAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true)),
            m_pHcdList(NEWPROFILER(HCDLIST, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), false)),
            m_pHcdDownoad(NEWPROFILER(HCDDOWNLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), false)) {}

        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);

        void setPendingFileCount( const SV_ULONG& count )
            {
                m_pendingFileCount = count;
            }
        SV_ULONG pendingFileCount()
            {
                return m_pendingFileCount.value();
            }
        void setPrevMatchedBytes( const SV_ULONGLONG& value )
            {
                m_prevMatchedBytesSent = value;
            }

        typedef enum eMissingSyncProcessStatus
        {
            ERRORPROCESSINGSYNCMISSING,    /* any error while processing sync missing; throw exception on this */
            SYNCMISSINGABSENT,             /* sync missing not present; can go down */
            CHUNKSPENDING,                 /* using sync missing, hcd process map updated; there are pending chunks; come down */
            TRANSITIONCOMPLETE             /* transition complete; break */

        } MissingSyncProcessStatus;

        void SendMissingHcdsPacket( );
        MissingSyncProcessStatus ProcessMissingSyncsPacket();
        MissingSyncProcessStatus receiveProcessMissingSyncPacket();
        bool isHcdMissingPacketAvailable();
        int ChunksToBeProcessed( SharedBitMapPtr& );
        std::string getHcdProcessMapFileName();
        bool isHcdProcessed(SV_ULONGLONG offset, CxTransport::ptr cxTransport, string deleteName);
        void deleteFromOffsetsInProcess(SV_ULONGLONG offset);
        void updateProgress( const time_t &intervalTime );
        void markAsMatched(SV_ULONGLONG offset, SV_ULONG bytes)
            {
                m_checksumProcessedMap->markAsMatched( offset, bytes );
            }

        void markAsUnMatched(SV_ULONGLONG offset, SV_ULONG bytes = 0 )
            {
                m_checksumProcessedMap->markAsUnMatched( offset, bytes );
            }
        bool compareAndProcessHCD( ChecksumFile_t* csfile );
        bool processHCDBatch(
            unsigned int &HCDToProcess, 
            HashCompareData::iterator &hcdIter, 
            boost::shared_ptr<HashCompareData> &hashCompareData, 
            VolumeDataAndHCDBatch_t *pvVolDataAndHCDBatch,
            ACE_Message_Block *mb,
            const std::string &deleteName
                             );
        void getHcdCountToProcess(
            const unsigned int &HCDToProcess, 
            const HashCompareData::iterator &hcdIter, 
            unsigned int &countofhcd,
            unsigned int &length
                                  );
        void FillHcdBatchToProcess(
            const unsigned int &countofhcd, 
            const std::string &deletename, 
            const unsigned int &numHCDs,
            const HashCompareData::iterator &hcdbegin, 
            char *volData, 
            VolumeDataAndHCDBatch_t *pvDataAndHcdFirstBatch, 
            HashCompareData::iterator &hcdIter
                                   );
        bool readFromVolume(SV_LONGLONG offset, unsigned int length, char *pbuffer);
        bool allocateHCDBatch(void);
        bool allocateReadBufAndMsgBlks(unsigned int length)
            {
                return m_volReadBuffers.allocateReadBufAndMsgBlks(length);
            }
        void setClusterSize(void);
        SV_ULONGLONG GetFullyUnUsedBytes(void);

    protected:
        bool Quit();

    private:
        FastSync* m_FastSync;
        SynchronousCount m_pendingFileCount;
        SharedBitMapPtr m_checksumProcessedMap;
        ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> m_prevMatchedBytesSent;
        CxTransport::ptr m_cxTransport;
        cdp_volume_t::Ptr m_volume ;
        VolumeReadBuffers_t m_volReadBuffers;
        ACE_Shared_MQ m_readThreadToPcstQ;
        ACE_Shared_MQ m_sendToReadThreadQ;
        ACE_Shared_MQ m_pcstToSendThreadQ;
        ACE_Recursive_Thread_Mutex m_OffsetsInProcessMutex;
        Offsets_t m_OffsetsInProcess;
        SV_UINT m_clusterSize;
        Profiler m_pDiskRead;
        Profiler m_pHcdList;
        Profiler m_pHcdDownoad;
    };

    typedef enum eHeaderStatus
    {
        E_NEEDTOSEND,
        E_SENT

    } E_HEADERSTATUS;

    class SendSyncDataWorker: public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        SendSyncDataWorker( FastSync* fastSync, 
                            ACE_Thread_Manager* threadManager,
                            ACE_Shared_MQ volReadQ,  
                            ACE_Shared_MQ pcstQ,
                            SynchronousCount& pendingCount
                            ):  
            m_FastSync(fastSync),
            ACE_Task<ACE_MT_SYNCH>(threadManager),                  
            m_volReadQ(volReadQ),
            m_pcstQ(pcstQ), 
            m_compressor( CompressionFactory::CreateCompressor( ZLIB ) ),
            m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
            m_eSyncFileHeaderStatus(E_NEEDTOSEND),
            m_bytesMatched(0),
            m_bytesInHcd(0),
            m_pSyncUpload(NEWPROFILER_WITHNORMANDTSBKT(SYNCUPLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true)),
            m_pendingFileCount(pendingCount)
            {
                m_compressMode = m_FastSync->Settings().compressionEnable;
                ResetPrevHcd();
            }

        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();

        bool IsCompressionEnabled(void) 
            {
                return (COMPRESS_SOURCESIDE == m_compressMode);
            } 

        COMPRESS_MODE CompressMode()
            {
                return m_compressMode;
            }
        bool SendUnMatchedSyncData(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch);
        void SendSyncData(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch, const unsigned int &i);
        bool IsContinuousWithinLimit(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch, const unsigned int &i,
                                     unsigned long lim);
        void RecordPrev(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch, const unsigned int &i);
        void SendPrevIfReq(void);
        bool SendSyncData(
            SV_LONGLONG offset, 
            SV_UINT length, 
            char *voldatatosend
                          );
        std::string GetCSRecord(char *buffer, SV_LONGLONG offset, SV_UINT length);
        void ResetPrevHcd(void)
            {
                m_PrevOffset = -1;
                m_PrevLength = 0;
                m_PrevVolData = 0;
            }

    private:
        FastSync* m_FastSync;
        ACE_Shared_MQ m_volReadQ;  
        ACE_Shared_MQ m_pcstQ;
        boost::shared_ptr<Compress> m_compressor ;
        CxTransport::ptr m_cxTransport;
        std::string m_preTarget;
        std::string m_target;
        COMPRESS_MODE m_compressMode;
        SV_ULONG m_bytesMatched;
        SV_ULONG m_bytesInHcd;
        E_HEADERSTATUS m_eSyncFileHeaderStatus;
        std::string m_CheckSumsToLog;
        SV_LONGLONG m_StartOffsetOfHCD;
        SynchronousCount &m_pendingFileCount;
        SV_LONGLONG m_PrevOffset;
        SV_UINT m_PrevLength;
        char *m_PrevVolData;
        Profiler m_pSyncUpload;
    };

    typedef boost::shared_ptr<SendSyncDataWorker> SendSyncDataWorkerPtr;
    typedef std::list<SendSyncDataWorkerPtr> SendSyncDataTasks_t;

    class PrimaryCSWorker: public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        PrimaryCSWorker( FastSync* fastSync, 
                         ACE_Thread_Manager* threadManager,
                         ACE_Shared_MQ volReadQ,  
                         ACE_Shared_MQ sendQ
                         ):  
            m_FastSync(fastSync),
            ACE_Task<ACE_MT_SYNCH>(threadManager),                  
            m_volReadQ(volReadQ),
            m_sendQ(sendQ),
            m_threadID(0),
            m_pHcdCalc(NEWPROFILER(HCDCALC, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true)) {}

        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();

    private:
        FastSync* m_FastSync ;
        ACE_Shared_MQ m_volReadQ;  
        ACE_Shared_MQ m_sendQ;
        unsigned int m_threadID;
        Profiler m_pHcdCalc;
    };

    typedef boost::shared_ptr<PrimaryCSWorker> PrimaryCSWorkerPtr;
    typedef std::list<PrimaryCSWorkerPtr> PrimaryCSTasks_t;

    class CSWorker: public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        CSWorker( FastSync* fastSync, 
                  ACE_Thread_Manager* threadManager,
                  ACE_Shared_MQ hcQ,  
                  ACE_Shared_MQ ackQ,
                  unsigned int threadID
                  ):  
            m_FastSync(fastSync),
            ACE_Task<ACE_MT_SYNCH>(threadManager),                  
            m_hcQ(hcQ),
            m_ackQ(ackQ),
            m_threadID(threadID),
            m_pHcdCalc(NEWPROFILER(HCDCALC, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true))
            {
            }

        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();

    private:
        FastSync* m_FastSync ;
        ACE_Shared_MQ m_hcQ;  
        ACE_Shared_MQ m_ackQ;
        unsigned int m_threadID;
        Profiler m_pHcdCalc;
    };

    typedef boost::shared_ptr<CSWorker> CSWorkerPtr;
    typedef std::list<CSWorkerPtr> CSTasks_t;

    class ProcessSyncDataTask : public ACE_Task<ACE_MT_SYNCH> {
    public:
        ProcessSyncDataTask(FastSync* fastSync, ACE_Thread_Manager* threadManager)
            : ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_FastSync(fastSync)
            {}
        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);
        bool PurgeRetentionIfRequired();

    protected:
        bool Quit();

    private:
        FastSync* m_FastSync;
    };

    class FastSyncApplyTask : public ACE_Task<ACE_MT_SYNCH> {
    public:

        FastSyncApplyTask( FastSync* fastSync, ACE_Thread_Manager* threadManager,
                           ACE_Shared_MQ getmq )
            : m_FastSync(fastSync),
              ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_getmq(getmq)
        {
            m_pSyncDownload = NEWPROFILER_WITHNORMANDTSBKT(SYNCDOWNLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
            if (m_FastSync->Settings().isAzureDataPlane())
            {
                int maxSyncChunkSize = m_FastSync->GetConfigurator().getFastSyncMaxChunkSizeForE2A();
                m_pSyncUncompress = ProfilerFactory::getProfiler(GENERIC_PROFILER, SYNCUNCOMPRESS, new NormProfilingBuckets(maxSyncChunkSize, SYNCUNCOMPRESS), m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
                m_pSyncApply = NEWPROFILER_WITHNORMANDTSBKT(SYNCUPLOADTOAZURE, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
            }
            else
            {
                int maxSyncChunkSize = m_FastSync->GetConfigurator().getFastSyncMaxChunkSize();
                m_pSyncUncompress = ProfilerFactory::getProfiler(GENERIC_PROFILER, SYNCUNCOMPRESS, new NormProfilingBuckets(maxSyncChunkSize, SYNCUNCOMPRESS), m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
                m_pSyncApply = NEWPROFILER_WITHNORMANDTSBKT(SYNCAPPLY, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true);
            }
        }

        virtual int open(void *args =0);
        virtual int close(u_long flags =0);
        virtual int svc();

    protected:
        virtual void processFiles(CxTransport::ptr &cxTransport, cdp_volume_t::Ptr volptr, CDPV2Writer::Ptr cdpv2writer, FastSyncMsg_t * msg);

    private:

        FastSync* m_FastSync;
        ACE_Shared_MQ m_getmq;
        Profiler m_pSyncDownload;
        Profiler m_pSyncUncompress;
        Profiler m_pSyncApply;
    };

    typedef boost::shared_ptr<FastSyncApplyTask> FastSyncApplyTaskPtr;
    typedef std::list<FastSyncApplyTaskPtr> FastSyncApplyTasks_t;

    class ClusterParams
    {
    public:
        ClusterParams(FastSync *fastSync);
        unsigned long getClusterSyncChunkSize(void) 
            {
                return m_clusterSyncChunkSize;
            }
        SV_ULONG totalChunks(void);
        bool AreValid(std::ostream &msg);
        bool initSharedBitMapFile(boost::shared_ptr<SharedBitMap>&  bitmap, 
                                  const string& fileName, CxTransport::ptr &cxTransport,
                                  const bool & bShouldCreateIfNotPresent);
        bool IsClusterTransferMapValid(SharedBitMapPtr &unprocessedClusterMap, 
                                       std::ostringstream &msg);
    private:
        FastSync *m_FastSync;
        unsigned long m_clusterSyncChunkSize;
    };

    typedef boost::shared_ptr<ClusterParams> ClusterParamsPtr;

    class GenerateClusterInfoWorker;
    class GenerateClusterInfoTask: public ACE_Task<ACE_MT_SYNCH> {
    public:
        GenerateClusterInfoTask(FastSync* fastSync, ACE_Thread_Manager* threadManager);
        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();

        void setPendingChunkCount( const SV_ULONG& count )
            {
                m_pendingChunkCount = count;
            }

        SV_ULONG pendingChunkCount() const
            {
                return m_pendingChunkCount.value();
            }

        SharedBitMap* clusterTransferMap()
            {
                return m_clusterTransferMap.get();
            }

        void setprevFullSyncBytesSent( const SV_ULONGLONG& value )
            {
                m_prevFullSyncBytesSent = value;
            }

        SV_UINT GetMaxClusterBitmapsAllowdAtCx(void) const;
        SV_UINT GetSecsToWaitForClusterBitmapSend(void) const;
        void nextClusterFileNames( std::string& preTarget,
                                   std::string& target );
        void formatClusterNoMoreFileName( std::string& hcdNoMore );
        bool isClusterBitTransferred(SV_ULONGLONG offset);
        unsigned long getClusterSyncChunkSize(void) 
            {
                return m_FastSync->GetClusterParams()->getClusterSyncChunkSize();
            }

    protected:
        bool Quit();
        void GenerateClusterBitmaps();
        bool canSendClusterBitmaps();
        void sendClusterNoMoreIfRequired();
        bool SendClusterNoMoreFile(CxTransport::ptr &cxTransport);
        bool IsUnProcessedClusterMapPresent(void);
        void LoadProcessedClusterMapIfPresent(void);
        void LoadProcessedClusterMap(void);
        void sendUnProcessedClusterIfRequired(void);

    private:
        FastSync* m_FastSync;
        SynchronousCount m_pendingChunkCount;
        CxTransport::ptr m_cxTransport;
        SharedBitMapPtr  m_clusterTransferMap;
        SV_UINT m_maxClusterBitmapsAllowdAtCx;
        SV_UINT m_secsToWaitForClusterBitmapSend;
        typedef boost::shared_ptr<GenerateClusterInfoWorker> GenerateClusterInfoWorkerPtr;
        typedef std::list<GenerateClusterInfoWorkerPtr> GenerateClusterInfoWorkerTasks_t;
        GenerateClusterInfoWorkerTasks_t m_workerTasks;
        mutable ACE_Mutex m_FileNameLock;
        ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> m_prevFullSyncBytesSent;
    };


    class GenerateClusterInfoWorker: public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        GenerateClusterInfoWorker( FastSync* fastSync,
                                   ACE_Thread_Manager* threadManager,
                                   ACE_Shared_MQ chunkQueue,
                                   SynchronousCount& pendingChunkCount, 
                                   GenerateClusterInfoTask *pClusterInfoTask
                                   );
        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);
        SV_UINT getClusterSize(void)
            {
                return m_clusterSize;
            }

    protected:
        void GenerateClusterBitmap( FastSyncMsg_t* msg );
        void GenerateClusterBitmap(const SV_LONGLONG* offset);

    private:
        FastSync* m_FastSync;
        ACE_Shared_MQ m_chunkQueue;
        SynchronousCount &m_pendingChunkCount;
        CxTransport::ptr m_cxTransport;
        cdp_volume_t::Ptr m_volume;
        GenerateClusterInfoTask *m_pClusterInfoTask;
        SV_UINT m_clusterSize;
        cdp_volume_util m_cdpVolumeUtil;
        Profiler m_pClusterUpload;

    private:
        void setClusterSize(void);
    };

    class ProcessClusterStage;
    class GenerateHcdStage;
    class SendHcdStage;

    class ProcessClusterTask : public ACE_Task<ACE_MT_SYNCH> {
    public:
        ProcessClusterTask( FastSync* fastSync, ACE_Thread_Manager* threadManager, 
                            ProcessClusterStage *pProcessClusterStage, 
                            GenerateHcdStage *pGenerateHcdStage, 
                            SendHcdStage *pSendHcdStage )
            : m_FastSync(fastSync),
              ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_pProcessClusterStage(pProcessClusterStage),
              m_pGenerateHcdStage(pGenerateHcdStage),
              m_pSendHcdStage(pSendHcdStage)
            {}

        virtual int open(void *args =0);
        virtual int close(u_long flags =0);
        virtual int svc();

    private:
        FastSync* m_FastSync;
        ProcessClusterStage *m_pProcessClusterStage;
        GenerateHcdStage *m_pGenerateHcdStage;
        SendHcdStage *m_pSendHcdStage;
    };

    class ProcessClusterStage: public InmPipeline::Stage {
    public:
        ProcessClusterStage(FastSync* fastSync, SharedBitMapPtr checksumTransferMap);

        typedef bool (FastSync::ProcessClusterStage::*QuitRequested_t)(long int seconds);
        typedef ClusterInfoForHcd * (FastSync::ProcessClusterStage::*GetClusterInfoForRead_t)(void);
        typedef void (FastSync::ProcessClusterStage::*SendClusterInfoToHcdGen_t)(ClusterInfoForHcd *);

        int svc();
        bool isHcdTransferred(SV_ULONGLONG offset);
        bool isHcdTransferred(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster,
                              const SV_UINT &clusterstoprocess);
        void WaitUntilHcdTransferred(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster,
                                     const SV_UINT &clusterstoprocess);
        void setPendingFileCount( const SV_ULONG& count );
        SV_ULONG pendingFileCount();
        SharedBitMap* checksumTransferMap();
        unsigned long getClusterSyncChunkSize(void) ;
        void updateProgress(void) ;
        void markAsFullyUsed(SV_ULONGLONG offset);
        void markAsFullyUnUsed(SV_ULONGLONG offset, SV_ULONG unusedbytes = 0);
        void markAsPartiallyUsed(SV_ULONGLONG offset);
        void SetGenAndSendHcdStage(GenerateHcdStage *pgenhcdstage, SendHcdStage *psendhcdstage);
        void DecrementPendingCount(void);
        void setPrevUnUsedBytes( const SV_ULONGLONG& value )
            {
                m_prevFullyUnUsedBytes = value;
            }

    protected:
        bool Quit();

    private:
        FastSync* m_FastSync;
        SharedBitMapPtr m_checksumTransferMap ;
        CxTransport::ptr m_cxTransport;
        ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> m_prevFullyUnUsedBytes;
        QuitRequested_t m_quitRequest[NBOOLS];
        GetClusterInfoForRead_t m_clusterInfoForRead[NBOOLS];
        SendClusterInfoToHcdGen_t m_sendClusterInfoToHcdGen[NBOOLS];
        cdp_volume_t::Ptr m_volume;
        ClusterInfosForHcd m_ClusterInfosForHcd;
        GenerateHcdStage *m_pGenerateHcdStage;
        SendHcdStage *m_pSendHcdStage;
        SynchronousCount m_pendingFileCount;
        Profiler m_pDiskRead;

    private:
        void process(ClusterFilePtr_t &pclusterfile);
        void ProcessClustersInHcd(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster, 
                                  const SV_UINT &clusterstoprocess, const SV_UINT &nclustersinreadbuffer);
        void ProcessClustersInRead(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster, 
                                   const SV_UINT &clusterstoprocess, const ClustersInfo_t &clustersinhcd);
        ClusterInfoForHcd * GetClusterInfoForRead(void);
        ClusterInfoForHcd * GetClusterInfoForReadInStage(void);
        ClusterInfoForHcd * GetClusterInfoForReadInApp(void);
        void SendClusterInfoToHcdGen(ClusterInfoForHcd *pcihcd);
        void SendClusterInfoToHcdGenInStage(ClusterInfoForHcd *pcihcd);
        void SendClusterInfoToHcdGenInApp(ClusterInfoForHcd *pcihcd);
        bool QuitRequested(long int seconds = 0);
        bool QuitRequestedInStage(long int seconds);
        bool QuitRequestedInApp(long int seconds);
        void InformStatus(const std::string &s);
        bool AllocateClusterBatchIfReq(ClusterFilePtr_t &pclusterfile);
        SV_ULONG GetClusterFilesToProcess(FileInfosPtr &fileInfos, FileInfosPtr &fileInfosToProcess);
        bool canSendHcds(void);
    };

    class HCDComputer: public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        HCDComputer( FastSync* fastSync,
                     ACE_Thread_Manager* threadManager,
                     GenerateHcdStage *pGenHcdStage,
                     ACE_Shared_MQ HcdGenQ,
                     ACE_Shared_MQ HcdAckQ )
            : m_FastSync( fastSync),
              ACE_Task<ACE_MT_SYNCH>( threadManager ),
              m_pGenerateHcdStage(pGenHcdStage),
              m_hcdGenQ( HcdGenQ ),
              m_hcdAckQ( HcdAckQ ),
              m_pHcdCalc(NEWPROFILER(HCDCALC, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true))
            {
            }

        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);

    private:
        FastSync* m_FastSync;
        ACE_Shared_MQ m_hcdGenQ;
        ACE_Shared_MQ m_hcdAckQ;
        GenerateHcdStage *m_pGenerateHcdStage;
        Profiler m_pHcdCalc;
    };
    typedef boost::shared_ptr<HCDComputer> HCDComputerPtr;
    typedef std::list<HCDComputerPtr> HCDComputerTasks_t;

    class GenerateHcdStage: public InmPipeline::Stage {
    public:
        GenerateHcdStage(FastSync* fastSync);
        typedef bool (FastSync::GenerateHcdStage::*QuitRequested_t)(long int seconds);
        typedef void (FastSync::GenerateHcdStage::*SendBackClusterInfo_t)(ClusterInfoForHcd *pcihcd);
        typedef HcdInfoToSend * (FastSync::GenerateHcdStage::*GetHcdInfoToSend_t)(void);
        typedef void (FastSync::GenerateHcdStage::*ForwardHcdInfoToSend_t)(HcdInfoToSend *phcdinfo);
        int svc();
        void process(ClusterInfoForHcd *pcihcd);
        bool Init(std::ostream &excmsg);
        void DeInit(void);
        ACE_Message_Block * retrieveMessage( ACE_Shared_MQ& sharedQueue );
        void UpdateHCD(HcdGenInfo *phcdgeninfo, Profiler &prf);
        bool QuitRequested(long int seconds = 0);
        void SetSendHcdStage(SendHcdStage *psendhcdstage);

    protected:
        bool Quit();

    private:
        FastSync* m_FastSync;
        QuitRequested_t m_quitRequest[NBOOLS];
        SendBackClusterInfo_t m_sendBackClusterInfo[NBOOLS];
        HcdInfosToSend m_HcdInfosToSend;
        SV_UINT m_NumGenHcdThreads;
        ACE_Thread_Manager m_ThreadManager;
        HCDComputerTasks_t m_workerTasks;
        ACE_Shared_MQ m_hcdGenQ;
        ACE_Shared_MQ m_hcdAckQ;
        HcdGenInfo *m_pHcdGenInfo;
        GetHcdInfoToSend_t m_hcdInfoToSend[NBOOLS];
        ForwardHcdInfoToSend_t m_forwardHcdInfoToSend[NBOOLS];
        MessageBlocks_t m_mesgBlks;
        SendHcdStage *m_pSendHcdStage;
        Profiler m_pHcdCalc;

    private:
        bool QuitRequestedInStage(long int seconds);
        bool QuitRequestedInApp(long int seconds);
        void InformStatus(const std::string &s);
        void SendBackClusterInfo(ClusterInfoForHcd *pcihcd);
        void SendBackClusterInfoInStage(ClusterInfoForHcd *pcihcd);
        void SendBackClusterInfoInApp(ClusterInfoForHcd *pcihcd);
        bool AllocateHcdBufferIfReq(ClusterInfoForHcd *pcihcd);
        HcdInfoToSend * GetHcdInfoToSend(void);
        HcdInfoToSend * GetHcdInfoToSendInStage(void);
        HcdInfoToSend * GetHcdInfoToSendInApp(void);
        void FillHcdBufferInGenInfo(HcdInfoToSend *phcdinfo);
        void GenerateHcd(const unsigned int &ngeninfos);
        void Idle(int seconds);
        void FormHcd(const SV_UINT &startingcluster, const SV_UINT &count, HcdGenInfo *phcdgeninfo);
        void FillHcdInfoToSend(HcdInfoToSend *phcdinfo, ClusterInfoForHcd *pcihcd);
        void FillClusterFileDetails(HcdInfoToSend *phcdinfo, ClusterInfoForHcd *pcihcd);
        void ForwardHcdInfoToSend(HcdInfoToSend *phcdinfo);
        void ForwardHcdInfoToSendInStage(HcdInfoToSend *phcdinfo);
        void ForwardHcdInfoToSendInApp(HcdInfoToSend *phcdinfo);
    };


    class SendHcdStage: public InmPipeline::Stage
    {
    public:
        SendHcdStage(FastSync* fastSync, ProcessClusterStage *processClusterStage);
        typedef bool (FastSync::SendHcdStage::*QuitRequested_t)(long int seconds);
        typedef void (FastSync::SendHcdStage::*SendBackHcdInfo_t)(HcdInfoToSend *phcdinfo);
        int svc();
        void process(HcdInfoToSend *phcdinfo);

    private:
        FastSync* m_FastSync ;
        ProcessClusterStage *m_pProcessClusterStage;
        CxTransport::ptr m_cxTransport;
        QuitRequested_t m_quitRequest[NBOOLS];
        SendBackHcdInfo_t m_sendBackHcdInfo[NBOOLS];
        std::string m_PreFileName;
        std::string m_FileName;
        SV_ULONG m_UsedBytes;
        SV_ULONG m_UnUsedBytes;
        HashCompareData m_HashCompareData;
        E_HEADERSTATUS m_eHcdFileHeaderStatus;
        Profiler m_pHcdUpload;

    private:
        bool QuitRequested(long int seconds);
        bool QuitRequestedInStage(long int seconds);
        bool QuitRequestedInApp(long int seconds);
        void SendBackHcdInfo(HcdInfoToSend *phcdinfo);
        void SendBackHcdInfoInStage(HcdInfoToSend *phcdinfo);
        void SendBackHcdInfoInApp(HcdInfoToSend *phcdinfo);
        void InformStatus(const std::string &s);
        void SendHcdWithHeaderIfReq(HcdInfoToSend *phcdinfo);
        void SendHcdTrailerIfReq(HcdInfoToSend *phcdinfo);
        void ThrottleSpace(void);
    };
    
    virtual void CleanupResyncStaleFilesOnPS();

    virtual void CheckResyncState() const { /* Dummy impl - overloaded in FastSyncRcm */ }

    virtual bool FailResync(const std::string resyncFailReason, const bool restartResync = false) const { /* Dummy impl - overloaded in FastSyncRcm */ return true; }

    virtual void RetrieveCdpSettings();

    virtual void HandleAzureCancelOpException(const std::string & msg) const;

    virtual void HandleAzureInvalidOpException(const std::string & msg, const long errorCode) const;

    virtual void HandleInvalidBitmapError(const std::string & msg) const;

    virtual void CompleteResync() const { /* Dummy impl - overloaded in FastSyncRcm */ }

    virtual bool IsNoDataTransferSync() const 
    { 
        /* Dummy impl - overloaded in FastSyncRcm */ 
        return false;
    }

    virtual bool IsMarsCompatibleWithNoDataTranfer() const
    {
        /* Dummy impl - overloaded in FastSyncRcm */
        return false;
    }

    virtual void getResyncTimeSettingsForNoDataTransfer(ResyncTimeSettings& resyncTimeSettings) const
    {
        /* Dummy impl - overloaded in FastSyncRcm */
    }

    virtual void UpdateResyncProgress() { /* Dummy impl - overloaded in FastSyncRcm */ }

    virtual void MonitorTargetToAzureHealth() { /* Dummy impl - overloaded in FastSyncRcm */ }

    virtual void DumpResyncProgress();

    virtual bool IsResyncDataAlreadyApplied(SV_ULONGLONG* offset, SharedBitMap*bm, DiffPtr change_metadata)
    {
        /* Dummy impl - overloaded in FastSyncRcm */
        return false;
    }

    template <class Task1, class Task2> void WaitForSyncTasks(Task1& task1, Task2& task2);

    //Added by BSR for Fast Sync TBC
    template <class Task1> void waitForSyncTasks( Task1& task, InmPipeline *p);
    //End of the

    void PostMsg(int msgId, int priority);
    void SyncStart();
    void SendHashCompareData(boost::shared_ptr<HashCompareData> hashCompareData, SV_LONGLONG offset, SV_ULONGLONG bytesRead, CxTransport::ptr cxTransport, Profiler &pHcdFileUpload);
    void SyncDone();
    /*Modified by BSR for FastSync TBC*/
    void ApplySyncData(std::string const & syncDataName, SV_LONGLONG & totalBytesApplied, std::string const & remoteName, cdp_volume_t::Ptr volume, CDPV2Writer::Ptr cdpv2writer, CxTransport::ptr &cxTransport, char* source = 0, SV_ULONG sourceLength = 0);
    /* End of the change*/
    virtual void ApplyChanges(const std::string &fileToApply) const { /* Dummy impl - overloaded in FastSyncRcm */ };

    int GenerateHashCompareData(SV_LONGLONG* offset, int processLimit, cdp_volume_t::Ptr volume, CxTransport::ptr cxTransport, Profiler &pHcdFileUpload, Profiler &pDiskRead, Profiler &pChecksumCalc);
    int GenerateHashCompareDataFromAzure(SV_LONGLONG* offset, int processLimit, CxTransport::ptr cxTransport, Profiler &pHcdFileUpload, Profiler &pGetHcdAzureAgent);

    // new functions added for full resync aware feature
    bool SendHcdNoMoreFile(CxTransport::ptr cxTransport);

    SV_ULONG RequiredCompatibility(int operation = 0) { return RequiredCompatibilityNum(); }

    void worker(CxTransport::ptr &cxTransport, cdp_volume_t::Ptr volptr, CDPV2Writer::Ptr cdpv2writer, const std::string & fileName, SV_ULONG size, Profiler &pSyncDownload, Profiler &pSyncUncompress);

    void SetPendingSyncCount(const SV_ULONG & count) { m_pendingSyncFiles = count; }
    SV_ULONG PendingSyncCount()  { return m_pendingSyncFiles.value(); }

    ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> m_previousProgressBytes ;


    void setChunkSize( const SV_ULONG& chunkSize ) { m_MaxSyncChunkSize = chunkSize ; }
    SV_ULONG getChunkSize() const { return m_MaxSyncChunkSize ; }
    SV_UINT getReadBufferSize() const { return m_readBufferSize; }

    //Added by BSR for TBC
    void startTargetBasedChecksumProcess( void );
    bool startHcdTaskAndWaitInFsResync(SharedBitMapPtr checksumTransferMap);
    bool startHcdTaskAndWait(SharedBitMapPtr checksumTransferMap);
    bool ShouldDoubleBitmapInFsResync(const std::string &filename) const;
    bool ShouldDoubleBitmap(const std::string &filename) const;

    bool tryMemoryApply(CxTransport::ptr &cxTransport,
                        cdp_volume_t::Ptr volptr,
                        CDPV2Writer::Ptr cdpv2writer,
                        string &fullFileName,
                        string &localFile,
                        SV_ULONG& fileSize,
                        Profiler &pSyncDownload,
                        Profiler &pSyncUncompress);

    void nextPresyncAndSyncFileNames( std::string& preTarget,
                                      std::string& target,
                                      COMPRESS_MODE compressMode);

    bool sendSVDHeader(CxTransport::ptr cxTransport,
                       const string& preTarget,
                       COMPRESS_MODE compressMode,
                       boost::shared_ptr<Compress> compressor );

    bool sendSyncData(CxTransport::ptr cxTransport,
                      const string& preTarget,
                      const SV_UINT dataLength,
                      const char* buffer,
                      const SV_LONGLONG offset,
                      COMPRESS_MODE compressMode,
                      const bool bMoreData,
                      boost::shared_ptr<Compress> compressor,
                      std::ostringstream& msg);

    void formatHcdNoMoreFileName( std::string& hcdNoMore );
    void formatHcdMissingFileName( std::string &hcdMissing );
    void nextHcdFileNames( std::string& preTarget,
                           std::string& target );
    SV_LONGLONG getVolumeSize(void) const 
	{
            return m_VolumeSize;
	}
    //End of the change

protected:
    const SV_ULONG m_LogResyncProgressInterval;
    SV_UINT m_ResyncStaleFilesCleanupInterval;
    bool m_ShouldCleanupCorruptSyncFile;
    const unsigned long m_HashCompareDataSize;
    const SV_ULONG m_ResyncUpdateInterval;
    const SV_ULONG m_ResyncSlowProgressCheckInterval;
    const SV_ULONG m_ResyncNoProgressCheckInterval;
    const bool m_IsMobilityAgent;
    boost::shared_ptr<ResyncProgressInfo> m_ResyncProgressInfo;
    SharedBitMapPtr m_checksumProcessedMap, m_checksumTransferMap, m_syncDataAppliedMap;

private:
    // no copying FastSync
    FastSync(const FastSync& fastSync);
    FastSync& operator=(const FastSync& fastSync);

    unsigned long m_MaxSyncChunkSize;

    SV_LONGLONG m_VolumeSize;

    cdp_volume_t::Ptr m_ProtectedData;

    ACE_HANDLE m_deviceHndl;

    ACE_Thread_Manager m_ThreadManager;

    ACE_Message_Queue<ACE_MT_SYNCH> m_MsgQueue;

    ACE_Atomic_Op<ACE_Thread_Mutex, unsigned long> m_pendingSyncFiles;

    const AGENT_RESTART_STATUS m_agentRestartStatus;

    mutable ACE_Mutex m_FileNameLock;
    typedef ACE_Guard<ACE_Mutex> AutoGuard;

    typedef bool (FastSync::*StartAndWaitForHcdTask_t)(SharedBitMapPtr checksumTransferMap);
    typedef bool (FastSync::*ShouldDoubleBitmap_t)(const std::string &filename) const;
    typedef void (FastSync::*DownloadAndProcessMissingHcd_t)(CxTransport::ptr cxTransport);
    typedef bool (FastSync::*AreHashesEqual_t)(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash,int phashSize);

    /*
     * Added for Fixing overshoot problem with Bitmaps
     * By Suresh
     */
    std::string m_bitMapDir;
    SharedBitMapPtr m_missingHcdsMap;

    boost::shared_ptr<GenerateHcdTask> m_genHCDTask;
    boost::shared_ptr<ProcessHcdTask> m_procHCDTask;
    boost::shared_ptr<ProcessClusterStage> m_procClusterStage;
    boost::shared_ptr<GenerateClusterInfoTask> m_generateClusterInfoTask;
    SV_UINT m_maxCSComputationThreads;
    bool m_bDICheck;
    bool m_bIsProcessClusterPipeEnabled;
    SV_UINT m_maxHcdsAllowdAtCx;
    SV_UINT m_secsToWaitForHcdSend;
    SV_UINT m_readBufferSize;
    ClusterParamsPtr m_clusterParamsPtr;
    StartAndWaitForHcdTask_t m_startHcdTaskAndWait[NBOOLS];
    ShouldDoubleBitmap_t m_shouldDoubleBitmap[NBOOLS];
    DownloadAndProcessMissingHcd_t m_downloadAndProcessMissingHcd[NBOOLS];
    bool m_compareHcd;
    AreHashesEqual_t m_areHashesEqual[NBOOLS];
    ResyncStats m_ResyncStats;
    mutable boost::mutex m_ResyncStatsLock;

public:
    void ProcessMissingHcdsPacket(CxTransport::ptr cxTransport);
    void ProcessMissingHcdsPacketInFsResync(CxTransport::ptr cxTransport);
    virtual bool sendResyncEndNotifyAndResyncTransition() ;
    bool CreateBitMapDir();
    bool initSharedBitMapFile(SharedBitMapPtr&, const string & fileName, CxTransport::ptr cxTransport, const bool & bShouldCreateIfNotPresent);
    bool isHcdTransferred(SV_ULONGLONG offset);
    void markHcdForTransfer(SV_ULONGLONG offset,bool);
    void markChunkAsUnMatched(SV_ULONGLONG);
    void markSyncFileForProcessing(SV_ULONGLONG, bool);
    void markChunkAsMatched(SV_ULONGLONG);
    bool updateAndPersistApplyInformation(CxTransport::ptr cxTransport) ;
    SVERROR CleanupBitMapInfo(CxTransport::ptr cxTransport);
    void formathcdTransferMap(string& fileName);
    void formathcdProcessMap(string& fileName);
    void formatsyncAppliedMap(string& fileName);
    void formatmissingHcdMap(string& fileName);
    void formatmissingSyncsMap(string& fileName);
    SV_ULONG totalChunks() const;
    string getHcdProcessMapFileName() const;
    string getHcdTransferMapFileName() const;
    string getSyncApplyMapFileName() const;
    string getMissingHcdsMapFileName();
    string getMissingSyncsMapFileName();

    void SendMissingSyncsPacket( CxTransport::ptr cxTransport ) ;
    void SendProcessedClusterMap( SharedBitMapPtr unprocessedClusterMap, CxTransport::ptr cxTransport );
    virtual bool ResyncStartNotifyIfRequired() ;

    bool createVolume(cdp_volume_t::Ptr &volume, std::string &err);
    cdp_volume_t::Ptr createVolume();
    void downloadAndProcessMissingHcdsPacket(CxTransport::ptr cxTransport);
    void downloadAndProcessMissingHcd( CxTransport::ptr cxTransport ) ;
    void downloadAndProcessMissingHcdInFsResync( CxTransport::ptr cxTransport ) ;
    void assignFileListToThreads( ACE_Shared_MQ& sharedListQueue, FileInfosPtr fileInfos, int taskCount, int maxEntries, int basePoint = 0 ) ;
    ACE_Message_Block * retrieveMessage( ACE_Shared_MQ& sharedListQueue ) ;

    SV_UINT GetMaxCSComputationThreads(void);
    void SetMaxCSComputationThreads(SV_UINT maxCSComputationThreads);
    bool IsDICheckEnabled(void);
    SV_UINT GetSecsToWaitForHcdSend(void);
    SV_UINT GetMaxHcdsAllowdAtCx(void);
    void UpdateHCDMatchState(VolumeDataAndHCD_t *pvolDataAndHcd);
	bool AreHashesEqual(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash, int phashSize);
	bool IsHashEqual(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash, int phashSize);
	bool ReturnHashesAsUnequal(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash, int phashSize);
    bool AreBitMapsValidInFsResync(
        SharedBitMap    *checksumTransferMap, 
        SharedBitMapPtr &missingHcdsMap, 
        SharedBitMapPtr &syncDataAppliedMap,
        SharedBitMapPtr &unprocessedClusterMap,
        std::ostringstream &msg
                                   );
    bool AreBitMapsValid(
        SharedBitMap    *checksumTransferMap, 
        SharedBitMapPtr &missingHcdsMap, 
        SharedBitMapPtr &syncDataAppliedMap,
        std::ostringstream &msg
                         );
    bool IsClusterTransferMapValid(SharedBitMapPtr &unprocessedClusterMap, 
                                   std::ostringstream &msg);
    bool PerformCleanUpIfRequired(void);
    bool AreClusterParamsValid(const unsigned long &clustersyncchunksize, 
                               const SV_UINT &clustersize, 
                               std::ostream &msg);
    bool IsReadBufferLengthValid(std::ostream &msg);
    bool IsProcessClusterPipeEnabled(void);
    unsigned long GetHashCompareDataSize(void);
    SV_UINT GetClusterSizeFromVolume(cdp_volume_t::Ptr& volume);
    ClusterParams *GetClusterParams(void);
    std::string getClusterBitMapFileName(void);
    std::string getUnProcessedClusterBitMapFileName(void);
    std::string getProcessedClusterBitMapFileName(void);
    std::string getUnProcessedPreClusterBitMapFileName(void);
    std::string getProcessedPreClusterBitMapFileName(void);
    bool sendHCDNoMoreIfRequired(CxTransport::ptr cxTransport);

    const CDP_SETTINGS & CdpSettings() { return m_cdpsettings;}
    cdp_resync_txn * CdpResyncTxnPtr() { return m_cdpresynctxn_ptr.get(); }
    void CheckResyncHealth();
    bool ResetHealthIssues(const std::vector<std::string> &vResetHICodes);
	AzureAgentInterface::AzureAgent::ptr AzureAgentPtr() { return m_azureagent_ptr; }
    void PerformResyncMonitoringTasks();
    bool IsTransportProtocolAzureBlob() const;
    void updateProfilingSummary(const bool resyncCompleted = false) const;

protected:
    virtual bool IsStartTimeStampSetBySource() const;
    virtual void UpdateResyncStats(const bool forceUpdate = false);
    virtual const ResyncStats GetResyncStats() const;
    virtual const std::string GetResyncStatsForCSLegacyAsJsonString();

protected:
        CDP_SETTINGS m_cdpsettings;
        AzureAgentInterface::AzureAgent::ptr m_azureagent_ptr;

private:
    cdp_resync_txn::ptr m_cdpresynctxn_ptr;
};

class IRSummary
{
public:
    std::string Job;
    std::string Dev;
    std::string DevSize_B;
    std::string IsSrc;
    std::string Status;
    std::vector<PrBuckets> IRSmry;

public:
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "IRSummary", false);
        JSON_E(adapter, Job);
        JSON_E(adapter, Dev);
        JSON_E(adapter, DevSize_B);
        JSON_E(adapter, IsSrc);
        JSON_E(adapter, Status);
        JSON_T(adapter, IRSmry);
    }
};

#endif // ifndef FASTSYNC__H
