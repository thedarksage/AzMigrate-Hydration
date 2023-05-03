#include "fastsync.h"
#include "theconfigurator.h"
#include "dataprotectionutils.h"
#include "hcdgen.h"
#include "hashcomparedata.h"
#include "configurevxagent.h"


FastSync::SendHcdStage::SendHcdStage(FastSync* fastSync, ProcessClusterStage *processClusterStage)
: m_FastSync(fastSync),
  m_pProcessClusterStage(processClusterStage),
  m_cxTransport(new CxTransport(m_FastSync->Protocol(), m_FastSync->TransportSettings(), m_FastSync->Secure(), m_FastSync->getCfsPsData().m_useCfs, m_FastSync->getCfsPsData().m_psId)),
  m_UsedBytes(0),
  m_UnUsedBytes(0),
  m_HashCompareData(0, m_FastSync->getChunkSize(), m_FastSync->GetHashCompareDataSize(), HashCompareData::MD5, m_FastSync->GetEndianTagSize()),
  m_eHcdFileHeaderStatus(E_NEEDTOSEND),
  m_pHcdUpload(NEWPROFILER(HCDUPLOAD, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), true))
{ 
    m_quitRequest[0] = &FastSync::SendHcdStage::QuitRequestedInApp;
    m_quitRequest[1] = &FastSync::SendHcdStage::QuitRequestedInStage;

    m_sendBackHcdInfo[0] = &FastSync::SendHcdStage::SendBackHcdInfoInApp;
    m_sendBackHcdInfo[1] = &FastSync::SendHcdStage::SendBackHcdInfoInStage;
}


