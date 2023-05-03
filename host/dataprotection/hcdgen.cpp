#include <cstdio>
#include <functional>
#include "fastsync.h"
#include "theconfigurator.h"
#include "dataprotectionutils.h"
#include "processcluster.h"
#include "hcdgen.h"
#include "hashcomparedata.h"
#include "configurevxagent.h"
#include "inmsafeint.h"
#include "inmageex.h"


FastSync::GenerateHcdStage::GenerateHcdStage(FastSync* fastSync)
: m_FastSync(fastSync),
  m_NumGenHcdThreads(m_FastSync->GetConfigurator().getMaxFastSyncGenerateHCDThreads()),
  m_hcdGenQ(new ACE_Message_Queue<ACE_MT_SYNCH>() ),
  m_hcdAckQ(new ACE_Message_Queue<ACE_MT_SYNCH>() ),
  m_pHcdGenInfo(0),
  m_pSendHcdStage(0),
  m_pHcdCalc(NEWPROFILER(HCDCALC, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true))
{ 
    m_quitRequest[0] = &FastSync::GenerateHcdStage::QuitRequestedInApp;
    m_quitRequest[1] = &FastSync::GenerateHcdStage::QuitRequestedInStage;

    m_sendBackClusterInfo[0] = &FastSync::GenerateHcdStage::SendBackClusterInfoInApp;
    m_sendBackClusterInfo[1] = &FastSync::GenerateHcdStage::SendBackClusterInfoInStage;

    m_hcdInfoToSend[0] = &FastSync::GenerateHcdStage::GetHcdInfoToSendInApp;
    m_hcdInfoToSend[1] = &FastSync::GenerateHcdStage::GetHcdInfoToSendInStage;

    m_forwardHcdInfoToSend[0] = &FastSync::GenerateHcdStage::ForwardHcdInfoToSendInApp;
    m_forwardHcdInfoToSend[1] = &FastSync::GenerateHcdStage::ForwardHcdInfoToSendInStage;
}


void FastSync::GenerateHcdStage::SetSendHcdStage(SendHcdStage *psendhcdstage)
{
    m_pSendHcdStage = psendhcdstage;
}


bool FastSync::GenerateHcdStage::Init(std::ostream &excmsg)
{
    bool binitialized = true;

    /* TODO: For now not enforcing this limit, 
     * Because maximum close is at hash compare data 
     * size and least hcd is / number of threads
    if ((m_FastSync->getReadBufferSize() / m_NumGenHcdThreads) < m_FastSync->GetHashCompareDataSize())
    {
        excmsg << "Number of generate hcd threads is incorrect."
               << "For read buffer size " << m_FastSync->getReadBufferSize()
               << ", hash compare data size " << m_FastSync->GetHashCompareDataSize()
               << ", maximum number of generate hcd threads should be " << (m_FastSync->getReadBufferSize() / m_FastSync->GetHashCompareDataSize())
               << ". Minimum should be 1\n";
        binitialized = false;
    }
    */

    if (binitialized)
    {
        if (!m_HcdInfosToSend.Init(m_FastSync->IsProcessClusterPipeEnabled() ? NBUFSFORHCDINPIPE : 1, 
                                   m_NumGenHcdThreads))
        {
            binitialized = false;
            excmsg << "could not initialize hcd infos for send\n";
        }
    }
     
    if (binitialized)
    {
        for (unsigned int i = 1; i < m_NumGenHcdThreads; i++)
        {
            HCDComputerPtr genTask( new HCDComputer( m_FastSync, &m_ThreadManager, this, m_hcdGenQ, m_hcdAckQ) );
            genTask->open() ;
            m_workerTasks.push_back( genTask ) ;
    
            ACE_Message_Block *mb = new ACE_Message_Block();
            mb->msg_priority(SYNC_NORMAL_PRIORITY);
            m_mesgBlks.push_back(mb);
        }

        m_pHcdGenInfo = new (std::nothrow) HcdGenInfo [m_NumGenHcdThreads];
        if (!m_pHcdGenInfo)
        {
            binitialized = false;
            excmsg << "could not allocate hcd genrate info\n";
        }
    }

    return binitialized;
}


