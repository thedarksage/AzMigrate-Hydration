#include "ReplicationManager.h"
#include "ReplicationWorker.h"

CReplicationWorker::CReplicationWorker(PGET_DB_TRANS_DATA pGetDbTransData,
    IPlatformAPIs* platformApis, ILogger* logger, IDataProcessor* pDataProcessor,
    CInmageIoctlController* ioctlController) :
m_pGetDbTransData(pGetDbTransData),
m_platformAPIs(platformApis),
m_pLogger(logger),
m_dataProcessor(pDataProcessor),
m_ioctlController(ioctlController),
m_continue(true),
m_notifyEvent(false, true, std::string())
{
    m_DirtyBlock.reset(new UDIRTY_BLOCK_V2);
    m_pSourceDevice.reset(new CDiskDevice(
        pGetDbTransData->m_sourceDevice->GetDeviceNumber(), platformApis));
    m_pLogger->LogInfo("Entering: %s\n", __FUNCTION__);
}

CReplicationWorker::~CReplicationWorker()
{
    // Terminate Replication Thread
    m_stopMutex.lock();
    m_continue = false;
    m_stopMutex.unlock();

    m_hReplicationThread.join();
    m_pLogger->LogInfo("Entering: %s\n", __FUNCTION__);
    SafeFree(m_pGetDbTransData);
}

void CReplicationWorker::ClearDifferentials()
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

    m_ioctlController->ClearDifferentials(m_pSourceDevice->GetDeviceId());

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::ResyncStartNotification()
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

    m_ioctlController->ResyncStartNotification(m_pSourceDevice->GetDeviceId());

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::ResyncEndNotification()
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

    m_ioctlController->ResyncEndNotification(m_pSourceDevice->GetDeviceId());

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::WaitForNextDirtyBlock()
{
    if (WAIT_SUCCESS != m_notifyEvent.Wait(10, 0))
    {
        m_pLogger->LogError("Wait timed out\n");
    }
    else
    {
        m_pLogger->LogInfo("Wait Succeeded");
    }
}

bool CReplicationWorker::ShouldWaitForNextDirtyBlock()
{
#ifdef SV_WINDOWS
    return (m_DirtyBlock.get()->uHdr.Hdr.ulicbTotalChangesPending.QuadPart < (8 * 1024 * 1024));
#else
    int ret = m_ioctlController->WaitDB(m_pSourceDevice->GetDeviceId());
    return false;
#endif
}

void CReplicationWorker::StartReplication()
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

    try
    {
        m_hReplicationThread = boost::thread(&CReplicationWorker::Run, this);
    }
    catch (exception& ex)
    {
        m_pLogger->LogError("%s Err: %s", __FUNCTION__, ex.what());
    }

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::StartFiltering()
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

#ifdef SV_WINDOWS
    m_ioctlController->StartFiltering(m_pSourceDevice->GetDeviceId());
#else
    SV_ULONG blockSize = m_pSourceDevice->GetCopyBlockSize();
    unsigned long long numBlocks = m_pSourceDevice->GetDeviceSize() / blockSize;
    m_ioctlController->StartFiltering(m_pSourceDevice->GetDeviceId(),
        numBlocks, m_pSourceDevice->GetCopyBlockSize());
#endif

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::StopFiltering(bool delBitmapFile = true)
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

    m_ioctlController->StopFiltering(m_pSourceDevice->GetDeviceId(), delBitmapFile, false);

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::Run()
{
    m_pLogger->LogInfo("%s: device: %d", __FUNCTION__, m_pSourceDevice->GetDeviceNumber());

    try
    {
        StartFiltering();
        GetDBTransV2(m_pGetDbTransData);
    }
    catch (exception& ex)
    {
        m_pLogger->LogError("%s Err: %s", __FUNCTION__, ex.what());
#ifdef SV_WINDOWS
        exit(-1);
#endif
    }

    m_pLogger->LogInfo("%s: Exited", __FUNCTION__);
}

