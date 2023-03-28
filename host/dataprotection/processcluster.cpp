#include "fastsync.h"
#include "theconfigurator.h"
#include "dataprotectionutils.h"
#include "processcluster.h"
#include "configurevxagent.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "configwrapper.h"
#include "inmalertdefs.h"

FastSync::ProcessClusterStage::ProcessClusterStage(FastSync* fastSync, SharedBitMapPtr checksumTransferMap)
    : m_FastSync(fastSync),
      m_checksumTransferMap(checksumTransferMap),

      m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
      m_volume( m_FastSync->createVolume() ),
      m_pGenerateHcdStage(0),
      m_pSendHcdStage(0),
      m_pDiskRead(NEWPROFILER_WITHTSBKT(DISKREAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true))
{
    m_pendingFileCount = 0 ;

    m_quitRequest[0] = &FastSync::ProcessClusterStage::QuitRequestedInApp;
    m_quitRequest[1] = &FastSync::ProcessClusterStage::QuitRequestedInStage;

    m_clusterInfoForRead[0] = &FastSync::ProcessClusterStage::GetClusterInfoForReadInApp;
    m_clusterInfoForRead[1] = &FastSync::ProcessClusterStage::GetClusterInfoForReadInStage;

    m_sendClusterInfoToHcdGen[0] = &FastSync::ProcessClusterStage::SendClusterInfoToHcdGenInApp;
    m_sendClusterInfoToHcdGen[1] = &FastSync::ProcessClusterStage::SendClusterInfoToHcdGenInStage;
}


void FastSync::ProcessClusterStage::DecrementPendingCount(void)
{
    m_pendingFileCount--;
}


void FastSync::ProcessClusterStage::SetGenAndSendHcdStage(GenerateHcdStage *pgenhcdstage, SendHcdStage *psendhcdstage)
{
    m_pGenerateHcdStage = pgenhcdstage;
    m_pSendHcdStage = psendhcdstage;
    m_pGenerateHcdStage->SetSendHcdStage(m_pSendHcdStage);
}


unsigned long FastSync::ProcessClusterStage::getClusterSyncChunkSize(void)
{
    return m_FastSync->GetClusterParams()->getClusterSyncChunkSize();
}

// ProcessClusterStage
/* TODO: some where in this pipe, need to fit the 
 * limit of 1000 hcds code; 
 * Also check where throttle code has to be put */