int FastSync::GenerateHcdStage::svc()
{  
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

	try
	{
        std::stringstream excmsg;
        excmsg << "For volume " << m_FastSync->GetDeviceName() << ", ";
        if (!Init(excmsg))
        {
		    throw DataProtectionException(excmsg.str());
        }

        do
        {
            void *pv = ReadFromBackStage(TaskWaitTimeInSeconds);
            if (pv)
            {
                ClusterInfoForHcd *pcihcd = (ClusterInfoForHcd *)pv;
                process(pcihcd);
            }
           
        } while( !QuitRequested( 0 ) );
        m_pHcdCalc->commit();

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

    DeInit();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0 ;
}


void FastSync::GenerateHcdStage::DeInit(void)
{
    /* order is important here 
     * remove the hcd gen info 
     * if all threads have done */
	HCDComputerTasks_t::iterator beginIter( m_workerTasks.begin() ) ;
	HCDComputerTasks_t::iterator endIter( m_workerTasks.end() ) ;
	for( ; beginIter != endIter; beginIter++ )
	{
		HCDComputerPtr taskptr = (*beginIter);
		m_ThreadManager.wait_task(taskptr.get());
	}

    if (m_pHcdGenInfo)
    {
        delete [] m_pHcdGenInfo;
        m_pHcdGenInfo = NULL;
    }
}


void FastSync::GenerateHcdStage::process(ClusterInfoForHcd *pcihcd)
{
    DebugPrintf(SV_LOG_DEBUG, "got cluster info for generation: %p\n", pcihcd);
    /* pcihcd->Print(); */

    std::stringstream excmsg;
    if (!AllocateHcdBufferIfReq(pcihcd))
    {
        excmsg << "For volume " << m_FastSync->GetDeviceName() << " failed to allocated hcd buffer\n";
        throw DataProtectionException(excmsg.str());
    }

    /* do the calculations first and then look for input */
    SV_UINT nclustersperthread = (pcihcd->m_pClusterBatch->GetCountOfClustersToProcess() / m_NumGenHcdThreads) +
                                 ((pcihcd->m_pClusterBatch->GetCountOfClustersToProcess() % m_NumGenHcdThreads) ? 1 : 0);
    SV_UINT count = 0;
    SV_UINT start = pcihcd->m_pClusterBatch->GetStartingClusterToProcess();
    SV_UINT nclustersremaining = pcihcd->m_pClusterBatch->GetCountOfClustersToProcess();
    unsigned int volumedataremaining = pcihcd->m_ReadInfo.GetLength();
    unsigned int volumelength = 0;
    char *pvolumedata = pcihcd->m_ReadInfo.GetData();

    unsigned int i = 0;
    while (nclustersremaining)
    {
        if (nclustersremaining > nclustersperthread)
        {
            count = nclustersperthread;
            if ((nclustersremaining - count) < nclustersperthread)
            {
                count = nclustersremaining;
            }
        }
        else
        {
            count = nclustersremaining;
        }

        DebugPrintf(SV_LOG_DEBUG, "For thread %u, allocating %u clusters to generate hcd out of total %u\n", 
                                  i, count, pcihcd->m_pClusterBatch->GetCountOfClustersToProcess());

        m_pHcdGenInfo[i].m_pClusterBatch = pcihcd->m_pClusterBatch;
        m_pHcdGenInfo[i].m_clustersToProcess.m_start = start;
        m_pHcdGenInfo[i].m_clustersToProcess.m_count = count;
        m_pHcdGenInfo[i].m_volumeData.m_pData = 0;
        m_pHcdGenInfo[i].m_volumeData.m_length = 0;


        if (pcihcd->m_ReadInfo.GetLength())
        {
            volumelength = count * pcihcd->m_pClusterBatch->GetSize();
            m_pHcdGenInfo[i].m_volumeData.m_pData = pvolumedata;
            m_pHcdGenInfo[i].m_volumeData.m_length = volumelength;
            pvolumedata += volumelength;
            volumedataremaining -= volumelength;
        }

        start += count;
        nclustersremaining -= count;
        i++;
    }

    /* TODO: check if volumedataremaining should be zero ? */
    DebugPrintf(SV_LOG_DEBUG, "volumedataremaining = %u\n", volumedataremaining);

    HcdInfoToSend *phcdinfo = 0;
    /* loop is needed because until we process, should not go out ;
     * as offsets and count of clusters increment call to this function */
    while (!QuitRequested())
    {
        phcdinfo = GetHcdInfoToSend();
        if (phcdinfo)
        {
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "GetHcdInfoToSend returned null for volume %s.\n",
                                      m_FastSync->GetDeviceName().c_str());
        }
    }

    if (!QuitRequested() && !phcdinfo)
    {
        excmsg << "For volume " << m_FastSync->GetDeviceName() << " GetHcdInfoToSend returned null\n";
        throw DataProtectionException(excmsg.str());
    }

    if (!QuitRequested())
    {
        FillHcdBufferInGenInfo(phcdinfo);
        /* TODO: test i being less than 4 */
        GenerateHcd(i);
    }

    if (!QuitRequested())
    {
        FillHcdInfoToSend(phcdinfo, pcihcd);
        /* send back as soon as ack is received from worker cs threads */
        SendBackClusterInfo(pcihcd);
    }

    if (!QuitRequested())
    {
        ForwardHcdInfoToSend(phcdinfo);
    }
}


