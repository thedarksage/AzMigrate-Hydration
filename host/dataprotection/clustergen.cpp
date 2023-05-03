#include "fastsync.h"
#include "theconfigurator.h"
#include "configurevxagent.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmalertdefs.h"
#include "configwrapper.h"
#include "dataprotectionutils.h"

FastSync::GenerateClusterInfoWorker::GenerateClusterInfoWorker( FastSync* fastSync,
                                                                ACE_Thread_Manager* threadManager,
                                                                ACE_Shared_MQ chunkQueue,
                                                                SynchronousCount& pendingChunkCount, 
                                                                GenerateClusterInfoTask *pClusterInfoTask
                                                                ) :
    m_FastSync(fastSync),
    ACE_Task<ACE_MT_SYNCH>( threadManager ),
    m_chunkQueue( chunkQueue ),
    m_pendingChunkCount( pendingChunkCount ) ,
    m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
    m_volume( m_FastSync->createVolume() ), 
    m_pClusterInfoTask(pClusterInfoTask),
    m_clusterSize(0),
    m_pClusterUpload(NEWPROFILER(CLUSTERUPLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true))
{
}

int FastSync::GenerateClusterInfoWorker::open( void *args )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return this->activate(THR_BOUND);
}

int FastSync::GenerateClusterInfoWorker::close( SV_ULONG flags )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    return 0 ;
}

int FastSync::GenerateClusterInfoWorker::svc()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    try
    {
        setClusterSize();

        while( !m_FastSync->QuitRequested( TaskWaitTimeInSeconds ) )
        {
            ACE_Message_Block * mb = m_FastSync->retrieveMessage( m_chunkQueue ) ;
            if( mb != NULL )
            {
                FastSyncMsg_t * msg = (FastSyncMsg_t*) mb->base();
                GenerateClusterBitmap( msg );
                delete msg;
                mb -> release();
            }
            else
            {
                m_cxTransport->heartbeat();
            }
        }
        m_pClusterUpload->commit(); // Commit if anything is pending
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync ->Stop();
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync ->Stop();
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "FastSync::GenerateClusterInfoWorker::svc caught an unnknown exception\n");
        m_FastSync ->Stop();
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return 0;
}


void FastSync::GenerateClusterInfoWorker::GenerateClusterBitmap( FastSyncMsg_t* msg )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    size_t chunkIndex = msg->index ;
    while(!m_FastSync->QuitRequested(0) && chunkIndex < msg->maxIndex )
    {
        SV_LONGLONG offset = chunkIndex * (SV_ULONGLONG) m_pClusterInfoTask->getClusterSyncChunkSize();
        DebugPrintf( SV_LOG_DEBUG, "ChunkIndex: %d, Offset to be generated: " LLSPEC "\n", chunkIndex, offset );
        GenerateClusterBitmap( &offset ) ;
        --m_pendingChunkCount ;
        chunkIndex += msg->skip ;
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return ;
}