int FastSync::ProcessClusterStage::svc()
{  
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string clusterNoMoreFile = completedClusterPrefix + m_FastSync->JobId() + noMore ;
    ACE_Time_Value lastUpdateTime = ACE_OS::gettimeofday(), currentTime = ACE_OS::gettimeofday() ;
    std::stringstream excmsg;

    try 
    {
        if (!m_FastSync->GetClusterParams()->AreValid(excmsg))
        {
            excmsg << ". For volume " << m_FastSync->GetDeviceName() << '\n';
            throw DataProtectionException(excmsg.str());
        }

        if (!m_FastSync->IsReadBufferLengthValid(excmsg))
        {
            throw DataProtectionException(excmsg.str());
        }

        /* TODO: alignment should be based and physical disk sector size (as seen in 3TB case which overrides
           system's page size; Also there getVxAlignmentSize is SV_ULONGLONG, make this proper; 
           Make all allocations in size_t */
        if (!m_ClusterInfosForHcd.Init(m_FastSync->IsProcessClusterPipeEnabled() ? NREADBUFSFORHCDINPIPE : 1,
                                       m_FastSync->getReadBufferSize(), m_FastSync->GetConfigurator().getVxAlignmentSize()))
        {
            excmsg << "For volume " << m_FastSync->GetDeviceName()
                   << ", could not initialize cluster infos for hcd\n";
            throw DataProtectionException(excmsg.str());
        }

        FileInfosPtr fileInfos( new FileInfos_t ) ;
        FileInfosPtr fileInfosToProcess (new FileInfos_t ) ;

        std::string remoteDir = m_FastSync->RemoteSyncDir() ;       
        std::string prefixName = remoteDir + ClusterBitmapPrefix;

        m_prevFullyUnUsedBytes = 0;

        Profiler pcl = NEWPROFILER(CLUSTERLIST, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), false);
        Profiler pcd = NEWPROFILER(CLUSTERDOWNLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), false);;
        do
        {
            currentTime = ACE_OS::gettimeofday()  ;
            if( (currentTime - lastUpdateTime).sec() >=  m_FastSync->m_ResyncUpdateInterval)
            {
                updateProgress() ;
                lastUpdateTime = ACE_OS::gettimeofday() ;
            }

            if (!canSendHcds())
            {
                DebugPrintf(SV_LOG_DEBUG, "cannot send hcd now as either missing hcd or missing sync map is present\n");
                continue;
            }

            /* TODO: if there is no pipeline configured, 
             * reading volume and sending hcd by same thread
             * will cause a definite timeout for this; 
             * should we put heart beat at appropriate places */
            if( 0 != pendingFileCount() )
            {
                // still processing last list of files, send a heartbeat
                // to avoid timing out this connection
                m_cxTransport->heartbeat();
            }
            else
            {
                pcl->start();
                if( !m_cxTransport->listFile( prefixName, *fileInfos.get() ) )
                {
                    excmsg << "failed to get list of cluster bitmap file "
                           << "with transport status " << m_cxTransport->status()
                           << " for volume " << m_FastSync->GetDeviceName()
                           << '\n';
                    throw DataProtectionException(excmsg.str());
                }
                pcl->stop(0);

                if( 1 == fileInfos->size() )
                {
                    if( std::string::npos != (*fileInfos->begin()).m_name.find(clusterNoMoreFile) )
                    {
                        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Processing ClusterNoMore file[%s] as this is the only file list\n",
                            FUNCTION_NAME, m_FastSync->JobId().c_str(), std::string(remoteDir + clusterNoMoreFile).c_str());
                        if (!m_FastSync->sendHCDNoMoreIfRequired(m_cxTransport))
                        {
                            excmsg << "failed to send hcd no more file "
                                   << "for volume " 
                                   << m_FastSync->GetDeviceName() << '\n';
                            throw DataProtectionException(excmsg.str());
                        }
                        if( !m_cxTransport->deleteFile(remoteDir + clusterNoMoreFile) )
                        {
                            excmsg << "failed to delete file " << remoteDir
                                   << clusterNoMoreFile
                                   << ", with error " << m_cxTransport->status() << '\n';
                            throw DataProtectionException(excmsg.str());
                        }
                        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s deleted ClusterNoMore file[%s]\n",
                            FUNCTION_NAME, m_FastSync->JobId().c_str(), std::string(remoteDir + clusterNoMoreFile).c_str());
                        continue;
                    }
                }

                SV_ULONG nclusterfilestoprocess = 0;
                if (fileInfos->size())
                {
                    fileInfosToProcess->clear();
                    nclusterfilestoprocess = GetClusterFilesToProcess(fileInfos, fileInfosToProcess);
                    if (0 == nclusterfilestoprocess)
                    {
                        continue;
                    }
                }

                DebugPrintf(SV_LOG_DEBUG, "nclusterfilestoprocess = %lu\n", nclusterfilestoprocess);
                setPendingFileCount( nclusterfilestoprocess );
                FileInfos_t::iterator iter(fileInfosToProcess->begin());
                FileInfos_t::iterator iterEnd(fileInfosToProcess->end());
                for( SV_ULONG i = 0; (iter != iterEnd) && (i < nclusterfilestoprocess) && (!QuitRequested(0)); ++iter, ++i)
                {
                    std::string& fileName = (*iter).m_name;
                    std::string clusterFile = remoteDir + fileName ;
                    char* inBuffer ;
                    const std::size_t bufferSize = (*iter).m_size ;
                    size_t bytesReturned = 0;

                    pcd->start();
                    if (m_cxTransport->getFile(clusterFile, bufferSize, &inBuffer, bytesReturned))
                    {
                        ClusterFilePtr_t pclusterfile(new ClusterFile_t());
                        pclusterfile->fileName = clusterFile ;
                        try
                        {
                            pclusterfile->fsClusterInfo.reset( new FsClusterInfo( inBuffer, bufferSize ) ) ;
                            DebugPrintf(SV_LOG_DEBUG, "The clusterfilename is %s\n", pclusterfile->fileName.c_str());
                            process(pclusterfile);
                            pcd->stop(bufferSize);
                        }
                        catch (CorruptResyncFileException& dpe)
                        {
                            DebugPrintf(SV_LOG_ERROR, "%s encountered CorruptResyncFileException : %s\n", FUNCTION_NAME, dpe.what());
                            InvalidResyncFileAlert a(m_FastSync->GetDeviceName(), clusterFile);
                            m_FastSync->SendAlert(SV_ALERT_ERROR, SV_ALERT_MODULE_RESYNC, a);
                            const std::string corruptfile = remoteDir + CORRUPT UNDERSCORE + fileName;
                            boost::tribool tb = m_cxTransport->renameFile(clusterFile, corruptfile, COMPRESS_NONE);
                            if (!tb || CxTransportNotFound(tb))
                            {
                                DebugPrintf(SV_LOG_ERROR, "%s: Failed to prefix invalid cluster file %s with " CORRUPT UNDERSCORE ", with error %s\n",
                                    FUNCTION_NAME, clusterFile.c_str(), m_cxTransport->status());
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_DEBUG, "%s: Prefixed invalid hcd with " CORRUPT UNDERSCORE "\n", FUNCTION_NAME);
                            }
                        }
                    }
                    else
                    {
                        throw DataProtectionException( "Retrieving cluster bitmap file from CX failed...\n" ) ;
                    }

                    currentTime = ACE_OS::gettimeofday() ;
                    if( ( currentTime - lastUpdateTime ).sec() > m_FastSync->m_ResyncUpdateInterval )
                    {
                        updateProgress();
                        lastUpdateTime = ACE_OS::gettimeofday() ;
                    }
                }
            }
        }while (!QuitRequested(WindowsWaitTimeInSeconds))  ;

        m_pDiskRead->commit();
        updateProgress();
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        InformStatus(dpe.what());
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        InformStatus(e.what());
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return 0;
}