void FastSync::GenerateHcdStage::ForwardHcdInfoToSend(HcdInfoToSend *phcdinfo)
{
    ForwardHcdInfoToSend_t p = m_forwardHcdInfoToSend[m_FastSync->IsProcessClusterPipeEnabled()];
    (this->*p)(phcdinfo);
}


void FastSync::GenerateHcdStage::ForwardHcdInfoToSendInStage(HcdInfoToSend *phcdinfo)
{
    /* TODO: find out what is the size for in message block ? will 
     *       it cause any allocations and so on ? */
    if (!WriteToForwardStage(phcdinfo, sizeof(HcdInfoToSend), TaskWaitTimeInSeconds))
    {
        std::stringstream excmsg;
        excmsg << "For volume " << m_FastSync->GetDeviceName()
               << ", forwarding the hcd info to send stage failed\n";
        throw DataProtectionException(excmsg.str());
    }
}


void FastSync::GenerateHcdStage::ForwardHcdInfoToSendInApp(HcdInfoToSend *phcdinfo)
{
    m_pSendHcdStage->process(phcdinfo);
}


void FastSync::GenerateHcdStage::FillHcdInfoToSend(HcdInfoToSend *phcdinfo, ClusterInfoForHcd *pcihcd)
{
    FillClusterFileDetails(phcdinfo, pcihcd);
    phcdinfo->m_clustersInHcd.m_start = pcihcd->m_pClusterBatch->GetStartingClusterInHcd();
    phcdinfo->m_clustersInHcd.m_count = pcihcd->m_pClusterBatch->GetCountOfClustersInHcd();
    phcdinfo->m_clustersToProcess.m_start = pcihcd->m_pClusterBatch->GetStartingClusterToProcess();
    phcdinfo->m_clustersToProcess.m_count = pcihcd->m_pClusterBatch->GetCountOfClustersToProcess();
    /* nothing to do for members m_HcdsToSend and m_HcdBuffer 
     * as these addressess are given to gen info */
}


void FastSync::GenerateHcdStage::FillClusterFileDetails(HcdInfoToSend *phcdinfo, ClusterInfoForHcd *pcihcd)
{
    /* cluster file details */
    phcdinfo->m_pClusterFileInfo->m_fileName = pcihcd->m_pClusterBatch->GetFileName();
    phcdinfo->m_pClusterFileInfo->m_count = pcihcd->m_pClusterBatch->GetTotalCount();
    phcdinfo->m_pClusterFileInfo->m_size = pcihcd->m_pClusterBatch->GetSize();
    phcdinfo->m_pClusterFileInfo->m_volumeclusternumber = pcihcd->m_pClusterBatch->GetVolumeClusterNumber();
}


void FastSync::GenerateHcdStage::FillHcdBufferInGenInfo(HcdInfoToSend *phcdinfo)
{
    for (unsigned int i = 0; i < phcdinfo->m_HcdsToSend.m_NumHcdBuffers; i++)
    {
        phcdinfo->m_HcdsToSend.m_pHcdNodes[i].Reset();
        m_pHcdGenInfo[i].m_pHcdNodes = phcdinfo->m_HcdsToSend.m_pHcdNodes + i;
    }
}