void FastSync::GenerateClusterInfoWorker::GenerateClusterBitmap(const SV_LONGLONG* offset)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::ostringstream msg;

    if (m_pClusterInfoTask->isClusterBitTransferred(*offset))
    {
        msg << "The cluster file sent for this chunk " << ((*offset)/m_pClusterInfoTask->getClusterSyncChunkSize())
            << " already";
        DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
        return;
    }

    m_FastSync->ThrottleSpace();
    unsigned long bytestocover;
    if( (m_FastSync->getVolumeSize() - *offset) <= m_pClusterInfoTask->getClusterSyncChunkSize() )
    {
        INM_SAFE_ARITHMETIC(bytestocover = (InmSafeInt<SV_LONGLONG>::Type(m_FastSync->getVolumeSize()) - *offset), INMAGE_EX(m_FastSync->getVolumeSize())(*offset))
            }
    else
    {
        bytestocover = m_pClusterInfoTask->getClusterSyncChunkSize();
    }

    VolumeClusterInformer::VolumeClusterInfo vci;
    INM_SAFE_ARITHMETIC(vci.m_CountRequested = InmSafeInt<unsigned long>::Type(bytestocover) / getClusterSize(), INMAGE_EX(bytestocover)(getClusterSize()))
        /* TODO: this code breaks if physicaloffset is not landing exactly on
         * cluster boundary ? */
        SV_LONGLONG physicaloffset = *offset + m_FastSync->GetSrcStartOffset();
    vci.m_Start = physicaloffset / getClusterSize();
    DebugPrintf(SV_LOG_DEBUG, "asking clusters from start = " ULLSPEC ", count requested = %u, offset = " 
                LLSPEC ", bytestocover = %lu\n", vci.m_Start, vci.m_CountRequested,
                *offset, bytestocover);
    bool bgotclusterinfo = m_volume->GetVolumeClusterInfo(vci); 
    if (bgotclusterinfo)
    {
        /* customize the bitmap here */
        m_cdpVolumeUtil.CustomizeClusterBitmap(*m_FastSync->GetClusterBitmapCustomizationInfos(), vci);
        m_volume->PrintVolumeClusterInfo(vci);
        boost::shared_array<char> hdrbuffer ;
        SV_UINT Size = 0;
        SV_UINT nbytesforbitmap = (vci.m_CountFilled / NBITSINBYTE) + ((vci.m_CountFilled % NBITSINBYTE) ? 1 : 0);
        SV_ULONGLONG clusterstartinfile = *offset / getClusterSize();
        FsClusterInfo hdr(getClusterSize(), clusterstartinfile, vci.m_CountFilled, vci.m_pBitmap);

        hdr.FillClusterInfo(hdrbuffer, Size);
        std::string preName, completedName ;
        m_pClusterInfoTask->nextClusterFileNames( preName, completedName ) ; 

        if (m_FastSync->IsTransportProtocolAzureBlob())
        {
            preName = completedName;
        }

        if (!m_FastSync->SendData(m_cxTransport, preName, hdrbuffer.get(), Size, CX_TRANSPORT_MORE_DATA, COMPRESS_NONE))
        {
            msg << "SendData Returned Fail for " << preName << " with status " << m_cxTransport->status() << '\n';
            throw DataProtectionException(msg.str());
        }

        m_pClusterUpload->start();
        if (!m_FastSync->SendData(m_cxTransport, preName, (char *)vci.m_pBitmap, nbytesforbitmap, CX_TRANSPORT_NO_MORE_DATA, COMPRESS_NONE))
        {
            msg << "SendData Returned Fail for " << preName << " with status " << m_cxTransport->status() << '\n';
            throw DataProtectionException(msg.str());
        }
        m_pClusterUpload->stop(nbytesforbitmap);

        if( !m_cxTransport->renameFile( preName, completedName, COMPRESS_NONE ) )
        {
            msg << "Failed to rename the file " << preName << " to " << completedName << '\n';
            throw DataProtectionException(msg.str());
        }
        m_pClusterInfoTask->clusterTransferMap()->markAsTransferred( *offset, bytestocover );
    }
    else
    {
        VolumeFSClusterQueryFailureAlert a(m_FastSync->GetDeviceName(), vci.m_Start, vci.m_CountRequested);
        throw DataProtectionException(msg.str()+"\n");
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void FastSync::GenerateClusterInfoWorker::setClusterSize(void)
{
    m_clusterSize = m_FastSync->GetClusterSizeFromVolume(m_volume);
    std::stringstream msg;
    if (!m_FastSync->AreClusterParamsValid(m_pClusterInfoTask->getClusterSyncChunkSize(), m_clusterSize, msg))
    {
        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
    }
}


// GenerateClusterInfoTask
FastSync::GenerateClusterInfoTask::GenerateClusterInfoTask(FastSync* fastSync, ACE_Thread_Manager* threadManager)
    : ACE_Task<ACE_MT_SYNCH>(threadManager),
      m_FastSync(fastSync),
      m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId))
{
    m_maxClusterBitmapsAllowdAtCx = m_FastSync->GetConfigurator().getMaxClusterBitmapsAllowdAtCx();
    m_secsToWaitForClusterBitmapSend = m_FastSync->GetConfigurator().getSecsToWaitForClusterBitmapSend();
}