void FastSync::ProcessClusterStage::process(ClusterFilePtr_t &pclusterfile)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    /* DebugPrintf(SV_LOG_DEBUG, "From cluster file name: %s:\n", pclusterfile->fileName.c_str());
       pclusterfile->fsClusterInfo->Print(); */
    std::stringstream excmsg;

    /* validity of received cluster bitmap */
    if (!m_FastSync->AreClusterParamsValid(getClusterSyncChunkSize(), pclusterfile->fsClusterInfo->GetSize(), excmsg))
    {
        throw DataProtectionException(excmsg.str(), SV_LOG_ERROR);
    }

    if (!AllocateClusterBatchIfReq(pclusterfile))
    {
        excmsg << "For volume " << m_FastSync->GetDeviceName() << " failed to allocated cluster batch\n";
        throw DataProtectionException(excmsg.str());
    }

    SV_UINT clustersremaining = pclusterfile->fsClusterInfo->GetCount();
    SV_UINT nclustersinsyncchunk = m_FastSync->getChunkSize() / pclusterfile->fsClusterInfo->GetSize();
    DebugPrintf(SV_LOG_DEBUG, "number of clusters in sync chunk is %u\n", nclustersinsyncchunk);
    SV_UINT startingcluster = 0;
    SV_UINT nclustersinreadbuffer = m_FastSync->getReadBufferSize() / pclusterfile->fsClusterInfo->GetSize();
    DebugPrintf(SV_LOG_DEBUG, "number of clusters in read buffer is %u\n", nclustersinreadbuffer);
     
    while (clustersremaining)
    {
        SV_UINT clusterstoprocess = (clustersremaining > nclustersinsyncchunk) ? nclustersinsyncchunk : clustersremaining;
        /* DebugPrintf(SV_LOG_DEBUG, "number of clusters to process = %u from start %u in one hcd\n", clusterstoprocess, startingcluster); */
        ProcessClustersInHcd(pclusterfile, startingcluster, clusterstoprocess, nclustersinreadbuffer);
        if (QuitRequested())
        {
            DebugPrintf(SV_LOG_DEBUG, "quit is requested while processing cluster bitmap file\n");
            break;
        }
        /* TODO: heartbeat after sending a hcd of 64 MB ? */
        m_cxTransport->heartbeat();
        startingcluster += clusterstoprocess;
        clustersremaining -= clusterstoprocess;
        /* DebugPrintf(SV_LOG_DEBUG, "number of clusters remaining = %u\n", clustersremaining); */
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}