void FastSync::GenerateHcdStage::GenerateHcd(const unsigned int &ngeninfos)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s with number of hcd gen infos = %u\n", FUNCTION_NAME, ngeninfos) ;

    QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::GenerateHcdStage::QuitRequested), this);
    for (unsigned int i = 1; i < ngeninfos; i++)
    {
        m_mesgBlks[i - 1]->base((char *) (m_pHcdGenInfo + i), sizeof (HcdGenInfo));
        if (!EnQ(m_hcdGenQ.get(), m_mesgBlks[i - 1], TaskWaitTimeInSeconds, qf))
        {
            throw DataProtectionException( "Enqueuing failed...\n" ) ;
        }
    }

    UpdateHCD(m_pHcdGenInfo, m_pHcdCalc);
    
    for (unsigned int i = 1; (i < ngeninfos) && (!QuitRequested(0)); i++)
    {
        while (!QuitRequested(0))
        {
            ACE_Message_Block* csdonemb = retrieveMessage( m_hcdAckQ ) ;
            if (csdonemb)
            {
                break;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void FastSync::GenerateHcdStage::UpdateHCD(HcdGenInfo *phcdgeninfo, Profiler &prf)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    SV_UINT clustersremaining = phcdgeninfo->m_clustersToProcess.m_count;
    SV_UINT nextcluster = phcdgeninfo->m_clustersToProcess.m_start;
    SV_UINT clustersize = phcdgeninfo->m_pClusterBatch->GetSize();
    SV_UINT continuousclusters;
    bool bisused;

    while (clustersremaining)
    {
        if (QuitRequested())
        {
            break;
        }
        continuousclusters = phcdgeninfo->m_pClusterBatch->GetContinuousClusters(nextcluster, clustersremaining, bisused);
        DebugPrintf(SV_LOG_DEBUG, "continuousclusters = %u from nextcluster = %u, clustersremaining = %u, is %s\n",
                                  continuousclusters, nextcluster, clustersremaining, bisused ? "used" : "unused");
        if (continuousclusters > clustersremaining)
        {
            continuousclusters = clustersremaining;
        }

        if (bisused)
        {
            prf->start();
            FormHcd(nextcluster, continuousclusters, phcdgeninfo);
            prf->stop(continuousclusters*clustersize);
            phcdgeninfo->m_pHcdNodes->m_UsedBytes += (continuousclusters*clustersize);
        }
        else
        {
            phcdgeninfo->m_pHcdNodes->m_UnUsedBytes += (continuousclusters*clustersize);
        }

        nextcluster += continuousclusters;
        clustersremaining -= continuousclusters;
    }

    /*
    if (!QuitRequested())
    {
        phcdgeninfo->Print();
    }
    */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void FastSync::GenerateHcdStage::FormHcd(const SV_UINT &startingcluster, const SV_UINT &count, HcdGenInfo *phcdgeninfo)
{
    SV_UINT relativestartingcluster = startingcluster - phcdgeninfo->m_clustersToProcess.m_start;
    HashCompareNode *phcdnode = reinterpret_cast<HashCompareNode *>(phcdgeninfo->m_pHcdNodes->m_pHcdData + phcdgeninfo->m_pHcdNodes->m_hcdDataLength);
    SV_ULONGLONG volumecluster = phcdgeninfo->m_pClusterBatch->GetVolumeClusterNumber() + startingcluster;
    phcdnode->m_Offset = volumecluster * phcdgeninfo->m_pClusterBatch->GetSize();
    phcdnode->m_Length = count * phcdgeninfo->m_pClusterBatch->GetSize();
    char *voldata = phcdgeninfo->m_volumeData.m_pData + (relativestartingcluster * phcdgeninfo->m_pClusterBatch->GetSize());

    /*
    DebugPrintf(SV_LOG_DEBUG, "phcdnode->m_Offset = " LLSPEC "\n", phcdnode->m_Offset);
    DebugPrintf(SV_LOG_DEBUG, "phcdnode->m_Length = %u\n", phcdnode->m_Length);
    DebugPrintf(SV_LOG_DEBUG, "phcdgeninfo->m_pHcdNodes->m_hcdDataLength = %u\n", phcdgeninfo->m_pHcdNodes->m_hcdDataLength);
    DebugPrintf(SV_LOG_DEBUG, "phcdgeninfo->m_pClusterBatch->GetSize() = %u\n", phcdgeninfo->m_pClusterBatch->GetSize());
    */

    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char *)voldata, phcdnode->m_Length);
    INM_MD5Final(phcdnode->m_Hash, &ctx);

    phcdgeninfo->m_pHcdNodes->m_hcdDataLength += sizeof(HashCompareNode);
}


ACE_Message_Block * FastSync::GenerateHcdStage::retrieveMessage( ACE_Shared_MQ& sharedQueue )
{
    return m_FastSync->retrieveMessage(sharedQueue);
}


void FastSync::GenerateHcdStage::Idle(int seconds)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    QuitRequested(seconds);
}