int FastSync::SendHcdStage::svc()
{  
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

	try
	{
        do
        {
            void *pv = ReadFromBackStage(TaskWaitTimeInSeconds);
            if (pv)
            {
                HcdInfoToSend *phcdinfo = (HcdInfoToSend *)pv;
                process(phcdinfo);
            }
            else
            {
                m_cxTransport->heartbeat();
            }
           
        } while( !QuitRequested( 0 ) );
        m_pHcdUpload->commit();  // Commit if anything is pending
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

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0 ;
}


void FastSync::SendHcdStage::process(HcdInfoToSend *phcdinfo)
{
    DebugPrintf(SV_LOG_DEBUG, "got hcd info to send\n");
    phcdinfo->Print();
    
    std::stringstream excmsg;
    excmsg << "For volume " << m_FastSync->GetDeviceName() << ", ";

    if (phcdinfo->m_clustersInHcd.m_start == phcdinfo->m_clustersToProcess.m_start)
    {
        /* TODO: should this be here ? Also test throttling cases */
        ThrottleSpace();
        m_FastSync->nextHcdFileNames( m_PreFileName, m_FileName ) ;
        if (m_FastSync->IsTransportProtocolAzureBlob())
        {
            m_PreFileName = m_FileName;
        }
		DebugPrintf(SV_LOG_DEBUG, "hcd pre file name: %s, file name: %s\n", m_PreFileName.c_str(), m_FileName.c_str());
        m_UsedBytes = m_UnUsedBytes = 0;
    }
    
    SendHcdWithHeaderIfReq(phcdinfo);

    /* start + count in hcd == start + count in process */
    if ((phcdinfo->m_clustersInHcd.m_start + phcdinfo->m_clustersInHcd.m_count) ==
        (phcdinfo->m_clustersToProcess.m_start + phcdinfo->m_clustersToProcess.m_count))
    {
        SendHcdTrailerIfReq(phcdinfo);
        /* TODO: all kinds of error checks */
        SV_ULONG totalbytesinhcd = phcdinfo->m_clustersInHcd.m_count * phcdinfo->m_pClusterFileInfo->m_size;
        SV_ULONGLONG clusternumber = phcdinfo->m_pClusterFileInfo->m_volumeclusternumber;
        clusternumber += phcdinfo->m_clustersInHcd.m_start;
        SV_LONGLONG volumeoffset = clusternumber * phcdinfo->m_pClusterFileInfo->m_size;
        if (m_UsedBytes == totalbytesinhcd)
        {
            m_pProcessClusterStage->markAsFullyUsed(volumeoffset);
        }
        else if (m_UnUsedBytes == totalbytesinhcd)
        {
            m_pProcessClusterStage->markAsFullyUnUsed(volumeoffset, m_UnUsedBytes);
        }
        else
        {
            m_pProcessClusterStage->markAsPartiallyUsed(volumeoffset);
        }
        m_eHcdFileHeaderStatus = E_NEEDTOSEND;
    }

    /* count in file info == start + count in process */
    if (phcdinfo->m_pClusterFileInfo->m_count == 
        (phcdinfo->m_clustersToProcess.m_start + phcdinfo->m_clustersToProcess.m_count))
    {
		if( !m_cxTransport->deleteFile(phcdinfo->m_pClusterFileInfo->m_fileName))
		{
			excmsg << "removal of "
			       << phcdinfo->m_pClusterFileInfo->m_fileName
			       << " failed: "
			       << m_cxTransport->status()
			       << "\n";
			throw DataProtectionException(excmsg.str());
		}
        m_pProcessClusterStage->DecrementPendingCount();
    }

    SendBackHcdInfo(phcdinfo);
}


bool FastSync::SendHcdStage::QuitRequested(long int seconds)
{
    QuitRequested_t p = m_quitRequest[m_FastSync->IsProcessClusterPipeEnabled()];
    return (this->*p)(seconds);
}


bool FastSync::SendHcdStage::QuitRequestedInStage(long int seconds)
{
    return ShouldQuit(seconds);
}


bool FastSync::SendHcdStage::QuitRequestedInApp(long int seconds)
{
    return m_FastSync->QuitRequested(seconds);
}


void FastSync::SendHcdStage::SendBackHcdInfo(HcdInfoToSend *phcdinfo)
{
    SendBackHcdInfo_t p = m_sendBackHcdInfo[m_FastSync->IsProcessClusterPipeEnabled()];
    (this->*p)(phcdinfo);
}


void FastSync::SendHcdStage::SendBackHcdInfoInStage(HcdInfoToSend *phcdinfo)
{
    /* TODO: find out what is the size for in message block ? will 
     *       it cause any allocations and so on ? */
    if (!WriteToBackStage(phcdinfo, sizeof(HcdInfoToSend), TaskWaitTimeInSeconds))
    {
        std::stringstream excmsg;
        excmsg << "For volume " << m_FastSync->GetDeviceName()
               << ", sending back hcd info to generate stage after send failed\n";
        throw DataProtectionException(excmsg.str());
    }
}


void FastSync::SendHcdStage::SendBackHcdInfoInApp(HcdInfoToSend *phcdinfo)
{
    /* nothing to do */
}


void FastSync::SendHcdStage::InformStatus(const std::string &s)
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


void FastSync::SendHcdStage::SendHcdWithHeaderIfReq(HcdInfoToSend *phcdinfo)
{
    bool bneedtosend = false;

    int length;
    for (unsigned int i = 0; i < phcdinfo->m_HcdsToSend.m_NumHcdBuffers; i++)
    {
        length = phcdinfo->m_HcdsToSend.m_pHcdNodes[i].m_hcdDataLength;
        if (length)
        {
            bneedtosend = true;
        }
        m_UsedBytes += phcdinfo->m_HcdsToSend.m_pHcdNodes[i].m_UsedBytes;
        m_UnUsedBytes += phcdinfo->m_HcdsToSend.m_pHcdNodes[i].m_UnUsedBytes;
    }
    
    if (bneedtosend)
    {
        std::stringstream excmsg;
        if (E_NEEDTOSEND == m_eHcdFileHeaderStatus)
        {
			DebugPrintf(SV_LOG_DEBUG, "sending hcd header for pre file name %s, with length %d\n", m_PreFileName.c_str(), m_HashCompareData.GetHeaderLength());
            /* send header */
            if (!m_FastSync->SendData(m_cxTransport, m_PreFileName, m_HashCompareData.Data(), m_HashCompareData.GetHeaderLength(), 
                          CX_TRANSPORT_MORE_DATA, COMPRESS_NONE))
            {
                excmsg << "failed to send header for hcd file " << m_PreFileName << '\n';
                throw DataProtectionException(excmsg.str());
            }
            m_eHcdFileHeaderStatus = E_SENT;
        }

        const char *p;
        for (unsigned int i = 0; i < phcdinfo->m_HcdsToSend.m_NumHcdBuffers; i++)
        {
            p = phcdinfo->m_HcdsToSend.m_pHcdNodes[i].m_pHcdData;
            length = phcdinfo->m_HcdsToSend.m_pHcdNodes[i].m_hcdDataLength;

            /* Added because all hcd infos may not have been filled
             * because of early finish. This is possible because we
             * do Reset in hcdgen */
            if (0 == length)
            {
                continue;
            }

			DebugPrintf(SV_LOG_DEBUG, "sending hcd payload for pre file name %s, with length %d\n", m_PreFileName.c_str(), length);
            m_pHcdUpload->start();
            if (!m_FastSync->SendData(m_cxTransport, m_PreFileName, p, length,
                          CX_TRANSPORT_MORE_DATA, COMPRESS_NONE))
            {
                excmsg << "failed to send payload for hcd file " << m_PreFileName << '\n';
                throw DataProtectionException(excmsg.str());
            }
            m_pHcdUpload->stop(length);
        }
    }
}


void FastSync::SendHcdStage::SendHcdTrailerIfReq(HcdInfoToSend *phcdinfo)
{
    if (E_SENT == m_eHcdFileHeaderStatus)
    {
        std::stringstream excmsg;
        const char *p = m_HashCompareData.Data() + m_HashCompareData.GetHeaderLength();
        int length = m_HashCompareData.DataLength() - m_HashCompareData.GetHeaderLength();
		DebugPrintf(SV_LOG_DEBUG, "sending hcd trailer for pre file name %s, with length %d\n", m_PreFileName.c_str(), length);
        if (!m_FastSync->SendData(m_cxTransport, m_PreFileName, p, length, 
                      CX_TRANSPORT_NO_MORE_DATA, COMPRESS_NONE))
        {
            excmsg << "failed to send trailer for hcd file " << m_PreFileName << '\n';
            throw DataProtectionException(excmsg.str());
        }

		DebugPrintf(SV_LOG_DEBUG, "renaming from hcd pre file name: %s, to file name: %s\n", m_PreFileName.c_str(), m_FileName.c_str());
        boost::tribool tb = m_cxTransport->renameFile(m_PreFileName, m_FileName, COMPRESS_NONE);
        if( !tb || CxTransportNotFound(tb) )
        {
            /* TODO: should we be removing the hcd file if rename fails ?
             *       removal code is not present earlier too */
            excmsg << "FAILED FastSync::Rename Failed From "<<m_PreFileName<<" To "<< m_FileName << ": " << m_cxTransport->status() << '\n';
            throw DataProtectionException(excmsg.str());
        }
    }
}


void FastSync::SendHcdStage::ThrottleSpace(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (IsRcmMode)
    {
        return;
    }

    do
    {
        bool throttle = m_FastSync->Source() ? TheConfigurator->getVxAgent().shouldThrottleResync( m_FastSync->GetDeviceName(), m_FastSync->Settings().endpoints.begin()->deviceName, 
                                                                                       m_FastSync->GetGrpId() ) :
        TheConfigurator->getVxAgent().shouldThrottleTarget( m_FastSync->GetDeviceName() );
        if( !throttle )
        {
            break;
        }

        if (m_FastSync->Source())
        {
            DebugPrintf(SV_LOG_ERROR, "Throttling is set for deviceName %s and endpointdevicename %s grpid %d\n",
                        m_FastSync->GetDeviceName().c_str(), m_FastSync->Settings().endpoints.begin()->deviceName.c_str(), 
                        m_FastSync->GetGrpId());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Throttling is set for deviceName %s\n",
                        m_FastSync->GetDeviceName().c_str());
        }
        if(QuitRequested(30))
            break;
        /* TODO: heartbeat here ? */
        m_cxTransport->heartbeat();

    } while( true );
}