SV_UINT FastSync::GenerateClusterInfoTask::GetMaxClusterBitmapsAllowdAtCx(void) const
{
    return m_maxClusterBitmapsAllowdAtCx;
}


SV_UINT FastSync::GenerateClusterInfoTask::GetSecsToWaitForClusterBitmapSend(void) const
{
    return m_secsToWaitForClusterBitmapSend;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::GenerateClusterInfoTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::GenerateClusterInfoTask::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::GenerateClusterInfoTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    try
    {
        std::stringstream msg;


		// If we are using earlier implementation of fastsync TBC, do not send the used/unused block 
		// information.

		// Specifically for azure target- we choose to go with earlier fastsync TBC  implementation
		// see design doc for TFS Feature 2166919 providing the reasons.

		if (!m_FastSync -> IsFsAwareResync())
		{
			m_FastSync->PostMsg(SYNC_DONE, SYNC_NORMAL_PRIORITY);
			return 0;
		}

        if (!m_FastSync->GetClusterParams()->AreValid(msg))
        {
            msg << ". For source volume " << m_FastSync->GetDeviceName() << '\n';
            throw DataProtectionException(msg.str());
        }
        if( !m_FastSync->GetClusterParams()->initSharedBitMapFile(m_clusterTransferMap, m_FastSync->getClusterBitMapFileName(), m_cxTransport, true) )
        {
            throw DataProtectionException("Cluster BitMap " + m_FastSync->getClusterBitMapFileName() + " initialization failed\n");
        }
        GenerateClusterBitmaps();
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }

    GenerateClusterInfoWorkerTasks_t::iterator beginIter( m_workerTasks.begin() ) ;
    GenerateClusterInfoWorkerTasks_t::iterator endIter( m_workerTasks.end() ) ;
    for( ; beginIter != endIter; beginIter++ )
    {
        GenerateClusterInfoWorkerPtr taskptr = (*beginIter);
        thr_mgr() -> wait_task(taskptr.get());
    }
    /* TODO: nothing happens even after sending exception,
     * we say done ? in case of exception ? */
    m_FastSync->PostMsg(SYNC_DONE, SYNC_NORMAL_PRIORITY);

    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
    return 0;
}