bool FastSync::GenerateHcdStage::AllocateHcdBufferIfReq(ClusterInfoForHcd *pcihcd)
{
    bool balloced = m_HcdInfosToSend.m_bAllocated;

    if (!balloced)
    {
        for (unsigned int i = 0; i < m_HcdInfosToSend.m_NumHcdInfos; i++)
        {
            balloced = m_HcdInfosToSend.m_pHcdInfos[i].AllocateHcdBuffer(m_FastSync->getReadBufferSize(), 
                                                                         pcihcd->m_pClusterBatch->GetSize());
            if (!balloced)
            {
                break;
            }
        }
        m_HcdInfosToSend.m_bAllocated = balloced;
    }

    return balloced;
}


void FastSync::GenerateHcdStage::SendBackClusterInfo(ClusterInfoForHcd *pcihcd)
{
    SendBackClusterInfo_t p = m_sendBackClusterInfo[m_FastSync->IsProcessClusterPipeEnabled()];
    (this->*p)(pcihcd);
}


void FastSync::GenerateHcdStage::SendBackClusterInfoInStage(ClusterInfoForHcd *pcihcd)
{
    /* TODO: find out what is the size for in message block ? will 
     *       it cause any allocations and so on ? */
    if (!WriteToBackStage(pcihcd, sizeof(ClusterInfoForHcd), TaskWaitTimeInSeconds))
    {
        std::stringstream excmsg;
        excmsg << "For volume " << m_FastSync->GetDeviceName()
               << ", sending back processed cluster info to process cluster stage failed\n";
        throw DataProtectionException(excmsg.str());
    }
}


void FastSync::GenerateHcdStage::SendBackClusterInfoInApp(ClusterInfoForHcd *pcihcd)
{
    /* nothing to do */
}


void FastSync::GenerateHcdStage::InformStatus(const std::string &s)
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

bool FastSync::GenerateHcdStage::QuitRequested(long int seconds)
{
    QuitRequested_t p = m_quitRequest[m_FastSync->IsProcessClusterPipeEnabled()];
    return (this->*p)(seconds);
}


bool FastSync::GenerateHcdStage::QuitRequestedInStage(long int seconds)
{
    return ShouldQuit(seconds);
}


bool FastSync::GenerateHcdStage::QuitRequestedInApp(long int seconds)
{
    return m_FastSync->QuitRequested(seconds);
}


HcdInfoToSend * FastSync::GenerateHcdStage::GetHcdInfoToSend(void)
{
    GetHcdInfoToSend_t p = m_hcdInfoToSend[m_FastSync->IsProcessClusterPipeEnabled()];
    return (this->*p)();
}


HcdInfoToSend * FastSync::GenerateHcdStage::GetHcdInfoToSendInStage(void)
{
    HcdInfoToSend *p = 0;
    if (m_HcdInfosToSend.m_FreeIndex < m_HcdInfosToSend.m_NumHcdInfos)
    {
        p = m_HcdInfosToSend.m_pHcdInfos + m_HcdInfosToSend.m_FreeIndex;
        m_HcdInfosToSend.m_FreeIndex++;
    }
    else
    {
        void *pv = ReadFromForwardStage(TaskWaitTimeInSeconds);
        p = (HcdInfoToSend *)pv;
    }

    return p;
}


HcdInfoToSend * FastSync::GenerateHcdStage::GetHcdInfoToSendInApp(void)
{
    return m_HcdInfosToSend.m_pHcdInfos;
}


HcdInfosToSend::~HcdInfosToSend()
{
    Release();
    m_NumHcdInfos = 0;
}


void HcdInfosToSend::Release(void)
{
    if (m_pHcdInfos)
    {
        delete [] m_pHcdInfos;
        m_pHcdInfos = 0;
    }
}