void CReplicationWorker::ProcessDirtyBlock(PGET_DB_TRANS_DATA pDBTranData,
    PUDIRTY_BLOCK_V2 pDirtyBlock)
{
    if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM)
    {
        size_t  streamSize = pDirtyBlock->uHdr.Hdr.ulcbChangesInStream;
        CSegStream  segStream(pDirtyBlock->uHdr.Hdr.ppBufferArray,
            pDirtyBlock->uHdr.Hdr.usNumberOfBuffers, streamSize,
            pDirtyBlock->uHdr.Hdr.ulBufferSize);
        ValidateAndSyncDirtyBlock(pDBTranData, pDirtyBlock, segStream);
    }
    else
    {
        m_pLogger->LogInfo("%s : UDIRTY_BLOCK_FLAG_SVD_STREAM flag is not set", __FUNCTION__);
        if (0 == pDirtyBlock->uHdr.Hdr.cChanges)
        {
            ProcessTags(pDirtyBlock);
            return;
        }

        if (NULL == m_dataProcessor)
        {
            throw CAgentException("NULL Data Processor");
        }

        for (unsigned long i = 0; i < pDirtyBlock->uHdr.Hdr.cChanges; i++)
        {
            m_dataProcessor->ProcessChanges(NULL, pDirtyBlock->ChangeOffsetArray[i],
                pDirtyBlock->ChangeLengthArray[i]);
        }
    }
}