void FastSync::GenerateClusterInfoTask::GenerateClusterBitmaps()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    setPendingChunkCount( 0 ) ;
    ACE_Shared_MQ chunkQueue(new ACE_Message_Queue<ACE_MT_SYNCH>() );
    SV_UINT taskCount = m_FastSync->GetConfigurator().getMaxGenerateClusterBitmapThreads() ;
    m_prevFullSyncBytesSent = 0;
    SV_ULONG intervalTime = 0 ;
    SV_UINT TotalNoOfChunks = m_FastSync->GetClusterParams()->totalChunks(); 

    while( !m_FastSync->QuitRequested(WindowsWaitTimeInSeconds) )
    {
        m_cxTransport->heartbeat();

        intervalTime += WindowsWaitTimeInSeconds ;
        if( intervalTime >=  m_FastSync->m_ResyncUpdateInterval)
        {
            if( (m_clusterTransferMap->syncToPersistentStore(m_cxTransport) == SVS_OK) &&
                (m_prevFullSyncBytesSent.value() != m_clusterTransferMap->getBytesProcessed()) && 
                m_FastSync->SetFastSyncFullSyncBytes( m_clusterTransferMap->getBytesProcessed() ) )
            {
                DebugPrintf( SV_LOG_DEBUG, "Updating FullSync Bytes Information after %u seconds:"
                             "\nFullSync Sent Bytes:" ULLSPEC"\n",
                             intervalTime,
                             m_clusterTransferMap->getBytesProcessed()) ;
                m_prevFullSyncBytesSent = m_clusterTransferMap->getBytesProcessed() ;
            }
            intervalTime = 0 ;
        }

        /* TODO: In comparision with hcd generation code,
         *       it used to check for unprocessed file
         *       only if it had some thing to send from
         *       the transfer map */
        if (IsUnProcessedClusterMapPresent())
        {
            DebugPrintf(SV_LOG_DEBUG, "unprocessed cluster map is present for volume %s. Waiting for it to be processed\n",
                        m_FastSync->GetDeviceName().c_str());
            m_FastSync->Idle( TaskWaitTimeInSeconds ) ;
            continue;
        }

        LoadProcessedClusterMapIfPresent();

        if( canSendClusterBitmaps() && (pendingChunkCount() == 0) )
        {
            size_t index1 = m_clusterTransferMap->getFirstUnSetBit();
            if( index1 != boost::dynamic_bitset<>::npos )
            {
                DebugPrintf(SV_LOG_DEBUG, "There are some chunks for which the cluster bitmap needs to be sent\n");
                m_clusterTransferMap->dumpBitMapInfo();

                size_t index2 = m_clusterTransferMap->getNextSetBit(index1);
                SV_UINT chunksToBeProcessed = 0;
                if( index2 == boost::dynamic_bitset<>::npos ) //reached end of the bitmap
                {
                    chunksToBeProcessed = TotalNoOfChunks - index1;
                }
                else
                {
                    chunksToBeProcessed = index2 - index1;
                }

                SV_UINT numMaxClusterBitmaps = GetMaxClusterBitmapsAllowdAtCx();
                std::string remoteDir = m_FastSync->RemoteSyncDir() ;
                std::string prefixName = remoteDir + ClusterBitmapPrefix;
                Files_t files;
                if (m_FastSync->GetFileList(m_cxTransport, prefixName, files))
                {
                    size_t numPresent = files.size();

                    SV_LONGLONG llNNeedToSend = chunksToBeProcessed;
                    if ((numPresent + chunksToBeProcessed) > numMaxClusterBitmaps)
                    {
                        SV_LONGLONG llNExisting = numPresent;
                        SV_LONGLONG llLimit = numMaxClusterBitmaps;
                        llNNeedToSend = llLimit - llNExisting;
                    }

                    if (llNNeedToSend > 0)
                    {
                        chunksToBeProcessed = llNNeedToSend;
                        if( m_workerTasks.size() == 0 )
                        {
                            for( int index = 0; index < taskCount; index++ )
                            {
                                GenerateClusterInfoWorkerPtr genTask( new GenerateClusterInfoWorker( m_FastSync, thr_mgr(), chunkQueue, 
                                                                                                     m_pendingChunkCount, this ) ) ;
                                genTask->open() ;
                                m_workerTasks.push_back( genTask ) ;
                            }
                        }
                        setPendingChunkCount( chunksToBeProcessed ) ;
                        FileInfosPtr fileInfos;
                        m_FastSync->assignFileListToThreads( chunkQueue, fileInfos, taskCount, chunksToBeProcessed, index1 ) ;
                    }
                    else
                    {
                        std::stringstream msg;
                        msg << "number of bitmaps to send = " << chunksToBeProcessed << ','
                            << "number of bitmaps already present at CX = " << numPresent << ','
                            << "limit on number of bitmaps present = " << numMaxClusterBitmaps << '\n';
                        DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
                        SV_UINT nsecstowaitforclusterbitmapsend = GetSecsToWaitForClusterBitmapSend();
                        DebugPrintf(SV_LOG_DEBUG, "hence waiting for %u seconds\n", nsecstowaitforclusterbitmapsend);
                        m_FastSync->Idle(nsecstowaitforclusterbitmapsend);
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_WARNING, "Failed to get the file list for cluster bitmap file spec %s. Retry After some time.\n", prefixName.c_str());
                }
            }
        }
    }

    if( m_clusterTransferMap->syncToPersistentStore(m_cxTransport) != SVS_OK )
    {
        throw DataProtectionException("FAILED Failed to sync cluster transfer map " + m_FastSync->getClusterBitMapFileName() + " to CX\n");
    }

    if ( !m_FastSync->SetFastSyncFullSyncBytes( m_clusterTransferMap->getBytesProcessed() ) )
    {
        throw DataProtectionException("FAILED Failed to update full sync bytes for volume " + m_FastSync->GetDeviceName() + "\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return ;
}


/* TODO: check if we need to model as closely as checking the 
 *       missing packets etc, . for cluster bitmap ? for now
 *       NO; Also check if we need to send any no more 
 *       cluster bitmap file ? */
bool FastSync::GenerateClusterInfoTask::canSendClusterBitmaps()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME ) ;
    bool bRetValue = false ;
    size_t index1 = m_clusterTransferMap->getFirstUnSetBit();
    if( index1 != boost::dynamic_bitset<>::npos )
    {
        bRetValue = true;
    }
    else
    {
        sendClusterNoMoreIfRequired();
        sendUnProcessedClusterIfRequired();
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
    return bRetValue ;
}