bool HcdInfosToSend::Init(const unsigned int &nhcdinfos, const unsigned int &ngenhcdthreads)
{
    bool binitialized = false;

    Release();
    m_pHcdInfos = new (std::nothrow) HcdInfoToSend[nhcdinfos];
    if (m_pHcdInfos)
    {
        for (unsigned int i = 0; i < nhcdinfos; i++)
        {
            binitialized = m_pHcdInfos[i].Init(ngenhcdthreads);
            if (!binitialized)
            {
                DebugPrintf(SV_LOG_ERROR, "could not initialize hcd info for send\n");
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate hcd info for send\n");
    }

    if (binitialized)
    {
        m_NumHcdInfos = nhcdinfos;
    }

    return binitialized;
}


bool HcdInfoToSend::Init(const unsigned int &ngenhcdthreads)
{
    ReleaseClusterInfo();
    m_pClusterFileInfo = new (std::nothrow) ClusterFileInfo;
    return m_pClusterFileInfo && m_HcdsToSend.Init(ngenhcdthreads);
}


void HcdInfoToSend::ReleaseClusterInfo(void)
{
    if (m_pClusterFileInfo)
    {
        delete m_pClusterFileInfo;
        m_pClusterFileInfo = 0;
    }
}


bool HcdsToSend::Init(const unsigned int &ngenhcdthreads)
{
    Release();
    m_pHcdNodes = new (std::nothrow) HcdNodes[ngenhcdthreads];
    if (m_pHcdNodes)
    {
        m_NumHcdBuffers = ngenhcdthreads;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate slots for hcds to send\n");
    }

    return m_pHcdNodes;
}


void HcdsToSend::Release(void)
{
    if (m_pHcdNodes)
    {
        delete [] m_pHcdNodes;
        m_pHcdNodes = 0;
    }
}


HcdsToSend::~HcdsToSend()
{
    Release();
}


bool HcdInfoToSend::AllocateHcdBuffer(const SV_UINT &readbuffersize, const SV_UINT &clustersize)
{
    ReleaseHcdBuffer();
    SV_UINT nclustersinreadbuffer;
    INM_SAFE_ARITHMETIC(nclustersinreadbuffer = InmSafeInt<SV_UINT>::Type(readbuffersize) / clustersize, INMAGE_EX(readbuffersize)(clustersize))
    /* worst case is no clubbing of continuous clusters */
    SV_UINT nhcdsinworstcase;
    INM_SAFE_ARITHMETIC(nhcdsinworstcase = InmSafeInt<SV_UINT>::Type(nclustersinreadbuffer) / 2, INMAGE_EX(nclustersinreadbuffer))
    SV_UINT length;
    INM_SAFE_ARITHMETIC(length = nhcdsinworstcase * InmSafeInt<size_t>::Type(sizeof(struct HashCompareNode)), INMAGE_EX(nhcdsinworstcase)(sizeof(struct HashCompareNode)))

    m_HcdBuffer.m_pData = new (std::nothrow) char[length];
    if (m_HcdBuffer.m_pData)
    {
        m_HcdBuffer.m_length = length;
        /*
        DebugPrintf(SV_LOG_DEBUG, "allocated hcd buffer:\n");
        m_HcdBuffer.Print();
        */
        GiveBufferToHcdNodes(nhcdsinworstcase);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "failed to allocate %u bytes for hcd buffer\n", length);
    }

    return m_HcdBuffer.m_pData;
}


void HcdInfoToSend::GiveBufferToHcdNodes(const SV_UINT &nhcdsinworstcase)
{
    unsigned int nhcdsremaining = nhcdsinworstcase;
    unsigned int nhcdsperthread = (nhcdsremaining / m_HcdsToSend.m_NumHcdBuffers) +
                                  ((nhcdsremaining % m_HcdsToSend.m_NumHcdBuffers) ? 1 : 0);
    unsigned int count = 0;
    char *phcddata = m_HcdBuffer.m_pData;

    int i = 0;
    while (nhcdsremaining)
    {
        if (nhcdsremaining > nhcdsperthread)
        {
            count = nhcdsperthread;
            if ((nhcdsremaining - count) < nhcdsperthread)
            {
                count = nhcdsremaining;
            }
        }
        else
        {
            count = nhcdsremaining;
        }
        m_HcdsToSend.m_pHcdNodes[i].m_pHcdData = phcddata;
        m_HcdsToSend.m_pHcdNodes[i].m_allocatedLength = (count*sizeof(struct HashCompareNode));
        DebugPrintf(SV_LOG_DEBUG, "for %d, hcd nodes data: %p, allocated length: %u, allocated hcds: %u\n", 
                                  i, m_HcdsToSend.m_pHcdNodes[i].m_pHcdData, 
                                  m_HcdsToSend.m_pHcdNodes[i].m_allocatedLength, count);
        phcddata += m_HcdsToSend.m_pHcdNodes[i].m_allocatedLength;
        nhcdsremaining -= count;
        i++;
    }
}


HcdInfoToSend::~HcdInfoToSend()
{
    ReleaseHcdBuffer();
    m_HcdBuffer.m_length = 0;
    ReleaseClusterInfo();
}


void HcdInfoToSend::ReleaseHcdBuffer(void)
{
    if (m_HcdBuffer.m_pData)
    {
        delete [] m_HcdBuffer.m_pData;
        m_HcdBuffer.m_pData = 0;
    }
}


int FastSync::HCDComputer::open( void *args )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	return this->activate(THR_BOUND);
}