void FastSync::ProcessClusterStage::ProcessClustersInHcd(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster, 
                                                         const SV_UINT &clusterstoprocess, const SV_UINT &nclustersinreadbuffer)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
 
    DebugPrintf(SV_LOG_DEBUG, "From cluster file %s, sending hcd from cluster number %u for %u clusters\n", 
                pclusterfile->fileName.c_str(), startingcluster, clusterstoprocess);
    ClustersInfo_t clustersinhcd(startingcluster, clusterstoprocess);
    SV_ULONGLONG clusternumber = pclusterfile->fsClusterInfo->GetStart() + startingcluster;
    SV_LONGLONG volumeoffset = clusternumber * pclusterfile->fsClusterInfo->GetSize();

    bool balreadyhcdsent = isHcdTransferred(pclusterfile, startingcluster, clusterstoprocess);
    if (QuitRequested())
    {
        return;
    }
    if (balreadyhcdsent)
    {
        DebugPrintf(SV_LOG_DEBUG, "The hcd file sent already from start " LLSPEC "\n", volumeoffset);
    }
    else
    {
        SV_UINT clustersremaining = clusterstoprocess;
        SV_UINT startingclusterforread = startingcluster;
         
        while (clustersremaining)
        {
            SV_UINT clusterstoprocessinread = (clustersremaining > nclustersinreadbuffer) ? nclustersinreadbuffer : clustersremaining;
            /* DebugPrintf(SV_LOG_DEBUG, "number of clusters to process = %u from start %u in one read\n", clusterstoprocessinread, startingclusterforread); */
            ProcessClustersInRead(pclusterfile, startingclusterforread, clusterstoprocessinread, clustersinhcd);
            if (QuitRequested())
            {
                DebugPrintf(SV_LOG_DEBUG, "quit is requested while processing clusters in hcd\n");
                break;
            }
            startingclusterforread += clusterstoprocessinread;
            clustersremaining -= clusterstoprocessinread;
            /* DebugPrintf(SV_LOG_DEBUG, "number of clusters remaining = %u\n", clustersremaining); */
        }
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void FastSync::ProcessClusterStage::ProcessClustersInRead(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster, 
                                                          const SV_UINT &clusterstoprocess, const ClustersInfo_t &clustersinhcd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    SV_ULONGLONG clusternumber = pclusterfile->fsClusterInfo->GetStart() + startingcluster;
    SV_LONGLONG readoffset = clusternumber * pclusterfile->fsClusterInfo->GetSize();
    SV_UINT readlength = clusterstoprocess * pclusterfile->fsClusterInfo->GetSize();
    std::stringstream excmsg;
    ClustersInfo_t clustersinread(startingcluster, clusterstoprocess);

    /*
      DebugPrintf(SV_LOG_DEBUG, "From cluster file %s, sending from cluster number %u for %u clusters in one read\n", 
      pclusterfile->fileName.c_str(), startingcluster, clusterstoprocess);
    */

    /* loop is needed because until we process, should not go out ;
     * as offsets and count of clusters increment call to this function */
    while (!QuitRequested())
    {
        ClusterInfoForHcd *pcihcd = GetClusterInfoForRead();
        if (pcihcd)
        {
            DebugPrintf(SV_LOG_DEBUG, "got cluster info for read: %p\n", pcihcd);
            pcihcd->m_pClusterBatch->SetClusterInfo(pclusterfile);
            pcihcd->m_pClusterBatch->SetClustersInHcd(clustersinhcd);
            pcihcd->m_pClusterBatch->SetBitmapToProcess(pclusterfile, clustersinread);
            ReadInfo &ri = pcihcd->m_ReadInfo;
            ri.Reset();
            if (pcihcd->m_pClusterBatch->IsAnyClusterUsed())
            {
                DebugPrintf(SV_LOG_DEBUG, "asking for data from volume: %s, offset: " LLSPEC 
                            ", length: %u\n", m_FastSync->GetDeviceName().c_str(),
                            readoffset, readlength);
                QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::ProcessClusterStage::QuitRequested), this);
                ReadInfo::ReadInputs_t rips(m_volume.get(), readoffset, readlength,
                                            true, ReadInfo::ReadRetryInfo_t(), qf); 
                m_pDiskRead->start();
                if (!ri.Read(rips))
                {
                    excmsg << "For volume " << m_FastSync->GetDeviceName()
                           << ", read failed with error message: " 
                           << ri.GetErrorMessage() << "\n";
                    cdp_volume_t::Ptr volume;
                    std::string errmsg;
                    if (!m_FastSync->createVolume(volume, errmsg) && (ENOENT != volume->LastError()))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: sending VolumeReadFailureAlert, Error: %s.\n", FUNCTION_NAME, errmsg.c_str());
                        SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, VolumeReadFailureAlert(m_FastSync->GetDeviceName()));
                    }
                    QuitRequested(120);
                    throw DataProtectionException(excmsg.str());
                }
                m_pDiskRead->stop(rips.m_Length);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "From cluster file %s, from cluster number %u, %u clusters are unused. Hence not reading volume\n", 
                            pclusterfile->fileName.c_str(), startingcluster, clusterstoprocess);
            }
            SendClusterInfoToHcdGen(pcihcd);
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "GetClusterInfoForRead returned null for volume %s.\n",
                        m_FastSync->GetDeviceName().c_str());
            /* TODO: heartbeat here ? */
            m_cxTransport->heartbeat();
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool FastSync::ProcessClusterStage::AllocateClusterBatchIfReq(ClusterFilePtr_t &pclusterfile)
{
    bool balloced = m_ClusterInfosForHcd.m_bAllocated;

    if (!balloced)
    {
        for (unsigned int i = 0; i < m_ClusterInfosForHcd.m_NumClusterInfos; i++)
        {
            balloced = m_ClusterInfosForHcd.m_pClusterInfos[i].AllocateClusterBatch(m_FastSync->getReadBufferSize(), 
                                                                                    pclusterfile->fsClusterInfo->GetSize());
            if (!balloced)
            {
                break;
            }
        }
        m_ClusterInfosForHcd.m_bAllocated = balloced;
    }

    return balloced;
}


