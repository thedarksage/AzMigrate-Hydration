#include "fastsync.h"
#include "theconfigurator.h"
#include "configurevxagent.h"


FastSync::ClusterParams::ClusterParams(FastSync *fastSync)
              :m_FastSync(fastSync)
{
	m_clusterSyncChunkSize = m_FastSync->GetConfigurator().getLengthForFileSystemClustersQuery();
}


SV_ULONG FastSync::ClusterParams::totalChunks(void)
{
	SV_ULONG rval = (SV_ULONG)(( m_FastSync->getVolumeSize()/ (SV_LONGLONG) m_clusterSyncChunkSize) + ( m_FastSync->getVolumeSize() % (SV_LONGLONG)m_clusterSyncChunkSize ? 1: 0));
	if (0 == rval)
	{
	    std::stringstream msg;
	    msg << "volume size = " << m_FastSync->getVolumeSize()
		    << ", m_clusterSyncChunkSize = " << m_clusterSyncChunkSize;
        DebugPrintf(SV_LOG_ERROR, "In function total cluster Chunks, with %s, the total chunks calculated are zero\n", msg.str().c_str());
	}
	/* DebugPrintf(SV_LOG_DEBUG, "In function total cluster Chunks, with %s, total chunks are %lu\n", msg.str().c_str(), rval); */

	return rval;
}


bool FastSync::ClusterParams::AreValid(std::ostream &msg)
{
    bool brvalid = true;

    if (0 == m_clusterSyncChunkSize)
    {
        msg << "cluster sync chunk size is zero";
        brvalid = false;
    }
    
    /* This is needed because a cluster bitmap file should
     * give a complete hcd file; Not an incomplete one */
    if (m_clusterSyncChunkSize % m_FastSync->getChunkSize())
    {
        msg << "cluster sync chunk size: " << m_clusterSyncChunkSize
            << " is not exact multiple of "
            << "fast sync chunk size: " << m_FastSync->getChunkSize();
        brvalid = false;
    }

    return brvalid;
}


bool FastSync::ClusterParams::initSharedBitMapFile(boost::shared_ptr<SharedBitMap>&  bitmap, 
                                                   const string& fileName, CxTransport::ptr &cxTransport,
                                                   const bool & bShouldCreateIfNotPresent)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	int chunkSize = getClusterSyncChunkSize();
	SV_ULONG bitMapSize = totalChunks();

	DebugPrintf(SV_LOG_DEBUG, "For file %s, bitmap size locally calculated is %lu\n", fileName.c_str(), bitMapSize);
	if (0 == bitMapSize)
	{
		DebugPrintf(SV_LOG_ERROR, "For file %s, totalChunks returned bitmap size zero\n", fileName.c_str());
	}

    bitmap.reset( new SharedBitMap(fileName, bitMapSize, m_FastSync->IsTransportProtocolAzureBlob(), chunkSize, m_FastSync->Secure() ) ) ;
	if( bitmap->initialize(cxTransport, bShouldCreateIfNotPresent) != SVS_OK )
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to initialize bitmap file %s\n", fileName.c_str());
		return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return true;
}


bool FastSync::ClusterParams::IsClusterTransferMapValid(SharedBitMapPtr &unprocessedClusterMap, 
							                            std::ostringstream &msg)
{
    bool bisvalid = true;

    if (m_clusterSyncChunkSize != unprocessedClusterMap->getBitMapGranualarity())
    {
        msg << "configured cluster sync chunk size: " << m_clusterSyncChunkSize
            << ", unprocessed cluster map granularity: " << unprocessedClusterMap->getBitMapGranualarity()
            << " do not match\n";
        bisvalid = false;
    }

    return bisvalid;
}