void FastSync::GenerateClusterInfoTask::sendClusterNoMoreIfRequired()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME ) ;
    Files_t files;
    string clusterNoMoreFile = m_FastSync->RemoteSyncDir( ) + completedClusterPrefix + m_FastSync->JobId() + noMore ;
    std::stringstream excmsg;

    DebugPrintf(SV_LOG_DEBUG, "cluster no more file name is %s\n", clusterNoMoreFile.c_str());
    if( m_FastSync->GetFileList(m_cxTransport, clusterNoMoreFile, files) )
    {
        if( files.size() == 0 && !SendClusterNoMoreFile(m_cxTransport))
        {
            excmsg << "Failed to send the cluster No More file for volume " << m_FastSync->GetDeviceName() <<". It will be retried.\n";
            throw DataProtectionException(excmsg.str(), SV_LOG_WARNING);
        }
    }
    else
    {
        excmsg << "Failed to get the file list for file spec " << clusterNoMoreFile << ". It will be retried.\n";
        throw DataProtectionException(excmsg.str(), SV_LOG_WARNING);
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
    return ;
}


bool FastSync::GenerateClusterInfoTask::SendClusterNoMoreFile(CxTransport::ptr &cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    FsClusterInfo hdr;
    boost::shared_array<char> hdrbuffer ;
    SV_UINT Size = 0;

    hdr.FillClusterInfo(hdrbuffer, Size);
    std::string preName, newName, temp ;
    nextClusterFileNames( preName, temp ) ; //temp is not being used
    formatClusterNoMoreFileName( newName ) ;
    newName = m_FastSync->RemoteSyncDir() + newName ;
    if(m_FastSync->IsTransportProtocolAzureBlob())
    {
        preName = newName;
    }

    if (!m_FastSync->SendData(cxTransport, preName, hdrbuffer.get(), Size, CX_TRANSPORT_NO_MORE_DATA, COMPRESS_NONE))
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s SendData Returned Fail for %s with status %s\n"
                    , FUNCTION_NAME, LINE_NO, FILE_NAME, preName.c_str(), cxTransport->status());
        return false;
    }

    if( !cxTransport->renameFile( preName, newName, COMPRESS_NONE ) )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to rename the file %s to %s with error %s\n", preName.c_str(), newName.c_str(), cxTransport->status());
        return false;
    }
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Sent ClusterNoMoreFile[%s]\n", FUNCTION_NAME, m_FastSync->JobId().c_str(), newName.c_str());

    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
    return true;
}


void FastSync::GenerateClusterInfoTask::nextClusterFileNames( std::string& preTarget,
                                                              std::string& target )
{
    AutoGuard lock( m_FileNameLock ) ;
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    std::ostringstream name;
    // TODO: better unique name
    ACE_Time_Value aceTv = ACE_OS::gettimeofday();

    name << PreCluster ;
    name << m_FastSync->JobId() << "_" ;
    name << std::hex << std::setw(16) << std::setfill('0') << ACE_TIME_VALUE_AS_USEC(aceTv) << DatExtension;

    preTarget = m_FastSync->RemoteSyncDir() + name.str();
    target = m_FastSync->RemoteSyncDir() + name.str().substr(4);
    ACE_Time_Value wait ;
    wait.msec( FileNameWaitInMilli ) ;
    ACE_OS::sleep( wait ) ;
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return ;
}