/* TODO: write the logic of deleting the 
 * cluster bitmap file, if all are already processed ? 
 * wait for all of hcds to be sent: 
 * for eg: last hcd - 1th is in progress and
 * last is already processed, then if cluster file is 
 * deleted, then if last - 1th send fails, then no file
 * is present to send information */
bool FastSync::ProcessClusterStage::isHcdTransferred(SV_ULONGLONG offset)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    checksumTransferMap()->dumpBitMapInfo();
    SV_ULONG chunkNo = (SV_ULONG) (offset / m_FastSync->getChunkSize());
    bool indx = checksumTransferMap()->isBitSet(chunkNo * 2);
    bool indxPlus1 = checksumTransferMap()->isBitSet(chunkNo * 2 + 1);

    bool bReturnValue = (indx || indxPlus1);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bReturnValue ;
}


bool FastSync::ProcessClusterStage::isHcdTransferred(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster,
                                                     const SV_UINT &clusterstoprocess)
{
    SV_ULONGLONG clusternumber = pclusterfile->fsClusterInfo->GetStart() + startingcluster;
    SV_LONGLONG volumeoffset = clusternumber * pclusterfile->fsClusterInfo->GetSize();
    bool brval = false;

    if (isHcdTransferred(volumeoffset))
    {
        brval = true;

        if (pclusterfile->fsClusterInfo->GetCount() == (startingcluster + clusterstoprocess))
        {
            /* last hcd */
            /* TODO: test at last */
            DebugPrintf(SV_LOG_DEBUG, "From cluster file: %s, volume offset " LLSPEC 
                        ", hcd of last clusters is already sent. "
                        "Hence deletion of this cluster file will happen "
                        "only if all before hcds in this cluster file are sent\n",
                        pclusterfile->fileName.c_str(), volumeoffset);
            SV_UINT clustersremaining = pclusterfile->fsClusterInfo->GetCount();
            SV_UINT nclustersinsyncchunk = m_FastSync->getChunkSize() / pclusterfile->fsClusterInfo->GetSize();
            SV_UINT eachstartingcluster = 0;
     
            while (clustersremaining)
            {
                SV_UINT eachclusterstoprocess = (clustersremaining > nclustersinsyncchunk) ? nclustersinsyncchunk : clustersremaining;
                WaitUntilHcdTransferred(pclusterfile, eachstartingcluster, eachclusterstoprocess);
                if (QuitRequested())
                {
                    DebugPrintf(SV_LOG_DEBUG, "quit is requested while processing cluster bitmap file\n");
                    break;
                }
                eachstartingcluster += eachclusterstoprocess;
                clustersremaining -= eachclusterstoprocess;
            }

            if (!QuitRequested())
            {
                DebugPrintf(SV_LOG_DEBUG, "volume read thread deleting cluster file %s\n", pclusterfile->fileName.c_str());
                /* TODO: test */
                if( !m_cxTransport->deleteFile(pclusterfile->fileName) )
                {
                    std::stringstream excmsg;
                    excmsg << "removal of "
                           << pclusterfile->fileName
                           << " failed: "
                           << m_cxTransport->status()
                           << "\n";
                    throw DataProtectionException(excmsg.str());
                }
                DecrementPendingCount();
            }
        }
    }

    return brval;
}


void FastSync::ProcessClusterStage::WaitUntilHcdTransferred(ClusterFilePtr_t &pclusterfile, const SV_UINT &startingcluster,
                                                            const SV_UINT &clusterstoprocess)
{
    SV_ULONGLONG clusternumber = pclusterfile->fsClusterInfo->GetStart() + startingcluster;
    SV_LONGLONG volumeoffset = clusternumber * pclusterfile->fsClusterInfo->GetSize();
    
    do
    {
        if (isHcdTransferred(volumeoffset))
        {
            break;
        }
        /* TODO: should we have heartbeat here ? */
        m_cxTransport->heartbeat();
    } while (!QuitRequested(1));
}


