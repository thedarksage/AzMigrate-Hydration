#ifndef _REPLICATION_WORKER_H_
#define _REPLICATION_WORKER_H_

#include "IPlatformAPIs.h"


#ifdef SV_WINDOWS
#include "InmFltIOCTL.h"
#include "InmFltInterface.h"
#else
#include "involflt.h"
#endif
#include "svdparse.h"
#include "svtypes.h"
#include "IBlockDevice.h"
#include "SegStream.h"
#include "InmageTestException.h"

#include "SVDStream.h"
#include "DataProcessor.h"

#include "boost/thread.hpp"

#include <map>
#include <list>
#include <vector>
#include <map>



typedef struct _TSSNSEQID
{
    SV_ULONG     ulContinuationId;
    SV_ULONG     ulSequenceNumber;
    SV_ULONGLONG ullTimeStamp;
}TSSNSEQID, *PTSSNSEQID;

typedef struct _TSSNSEQIDV2
{
    SV_ULONG     ulContinuationId;
    SV_ULONGLONG ullSequenceNumber;
    SV_ULONGLONG ullTimeStamp;
}TSSNSEQIDV2, *PTSSNSEQIDV2;

typedef struct _GET_DB_TRANS_DATA {
    bool    CommitPreviousTrans;
    bool    CommitCurrentTrans;
    bool    bPollForDirtyBlocks;
    bool    bWaitIfTSO;
    bool    bResetResync;
    SV_ULONG   ulWaitTimeBeforeCurrentTransCommit;
    SV_ULONG   ulRunTimeInMilliSeconds;
    SV_ULONG   ulPollIntervalInMilliSeconds;
    SV_ULONG   ulNumDirtyBlocks;

    // Required for Sync
    bool    sync;
    bool    resync_req;
    bool    skip_step_one;

    SV_ULONG            BufferSize;
    IBlockDevice*   m_sourceDevice;

    TSSNSEQID   FCSequenceNumber;
    TSSNSEQID   LCSequenceNumber;
    TSSNSEQIDV2 FCSequenceNumberV2;
    TSSNSEQIDV2 LCSequenceNumberV2;

    _GET_DB_TRANS_DATA()
    {
        memset(this, 0, sizeof(*this));
    }
} GET_DB_TRANS_DATA, *PGET_DB_TRANS_DATA;

class CReplicationWorker
{
private:
    boost::thread                       m_hReplicationThread;
    PGET_DB_TRANS_DATA                  m_pGetDbTransData;
    IPlatformAPIs*                      m_platformAPIs;
    boost::shared_ptr<IBlockDevice>     m_pSourceDevice;
    SV_ULONG                            m_dwCopySize;
    ILogger*                            m_pLogger;
    IDataProcessor*                     m_dataProcessor;
    CInmageIoctlController*             m_ioctlController;
    bool                                m_delBitmapFlag;
    Event                               m_notifyEvent;
    boost::mutex                        m_stopMutex;
    bool                                m_continue;
    boost::shared_ptr<UDIRTY_BLOCK_V2>  m_DirtyBlock;

    void GetDBTransV2(PGET_DB_TRANS_DATA  pDBTranData);
    void CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction);

    void ValidateAndSyncDirtyBlock(PGET_DB_TRANS_DATA pDBTranData,
                                   PUDIRTY_BLOCK_V2  DirtyBlock, 
                                   CDataStream &DataStream);

    void ValidateTimeStampSeqV2(PGET_DB_TRANS_DATA pDBTranData,
                                PUDIRTY_BLOCK_V2 pDirtyBlock,
                                PTSSNSEQIDV2 ptempPrevTSSequenceNumberV2);

    bool GetDirtyBlocksTrans(PCOMMIT_TRANSACTION  pCommitTrans,
                             PUDIRTY_BLOCK_V2  pDirtyBlock);

    void GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans,
                                  PUDIRTY_BLOCK_V2&  pDirtyBlock,
                                  bool bThowIfResync);

    void ProcessDirtyBlock(PGET_DB_TRANS_DATA  pDBTranData,
                           PUDIRTY_BLOCK_V2  pDirtyBlock);

    void ProcessTags(PUDIRTY_BLOCK_V2 udbp);
    void ProcessChanges();

public:
    void Run();
    CReplicationWorker(PGET_DB_TRANS_DATA       pGetDbTransData, 
                       IPlatformAPIs*           platformApis,
                       ILogger*                 logger,
                       IDataProcessor           *dataProcessor,
                       CInmageIoctlController*  ioctlController);

    ~CReplicationWorker();

    void ClearDifferentials();
    void StartFiltering();
    void StartReplication();
    void ResyncStartNotification();
    void ResyncEndNotification();
    void StopFiltering(bool delBitmapFile);
    void WaitForNextDirtyBlock();
    bool ShouldWaitForNextDirtyBlock();
};

#endif