int FastSync::HCDComputer::close( SV_ULONG flags )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	return 0 ;
}


int FastSync::HCDComputer::svc()
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::GenerateHcdStage::QuitRequested), m_pGenerateHcdStage);
    while (!m_pGenerateHcdStage->QuitRequested())
    {
		ACE_Message_Block* mbret = m_pGenerateHcdStage->retrieveMessage( m_hcdGenQ  ) ;
		if( mbret )
		{
			HcdGenInfo *phcdgeninfo = (HcdGenInfo *) mbret->base() ;

            m_pGenerateHcdStage->UpdateHCD(phcdgeninfo, m_pHcdCalc);

			if (!EnQ(m_hcdAckQ.get(), mbret, TaskWaitTimeInSeconds, qf))
			{
                throw DataProtectionException( "Enqueuing failed...\n" ) ;
			}
		}
         
    }
    m_pHcdCalc->commit();

	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return 0;
}


void HcdGenInfo::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== hcd generation info: start =====\n");
    m_pClusterBatch->Print();
    m_clustersToProcess.Print();
    DebugPrintf(SV_LOG_DEBUG, "volume data:\n");
    m_volumeData.Print();
    DebugPrintf(SV_LOG_DEBUG, "hcd nodes:\n");
    m_pHcdNodes->Print();
    DebugPrintf(SV_LOG_DEBUG, " ===== hcd generation info: end =====\n");
}


void HcdNodes::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== hcd nodes: start =====\n");
    DebugPrintf(SV_LOG_DEBUG, "hcd data: %p\n", m_pHcdData);
    DebugPrintf(SV_LOG_DEBUG, "allocated length: %u\n", m_allocatedLength);
    DebugPrintf(SV_LOG_DEBUG, "hcd data length: %u\n", m_hcdDataLength);
    DebugPrintf(SV_LOG_DEBUG, "pattern used bytes: %lu\n", m_UsedBytes);
    DebugPrintf(SV_LOG_DEBUG, "pattern un used bytes: %lu\n", m_UnUsedBytes);
    DebugPrintf(SV_LOG_DEBUG, " ===== hcd nodes: end =====\n");
}


void HcdInfoToSend::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "===== hcd info to send: start =====\n");
    m_pClusterFileInfo->Print();
    m_clustersInHcd.Print();
    m_clustersToProcess.Print();
    m_HcdsToSend.Print();
    m_HcdBuffer.Print();
    DebugPrintf(SV_LOG_DEBUG, "===== hcd info to send: end =====\n");
}


void ClusterFileInfo::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "cluster file info:\n");
    DebugPrintf(SV_LOG_DEBUG, "filename: %s\n", m_fileName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "count of clusters: %u\n", m_count);
    DebugPrintf(SV_LOG_DEBUG, "cluster size: %u\n", m_size);
    DebugPrintf(SV_LOG_DEBUG, "volume cluster number: " ULLSPEC "\n", m_volumeclusternumber);
}


void HcdsToSend::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "hcds to send:\n");
    DebugPrintf(SV_LOG_DEBUG, "number of hcd data buffers: %u\n", m_NumHcdBuffers);
    for (SV_UINT i = 0; i < m_NumHcdBuffers; i++)
    { 
        m_pHcdNodes[i].Print();
    }
}