void FastSync::ProcessClusterStage::InformStatus(const std::string &s)
{
    if (m_FastSync->IsProcessClusterPipeEnabled())
    {
        SetExceptionStatus(s);
    }
    else
    {
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }
}


void FastSync::ProcessClusterStage::setPendingFileCount( const SV_ULONG& count )
{
    m_pendingFileCount = count ;
}


SV_ULONG FastSync::ProcessClusterStage::pendingFileCount()
{
    return m_pendingFileCount.value() ;
}


SharedBitMap* FastSync::ProcessClusterStage::checksumTransferMap()
{
    return m_checksumTransferMap.get();
}


void FastSync::ProcessClusterStage::markAsFullyUsed(SV_ULONGLONG offset)
{
    m_checksumTransferMap->markAsFullyUsed(offset);
}


void FastSync::ProcessClusterStage::markAsFullyUnUsed(SV_ULONGLONG offset, SV_ULONG unusedbytes)
{
    m_checksumTransferMap->markAsFullyUnUsed(offset, unusedbytes);
}

 
void FastSync::ProcessClusterStage::markAsPartiallyUsed(SV_ULONGLONG offset)
{
    m_checksumTransferMap->markAsPartiallyUsed(offset);
}


bool FastSync::ProcessClusterStage::QuitRequested(long int seconds)
{
    QuitRequested_t p = m_quitRequest[m_FastSync->IsProcessClusterPipeEnabled()];
    return (this->*p)(seconds);
}


bool FastSync::ProcessClusterStage::QuitRequestedInStage(long int seconds)
{
    return ShouldQuit(seconds);
}


bool FastSync::ProcessClusterStage::QuitRequestedInApp(long int seconds)
{
    return m_FastSync->QuitRequested(seconds);
}


ClusterInfoForHcd * FastSync::ProcessClusterStage::GetClusterInfoForRead(void)
{
    GetClusterInfoForRead_t p = m_clusterInfoForRead[m_FastSync->IsProcessClusterPipeEnabled()];
    return (this->*p)();
}


ClusterInfoForHcd * FastSync::ProcessClusterStage::GetClusterInfoForReadInStage(void)
{
    ClusterInfoForHcd *p = 0;
    if (m_ClusterInfosForHcd.m_FreeIndex < m_ClusterInfosForHcd.m_NumClusterInfos)
    {
        p = m_ClusterInfosForHcd.m_pClusterInfos + m_ClusterInfosForHcd.m_FreeIndex;
        m_ClusterInfosForHcd.m_FreeIndex++;
    }
    else
    {
        void *pv = ReadFromForwardStage(TaskWaitTimeInSeconds);
        p = (ClusterInfoForHcd *)pv;
    }

    return p;
}


ClusterInfoForHcd * FastSync::ProcessClusterStage::GetClusterInfoForReadInApp(void)
{
    return m_ClusterInfosForHcd.m_pClusterInfos;
}


void FastSync::ProcessClusterStage::SendClusterInfoToHcdGen(ClusterInfoForHcd *pcihcd)
{
    SendClusterInfoToHcdGen_t p = m_sendClusterInfoToHcdGen[m_FastSync->IsProcessClusterPipeEnabled()];
    (this->*p)(pcihcd);
}


void FastSync::ProcessClusterStage::SendClusterInfoToHcdGenInStage(ClusterInfoForHcd *pcihcd)
{
    /* TODO: find out what is the size for in message block ? will 
     *       it cause any allocations and so on ? */
    if (!WriteToForwardStage(pcihcd, sizeof(ClusterInfoForHcd), TaskWaitTimeInSeconds))
    {
        std::stringstream excmsg;
        excmsg << "For volume " << m_FastSync->GetDeviceName()
               << ", forwarding the cluster info in read to generate hcd stage failed\n";
        throw DataProtectionException(excmsg.str());
    }
}


void FastSync::ProcessClusterStage::SendClusterInfoToHcdGenInApp(ClusterInfoForHcd *pcihcd)
{
    m_pGenerateHcdStage->process(pcihcd);
}