void CReplicationWorker::ProcessTags(PUDIRTY_BLOCK_V2 udbp)
{
    m_pLogger->LogInfo("ENTERED %s", __FUNCTION__);
    PSTREAM_REC_HDR_4B pTag = &udbp->uTagList.TagList.TagEndOfList;
    list<string>        taglist;

    SV_ULONG   ulBytesProcessed = 0;
    SV_ULONG   ulNumberOfUserDefinedTags = 0;

    m_pLogger->LogInfo("%s", __FUNCTION__);
    while (pTag->usStreamRecType != STREAM_REC_TYPE_END_OF_TAG_LIST)
    {
        SV_ULONG ulLength = STREAM_REC_SIZE(pTag);
        SV_ULONG ulHeaderLength = (pTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?
            sizeof(STREAM_REC_HDR_8B) : sizeof(STREAM_REC_HDR_4B);

        if (pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG)
        {
            SV_ULONG    ulTagLength = 0;
            char*       pTagData;

            ulNumberOfUserDefinedTags++;

            ulTagLength = *(SV_PUSHORT)((SV_PUCHAR)pTag + ulHeaderLength);

            // This is tag name
            pTagData = (char*)pTag + ulHeaderLength + sizeof(SV_USHORT);
#ifdef SV_WINDOWS
            taglist.push_back(pTagData);
#else
            PSTREAM_REC_HDR_4B tagGuidHdrP = (PSTREAM_REC_HDR_4B)pTagData;
            char tagGuid[256] = { 0 } ;
            memcpy(tagGuid, (char*)tagGuidHdrP + ulHeaderLength,
                tagGuidHdrP->ucLength - ulHeaderLength);
            std::string tagGuidStr = tagGuid;

            m_pLogger->LogInfo("%s: Tag Guid Received: %s", __FUNCTION__, tagGuidStr.c_str());
            taglist.push_back(tagGuidStr);
#endif
        }

        ulBytesProcessed += ulLength;
        pTag = (PSTREAM_REC_HDR_4B)((SV_PUCHAR)pTag + ulBytesProcessed);
    }

    m_dataProcessor->ProcessTags(taglist);

    m_pLogger->LogInfo("EXITED %s", __FUNCTION__);
}

void CReplicationWorker::GetDBTransV2(PGET_DB_TRANS_DATA  pDBTranData)
{
    m_pLogger->LogInfo("ENTERED %s", __FUNCTION__);

    PUDIRTY_BLOCK_V2    pDirtyBlock;
    COMMIT_TRANSACTION  CommitTrans = { 0 };
    SV_ULONG            ulNumDirtyBlocksReturned;
    TSSNSEQIDV2         tempPrevTSSequenceNumberV2 = { 0, 0, 0 };
    int                 iTsoCount = 0;
    int                 iStartTsoCount = 0;

    ulNumDirtyBlocksReturned = 0;

#ifdef SV_WINDOWS
    m_ioctlController->RegisterForSetDirtyBlockNotify(
        m_pSourceDevice->GetDeviceId(), m_notifyEvent.Self());
    SV_ULONG   dwStartTime = GetTickCount();
#endif

    bool bGetNextDirtyBlock = false;
    bool bContinue;

    do
    {
        pDirtyBlock = m_DirtyBlock.get();

        memset(&(pDBTranData->FCSequenceNumberV2), 0, sizeof(TSSNSEQIDV2));
        memset(&(pDBTranData->LCSequenceNumberV2), 0, sizeof(TSSNSEQIDV2));

        CommitTrans.ulFlags = 0;
#ifdef SV_WINDOWS
        CommitTrans.ulTransactionID.QuadPart = 0;
#else
        CommitTrans.ulTransactionID = 0;
#endif
        try
        {
#ifdef SV_UNIX
            m_dataProcessor->WaitForAllowDraining();
#endif
            m_ioctlController->GetDirtyBlockTransaction(&CommitTrans,
                pDirtyBlock, true, m_pSourceDevice->GetDeviceId());
            ulNumDirtyBlocksReturned++;

            m_pLogger->LogInfo("%s: TransId: 0x%I64x Flags: 0x%x",  __FUNCTION__,
                pDirtyBlock->uHdr.Hdr.uliTransactionID, pDirtyBlock->uHdr.Hdr.ulFlags);

            if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE)
            {
                iTsoCount++;
                pDBTranData->FCSequenceNumberV2.ullSequenceNumber =
                    pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber;
                pDBTranData->FCSequenceNumberV2.ullTimeStamp =
                    pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                pDBTranData->LCSequenceNumberV2.ullSequenceNumber =
                    pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber;
                pDBTranData->LCSequenceNumberV2.ullTimeStamp =
                    pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;

                m_pLogger->LogInfo("%s: ProcessTSO", __FUNCTION__);
                m_dataProcessor->ProcessTSO();
            }
            else
            {
                iTsoCount = 0;
                iStartTsoCount = 1;
                ProcessDirtyBlock(pDBTranData, pDirtyBlock);
            }

            ValidateTimeStampSeqV2(pDBTranData, pDirtyBlock, &tempPrevTSSequenceNumberV2);

            CommitTrans.ulFlags = 0;
#ifdef SV_WINDOWS
            CommitTrans.ulTransactionID.QuadPart = pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart;
#else
            CommitTrans.ulTransactionID = pDirtyBlock->uHdr.Hdr.uliTransactionID;
#endif

            if ((pDBTranData->bResetResync) &&
                (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED))
            {
                CommitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
            }
#ifdef SV_WINDOWS
            if (0 != CommitTrans.ulTransactionID.QuadPart)
#else
            if (0 != CommitTrans.ulTransactionID)
#endif
            {
                m_ioctlController->CommitTransaction(&CommitTrans);
            }
        }
        catch (exception& ex)
        {
            m_pLogger->LogError("%s Error: %s\n", __FUNCTION__, ex.what());
#ifdef SV_UNIX
            boost::this_thread::sleep(boost::posix_time::seconds(10));
#endif
        }

#ifdef SV_WINDOWS
        if (ShouldWaitForNextDirtyBlock())
#else
        if (m_continue && ShouldWaitForNextDirtyBlock())
#endif
        {
            WaitForNextDirtyBlock();
        }

        m_stopMutex.lock();
        bContinue = m_continue;
        m_stopMutex.unlock();
        if (!bContinue)
        {
            m_pLogger->LogInfo("Exiting: %s", __FUNCTION__);
        }
    } while (bContinue);

    m_pLogger->LogInfo("EXITED %s", __FUNCTION__);
}