void FastSync::GenerateClusterInfoTask::formatClusterNoMoreFileName( std::string& clusterNoMore )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    clusterNoMore = completedClusterPrefix + m_FastSync->JobId() + noMore ;
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return ;
}


bool FastSync::GenerateClusterInfoTask::isClusterBitTransferred(SV_ULONGLONG offset)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    clusterTransferMap()->dumpBitMapInfo();
    bool bReturnValue = clusterTransferMap()->isBitSet(offset/m_FastSync->GetClusterParams()->getClusterSyncChunkSize());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bReturnValue ;
}


bool FastSync::GenerateClusterInfoTask::IsUnProcessedClusterMapPresent(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    //If unprocessed cluster map is present, do not process further 
    Files_t files;
    if( !m_FastSync->GetFileList(m_cxTransport, m_FastSync->getUnProcessedClusterBitMapFileName(), files ) ) 
    {
        std::stringstream excmsg;
        excmsg << "Failed to do get file list to know presence of " << m_FastSync->getUnProcessedClusterBitMapFileName() << '\n';
        throw DataProtectionException(excmsg.str());
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return (files.size() == 1);
}


void FastSync::GenerateClusterInfoTask::LoadProcessedClusterMapIfPresent(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    FileInfosPtr ProcessedClusterInfo( new FileInfos_t ) ;

    std::stringstream msg;
    boost::tribool tbms = m_cxTransport->listFile(m_FastSync->getProcessedClusterBitMapFileName(), *ProcessedClusterInfo.get());
    if( !tbms )
    {
        msg << "FAILED: unable to find presence of processed cluster bitmap file: "
            << m_FastSync->getProcessedClusterBitMapFileName() << '\n';
        throw  DataProtectionException( msg.str() );
    }
    else if( tbms )
    {
        /* File is present */
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s ProcessedClusterBitMapFile present in CX[%s]\n", FUNCTION_NAME, m_FastSync->JobId().c_str(), m_FastSync->getProcessedClusterBitMapFileName().c_str());
        LoadProcessedClusterMap();
    }
    else
    {
        /* File is not present */
        DebugPrintf(SV_LOG_DEBUG, "File %s is not present in CX.\n", m_FastSync->getProcessedClusterBitMapFileName().c_str());
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}


void FastSync::GenerateClusterInfoTask::LoadProcessedClusterMap(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    std::ostringstream msg;

    SharedBitMapPtr processedClusterMap ;
    if(!m_FastSync->GetClusterParams()->initSharedBitMapFile(processedClusterMap, m_FastSync->getProcessedClusterBitMapFileName(), m_cxTransport, true))
    {
        msg << "FAILED FastSync::Failed to initialize processed cluster map from file: " << m_FastSync->getProcessedClusterBitMapFileName() << '\n';
        throw DataProtectionException( msg.str() ) ;
    }

    if( processedClusterMap->syncToPersistentStore(m_cxTransport, m_FastSync->getClusterBitMapFileName())  != SVS_OK )
    {
        msg << "Failed to persist the processed cluster map as " 
            << m_FastSync->getClusterBitMapFileName() << '\n';
        throw DataProtectionException( msg.str() );
    }

    if(!m_FastSync->GetClusterParams()->initSharedBitMapFile(m_clusterTransferMap, m_FastSync->getClusterBitMapFileName(), m_cxTransport, true))
    {
        msg << "FAILED FastSync::Failed to initialize cluster transfer map.." << m_FastSync->getClusterBitMapFileName() << '\n';
        throw DataProtectionException( msg.str() );
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Processing ProcessedClusterBitMapFile[%s] in ClusterBitMapFile[%s]\n",
        FUNCTION_NAME, m_FastSync->JobId().c_str(), m_FastSync->getProcessedClusterBitMapFileName().c_str(), m_FastSync->getClusterBitMapFileName().c_str());

    /* TODO: there is a small issue here that if any files
     *       are missed, like cluster, hcd or sync file, 
     *       after reconciliation (total value goes down during reconciliation), 
     *       At last, final correct value is getting update after transition is 
     *       asked for by process hcd thread. Hence getting error; this has 
     *       to be fixed.
     *       May be we can call setfastsyncfullsyncbytes from process hcd thread using cluster generator pointer
     *       before asking for transition but this is not a major issue.
     */
    if ( !m_FastSync->SetFastSyncFullSyncBytes( m_clusterTransferMap->getBytesProcessed()) )
    {
        throw DataProtectionException("FAILED Failed to update full sync bytes for volume " + m_FastSync->GetDeviceName() + " after loading processed cluster map\n");
    }
    m_prevFullSyncBytesSent = m_clusterTransferMap->getBytesProcessed();

    if( !m_cxTransport->deleteFile( m_FastSync->getProcessedClusterBitMapFileName() ) )
    {
        /* TODO: should we exit here by throwing exception ? */
        DebugPrintf(SV_LOG_WARNING, "Unable to remove %s from CX.. tal status %s\n", m_FastSync->getProcessedClusterBitMapFileName().c_str(), 
                    m_cxTransport->status() ) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void FastSync::GenerateClusterInfoTask::sendUnProcessedClusterIfRequired(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    std::ostringstream msg;
    Files_t files;
    string preClusterUnProcessed = m_FastSync->getUnProcessedPreClusterBitMapFileName();
    string completedUnProcessed = m_FastSync->getUnProcessedClusterBitMapFileName();

    if (m_FastSync->IsTransportProtocolAzureBlob())
    {
        preClusterUnProcessed = completedUnProcessed;
    }

    if( m_clusterTransferMap->syncToPersistentStore(m_cxTransport)  != SVS_OK )
    {
        throw DataProtectionException("FAILED FastSync:Unable to persist cluster transfer map\n");
    }

    if( !m_FastSync->GetFileList(m_cxTransport, completedUnProcessed, files) )
    {
        msg << "FAILED FastSync::Failed GetFileList for file spec " << completedUnProcessed <<" tal status : " << m_cxTransport->status() << endl ;
        throw DataProtectionException( msg.str() ) ;
    }

    if( files.size() == 0 )  //Send the unprocessed cluster dat file if it doesnt already exist at cx..
    {
        if ( m_clusterTransferMap->syncToPersistentStore(m_cxTransport, preClusterUnProcessed ) != SVS_OK )
        {
            msg << "FAILED FastSync::sendUnProcessedClusterIfRequired, unable to persist pre cluster unprocessed file: "
                << preClusterUnProcessed << '\n';
            throw  DataProtectionException ( msg.str() ) ;
        }

        /* This is a must because due to above, cluster transfer bitmap file name becomes the precompleted cluster  
         * and every thing breaks */
        if ( m_clusterTransferMap->syncToPersistentStore(m_cxTransport, m_FastSync->getClusterBitMapFileName()) != SVS_OK )
        {
            msg << "FAILED FastSync::sendUnProcessedClusterIfRequired, Unable to persist cluster transfer Map: "
                << m_FastSync->getClusterBitMapFileName() << '\n';
            throw DataProtectionException( msg.str() );
        }

        if (!m_FastSync->IsTransportProtocolAzureBlob()) // Skip rename for AzureBlob transport as the name is already completedUnProcessed here.
        {
            boost::tribool tb = m_cxTransport->renameFile(preClusterUnProcessed, completedUnProcessed, COMPRESS_NONE);
            if (!tb || CxTransportNotFound(tb))
            {
                msg << "FAILED FastSync::Failed rename cluster map unprocessed packet From " << preClusterUnProcessed << " To " \
                    << completedUnProcessed << " Tal Status : " << m_cxTransport->status() << endl;
                m_cxTransport->deleteFile(preClusterUnProcessed);
                throw DataProtectionException(msg.str());
            }
        }
    }
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Sent UnProcessedClusterBitMapFile[%s] (copy of ClusterBitMapFile[%s])\n",
        FUNCTION_NAME, m_FastSync->JobId().c_str(), completedUnProcessed.c_str(), m_FastSync->getClusterBitMapFileName().c_str());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return ;
}