void FastSync::ProcessClusterStage::updateProgress( void )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    /* std::stringstream excmsg; */

    /* TODO: updation of used and unused bytes to CX */
    if( (m_checksumTransferMap->syncToPersistentStore(m_cxTransport) == SVS_OK) &&  
        (m_prevFullyUnUsedBytes.value() != m_checksumTransferMap->getBytesNotProcessed()) &&
        (m_FastSync->SetFastSyncFullyUnusedBytes(m_checksumTransferMap->getBytesNotProcessed())) )
    {
        m_prevFullyUnUsedBytes = m_checksumTransferMap->getBytesNotProcessed();
        DebugPrintf(SV_LOG_DEBUG, "updating unused bytes as: " ULLSPEC "\n",
                    m_prevFullyUnUsedBytes.value());
    }
    /*
      else
      {  
      excmsg << "hcd transfer bitmap storing to CX failed for " << m_FastSync->getHcdTransferMapFileName() << '\n';
      throw DataProtectionException(excmsg.str());
      }
    */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return ;
}


SV_ULONG FastSync::ProcessClusterStage::GetClusterFilesToProcess(FileInfosPtr &fileInfos, FileInfosPtr &fileInfosToProcess)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME ) ;

    SV_ULONG n = 0;
    std::stringstream excmsg;
    excmsg << "For volume " << m_FastSync->GetDeviceName() << ", ";
    std::string remoteDir = m_FastSync->RemoteSyncDir() ;
    std::string clusterNoMoreFile = completedClusterPrefix + m_FastSync->JobId() + noMore ;

    FileInfos_t::iterator iter(fileInfos->begin());
    FileInfos_t::iterator iterEnd(fileInfos->end());
    for( /* empty */ ; iter != iterEnd; ++iter)
    {
        std::string& fileName = (*iter).m_name;
        if( !canProcess( fileName, m_FastSync->JobId() ) )
        {
            //Log the error and Delete the file.
            DebugPrintf( SV_LOG_ERROR,
                         "File received %s from CX seems to be older file. Skipping and Deleting from CX\n",
                         fileName.c_str() ) ;
            m_cxTransport->deleteFile( remoteDir + fileName ) ;
        }
        /* TODO: enable missing stuff */
        else if ((fileName != clusterNoMoreFile) && 
                 (fileName != ClusterNoMoreFileName) /* && 
                                                        (fileName != clusterMissingFile) */ )
        {
            fileInfosToProcess->push_back(*iter);
        }
    }

    SV_UINT numMaxHcds = m_FastSync->GetMaxHcdsAllowdAtCx();
    std::string prefixName = remoteDir + HcdPrefix;
    Files_t files;
    if (m_FastSync->GetFileList(m_cxTransport, prefixName, files))
    {
        size_t numPresent = files.size();
        unsigned long nhcdfilesinclusterfile = getClusterSyncChunkSize() / m_FastSync->getChunkSize();
        SV_LONGLONG llNNeedToSend = fileInfosToProcess->size() * nhcdfilesinclusterfile;
        SV_LONGLONG save = llNNeedToSend;
        std::stringstream msg;
        msg << "number of hcds to send = " << save << ','
            << "number of hcds already present at CX = " << numPresent << ','
            << "number of hcds in a cluster file = " << nhcdfilesinclusterfile << ','
            << "limit on number of hcds present = " << numMaxHcds;
        if ((numPresent + llNNeedToSend) > numMaxHcds)
        {
            SV_LONGLONG llNExisting = numPresent;
            SV_LONGLONG llLimit = numMaxHcds;
            llNNeedToSend = llLimit - llNExisting;
        }
        if (llNNeedToSend > 0)
        {
            n = llNNeedToSend / nhcdfilesinclusterfile;
            msg << ", number of hcds that can be sent = " << llNNeedToSend
                << ", number of cluster files that can be processed = " << n;
            DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
            SV_UINT nsecstowaitforhcdsend = m_FastSync->GetSecsToWaitForHcdSend();
            DebugPrintf(SV_LOG_DEBUG, "hence waiting for %u seconds\n", nsecstowaitforhcdsend);
            QuitRequested(nsecstowaitforhcdsend);
        }
    }
    else
    {
        excmsg << "could not get file list for knowing number of hcds at PS\n";
        throw DataProtectionException(excmsg.str());
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
    return n;
}


bool FastSync::ProcessClusterStage::canSendHcds(void)
{
    const std::string& remoteDir = m_FastSync->RemoteSyncDir() ;
    const std::string& jobId = m_FastSync->JobId() ;
    std::string completedMissingSyncsPkt = remoteDir +  completedSyncPrefix + jobId + missingSuffix;
    std::string completedMissingHCDsPkt = remoteDir +  completedHcdPrefix + jobId + missingSuffix;
    std::stringstream excmsg;

    Files_t missinghcds;
    if (!m_FastSync->GetFileList(m_cxTransport, completedMissingHCDsPkt, missinghcds))
    {
        excmsg << "GetFileList failed for listing " << completedMissingHCDsPkt << '\n';
        throw DataProtectionException(excmsg.str());
    }
  
    Files_t missingsyncs;
    if (!m_FastSync->GetFileList(m_cxTransport, completedMissingSyncsPkt, missingsyncs))
    {
        excmsg << "GetFileList failed for listing " << completedMissingSyncsPkt << '\n';
        throw DataProtectionException(excmsg.str());
    }

    return (missinghcds.size() == 0) && (missingsyncs.size() == 0);
}


bool ClusterInfoForHcd::Init(const SV_UINT &readlen, const SV_UINT &alignment)
{
    ReleaseClusterBatch();
    m_pClusterBatch = new (std::nothrow) FsClusterProcessBatch;
    return m_pClusterBatch && m_ReadInfo.Init(readlen, alignment);
}


void ClusterInfoForHcd::ReleaseClusterBatch(void)
{
    if (m_pClusterBatch)
    {
        delete m_pClusterBatch;
        m_pClusterBatch = 0;
    }
}


ClusterInfoForHcd::~ClusterInfoForHcd()
{
    ReleaseClusterBatch();
}

void ClusterInfoForHcd::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== process cluster info for hcd: start =====\n");
    if (m_pClusterBatch)
    {
        m_pClusterBatch->Print();
    }
    m_ReadInfo.Print();
    DebugPrintf(SV_LOG_DEBUG, " ===== process cluster info for hcd: end =====\n");
}


bool ClusterInfoForHcd::AllocateClusterBatch(const SV_UINT &readbuffersize, const SV_UINT &clustersize)
{
    SV_UINT nclustersinreadbuffer;
    INM_SAFE_ARITHMETIC(nclustersinreadbuffer = InmSafeInt<SV_UINT>::Type(readbuffersize) / clustersize, INMAGE_EX(readbuffersize)(clustersize))
        SV_UINT rem;
    INM_SAFE_ARITHMETIC(rem = InmSafeInt<SV_UINT>::Type(nclustersinreadbuffer) % NBITSINBYTE, INMAGE_EX(nclustersinreadbuffer))
        SV_UINT add;
    add = rem ? 1 : 0;
    SV_UINT nbytesforbitmap;
    INM_SAFE_ARITHMETIC(nbytesforbitmap = (InmSafeInt<SV_UINT>::Type(nclustersinreadbuffer) / NBITSINBYTE) + add, INMAGE_EX(nclustersinreadbuffer)(add))
        DebugPrintf(SV_LOG_DEBUG, "allocating %u bytes for cluster bitmap in cluster process batch\n", nbytesforbitmap);
    return m_pClusterBatch->Allocate(nbytesforbitmap);
}


void ClusterFile::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== cluster file : start =====\n");
    DebugPrintf(SV_LOG_DEBUG, "cluster file name: %s\n", fileName.c_str());
    if (fsClusterInfo)
    {
        fsClusterInfo->Print();
    }
    DebugPrintf(SV_LOG_DEBUG, " ===== cluster file : end =====\n");
}


ClusterInfosForHcd::~ClusterInfosForHcd()
{
    Release();
    m_NumClusterInfos = 0;
}


void ClusterInfosForHcd::Release(void)
{
    if (m_pClusterInfos)
    {
        delete [] m_pClusterInfos;
        m_pClusterInfos = 0;
    }
}


bool ClusterInfosForHcd::Init(const unsigned int &nclusterinfos, const SV_UINT &readlen, const SV_UINT &alignment)
{
    bool binitialized = false;

    Release();
    m_pClusterInfos = new (std::nothrow) ClusterInfoForHcd[nclusterinfos];
    if (m_pClusterInfos)
    {
        for (unsigned int i = 0; i < nclusterinfos; i++)
        {
            binitialized = m_pClusterInfos[i].Init(readlen, alignment);
            if (!binitialized)
            {
                DebugPrintf(SV_LOG_ERROR, "could not initialize cluster info for hcd\n");
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate cluster info for hcd\n");
    }

    if (binitialized)
    {
        m_NumClusterInfos = nclusterinfos;
    }

    return binitialized;
}


void ClustersInfo::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "clusters info: start = %u, count = %u\n", m_start, m_count);
}


