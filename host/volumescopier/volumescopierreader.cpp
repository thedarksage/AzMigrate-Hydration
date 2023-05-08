#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <functional>

#include "ace/ACE.h"

#include "volumescopierreader.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "inmageex.h"
#include "inmsafeint.h"
#include "inmageex.h"

#ifdef SV_UNIX
#include "portablehelpersminor.h"
#endif

VolumesCopierReaderStage::VolumesCopierReaderStage(const CopyInfo &rci)
: InmPipeline::Stage(rci.m_Profile),
  m_CopyInfo(rci),
  m_StartOffset(0),
  m_EndOffset(0),
  m_PartID(0),
  m_pSourceVolume(0),
  m_ClusterSize(0),
  m_pVcis(0),
  m_pCurrentVci(0),
  m_TimeForRead(0),
  m_pVolumesCopierWriterStage(0)
{
    m_QuitRequest[0] = &VolumesCopierReaderStage::QuitRequestedInApp;
    m_QuitRequest[1] = &VolumesCopierReaderStage::QuitRequestedInStage;

    m_RWBufGetter[0] = &VolumesCopierReaderStage::GetReadWriteBufferInApp;
    m_RWBufGetter[1] = &VolumesCopierReaderStage::GetReadWriteBufferInStage;

    m_RWBufSender[0] = &VolumesCopierReaderStage::SendReadWriteBufferInApp;
    m_RWBufSender[1] = &VolumesCopierReaderStage::SendReadWriteBufferInStage;

	m_Read[0] = &VolumesCopierReaderStage::NoProfileRead;
	m_Read[1] = &VolumesCopierReaderStage::ProfileRead;

    m_SetCurrentVci[0] = &VolumesCopierReaderStage::SetCurrentVciInApp;
    m_SetCurrentVci[1] = &VolumesCopierReaderStage::SetCurrentVciInStage;
}


bool VolumesCopierReaderStage::Init(std::ostream &excmsg)
{
    bool binit = true;

    if (!m_CopyInfo.IsValid(excmsg))
    {
        binit = false;
    }

    /* TODO: add error message as to why the instantiation failed ? */
    if (binit && 
        ((m_pSourceVolume = m_CopyInfo.m_VolumeInstantiator(GetRawVolumeName(m_CopyInfo.m_SourceVolumeName))) == 0))
    {
        excmsg << "Failed to instantiate source volume " << m_CopyInfo.m_SourceVolumeName;
        binit = false;
    }

    m_pSourceVolume->Open(BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioDirect);
    if (!m_pSourceVolume->Good()) 
    {
        excmsg << "failed to open source volume :" 
               << m_pSourceVolume->GetName()<< " error: " << m_pSourceVolume->LastError();
        binit = false;
    }

    std::string errmsg;
    if (binit &&
        ((m_ClusterSize = m_CdpVolumeUtil.GetClusterSize(*m_pSourceVolume, m_CopyInfo.m_Filesystem, m_CopyInfo.m_SourceSize,
                                                         m_CopyInfo.m_SourceStartOffset, m_CopyInfo.m_FsSupportAction, errmsg)) == 0))
    {
        excmsg << "Failed to get cluster size for volume " << m_pSourceVolume->GetName() << " with error: " << errmsg;
        binit = false;
    }

    if (binit)
    {
		SV_UINT nbufs = m_CopyInfo.m_PipelineReadWrite ? m_CopyInfo.m_ReadBufferCount : 1;
        SV_UINT quotient = m_CopyInfo.m_ReadBufferSize / m_ClusterSize;
        if ((quotient*m_ClusterSize) != m_CopyInfo.m_ReadBufferSize)
        {
            /* TODO: should this be WARNING ? */
            DebugPrintf(SV_LOG_ERROR, "For source %s, read buffer size (%u) is not multiple of cluster size. "
                                      " The cluster size is %u. Moving a head by making it as nearest multiple upwards\n", 
                                      m_CopyInfo.m_SourceVolumeName.c_str(), m_CopyInfo.m_ReadBufferSize, m_ClusterSize);
            /* NOTE: going upwards takes care of m_CopyInfo.m_ReadBufferSize being
             * lesser than m_ClusterSize */
            m_CopyInfo.m_ReadBufferSize = (m_ClusterSize * (quotient+1));
        }

        /* make number of cluster to query multiple of read buffer size */
        quotient = m_CopyInfo.m_LengthForFileSystemClustersQuery / m_CopyInfo.m_ReadBufferSize;
        if ((quotient*m_CopyInfo.m_ReadBufferSize) != m_CopyInfo.m_LengthForFileSystemClustersQuery)
        {
            /* TODO: should this be WARNING ? */
            DebugPrintf(SV_LOG_ERROR, "For source %s, number of clusters to query (%u) is not multiple of read buffer size %u. "
                                      "Moving a head by making it as nearest multiple upwards\n", 
                                      m_CopyInfo.m_SourceVolumeName.c_str(), m_CopyInfo.m_LengthForFileSystemClustersQuery, 
									  m_CopyInfo.m_ReadBufferSize);
			/* NOTE: going upwards takes care of m_LengthForFileSystemClustersQuery being
             * lesser than m_CopyInfo.m_ReadBufferSize */
            m_CopyInfo.m_LengthForFileSystemClustersQuery = (m_CopyInfo.m_ReadBufferSize * (quotient+1));
        }

		if (m_CopyInfo.m_PipelineReadWrite)
		{
			/* As there are two query buffers, each should be 
			 * able to hold, at minimum, maximum outstanding 
			 * number of buffers in pipe, so that reader 
			 * can change to next query buffer with out 
			 * worrying about this query buffer being in use
			 * by the writer stage */
			SV_UINT nbufsoutstanding = nbufs-1;
			SV_UINT minquerylength = nbufsoutstanding*m_CopyInfo.m_ReadBufferSize;
			if (m_CopyInfo.m_LengthForFileSystemClustersQuery < minquerylength)
			{
				m_CopyInfo.m_LengthForFileSystemClustersQuery = minquerylength;
			}
		}

        /* TODO: For unix platforms, alignment has to be on 
         * page boundary. What about windows ? 
         * Belief is that regardless of platforms, 
         * alignment should be based on disk physical sector size. */
		if (!m_RWBufs.Init(nbufs, m_CopyInfo.m_ReadBufferSize, m_CopyInfo.m_ReadBufferAlignment))
        {
            excmsg << "Could not initialize read write buffer" << (m_CopyInfo.m_PipelineReadWrite ? "s" : "") 
                   << ", when read buffer size is " << m_CopyInfo.m_ReadBufferSize;
            binit = false;
        }
    }

	/* always allocate only two volume cluster infos in case of pipeline 
	 * as length to query file system is made such that it accomodates 
	 * any number of io buffers */
    if (binit && 
        !AllocateVolumeClusterInfos(m_CopyInfo.m_PipelineReadWrite ? 2 : 1))
    {
        excmsg << "Failed to initialize volume cluster info" << (m_CopyInfo.m_PipelineReadWrite ? "s" : "");
        binit = false;
    }

    return binit;
}


VolumesCopierReaderStage::~VolumesCopierReaderStage(void)
{
    if (m_pSourceVolume)
    {
        m_CopyInfo.m_VolumeDestroyer(m_pSourceVolume);
        m_pSourceVolume = 0;
    }

    ReleaseVolumeClusterInfos();
}


void VolumesCopierReaderStage::ReleaseVolumeClusterInfos(void)
{
    if (m_pVcis)
    {
        delete [] m_pVcis;
        m_pVcis = 0;
    }
    m_pCurrentVci = 0;
}


bool VolumesCopierReaderStage::AllocateVolumeClusterInfos(const SV_UINT &nvcis)
{
    m_pVcis = new (std::nothrow) VolumeClusterInformer::VolumeClusterInfo[nvcis];
    m_pCurrentVci = m_pVcis;
    return m_pVcis ? true : false;
}


void VolumesCopierReaderStage::SetWriter(VolumesCopierWriterStage *pwriter)
{
    m_pVolumesCopierWriterStage = pwriter;
}


int VolumesCopierReaderStage::svc(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

	try
	{
        std::stringstream excmsg;
        if (!Init(excmsg))
        {
		    throw INMAGE_EX(excmsg.str());
        }

        VolumesCopier::ChkptInfos_t &chs = *m_CopyInfo.m_pChkptInfos;
        for (VolumesCopier::ChkptInfos_t::size_type index = 0; ((index < chs.size()) && (!QuitRequested())); ++index)
        {
            if(chs[index].endOffset == chs[index].lastWroteOffset)
                continue;

            m_StartOffset = chs[index].startOffset;
            m_EndOffset = chs[index].endOffset;
            m_PartID = index;
            DebugPrintf(SV_LOG_DEBUG, "copying from start offset " ULLSPEC
                                      " to end offset " ULLSPEC
                                      ", partition ID %u\n", m_StartOffset,
                                        m_EndOffset, m_PartID);
            /* throws INMAGE_EX */
            Copy();
        }
	}
	catch (ContextualException& ce)
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", ce.what());
        InformStatus(ce.what());
	}
	catch (std::exception& e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", e.what());
        InformStatus(e.what());
	}

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0;
}


void VolumesCopierReaderStage::Copy(void)
{
    SV_ULONGLONG cbRemaining;
    INM_SAFE_ARITHMETIC(cbRemaining = InmSafeInt<SV_ULONGLONG>::Type(m_EndOffset)-m_StartOffset, INMAGE_EX(m_EndOffset)(m_StartOffset))
    SV_UINT cbToRead = 0;
    SV_UINT cbread = 0;
    std::stringstream msg;

	DebugPrintf(SV_LOG_DEBUG,"In reader Thread For partition %u, Start offset " ULLSPEC " And EndOffset " ULLSPEC ".\n",
		        m_PartID, m_StartOffset, m_EndOffset);

    //Issue read ahead for next possible offset.
    if (posix_fadvise_wrapper(m_pSourceVolume->GetHandle(), m_StartOffset+m_CopyInfo.m_SourceStartOffset, 
                              m_EndOffset-m_StartOffset, INM_POSIX_FADV_WILLNEED))
    {
        DebugPrintf(SV_LOG_ERROR, "In VolumesCopierReaderStage::Copy, posix_fadvise failed \n");
    }

    SV_LONGLONG lastReadOffset = m_StartOffset;
    SV_LONGLONG physicalLastReadOffset = 0;
    std::string errmsg;
    VolumesCopier::ReaderWriterBuffer_t *prwbuf;
    while (cbRemaining > 0 && !QuitRequested())
    {
        prwbuf = GetReadWriteBuffer();
        if (0 == prwbuf)
        {
            DebugPrintf(SV_LOG_DEBUG, "no read write buffer got back to process next for source %s\n", 
                                      m_pSourceVolume->GetName().c_str());
            /* wait for WAITSECS is inside the get function */
            continue;
        }

        prwbuf->Reset();
        cbToRead = (SV_UINT) std::min( cbRemaining, (SV_ULONGLONG)m_CopyInfo.m_ReadBufferSize );
        INM_SAFE_ARITHMETIC(lastReadOffset += InmSafeInt<SV_UINT>::Type(cbread), INMAGE_EX(lastReadOffset)(cbread))
        INM_SAFE_ARITHMETIC(physicalLastReadOffset = lastReadOffset+InmSafeInt<SV_ULONGLONG>::Type(m_CopyInfo.m_SourceStartOffset), INMAGE_EX(lastReadOffset)(m_CopyInfo.m_SourceStartOffset))

        /* returns success if able to read till cbToRead else
         * gets bitmap till minimum of m_LengthForFileSystemClustersQuery or cbRemaining */
        if (!GetClusterBitmapIfRequired(physicalLastReadOffset, cbToRead, cbRemaining, m_CopyInfo.m_LengthForFileSystemClustersQuery))
        {
            msg << "For source volume " << m_CopyInfo.m_SourceVolumeName
                << ", could not get cluster volume bitmap. "
                << "offset: " << physicalLastReadOffset
                << ", Immediate count required: " << cbToRead
                << ", count for preread(includes immediate required): " << cbRemaining
                << ", maximum count configured: (includes immediate required) " << m_CopyInfo.m_LengthForFileSystemClustersQuery;
            throw INMAGE_EX(msg.str());
        }
        prwbuf->m_pVci = m_pCurrentVci;
        prwbuf->m_PartID = m_PartID;

        ReadInfo &rif = prwbuf->m_ReadInfo;
        if (m_CdpVolumeUtil.IsAnyClusterUsed(*m_pCurrentVci, physicalLastReadOffset, cbToRead, m_ClusterSize))
        {
            QuitFunction_t qf = std::bind1st(std::mem_fun(&VolumesCopierReaderStage::QuitRequested), this);
            ReadInfo::ReadInputs_t ri(m_pSourceVolume, physicalLastReadOffset, cbToRead,
                                      true, m_CopyInfo.m_SourceReadRetryInfo, qf);
			Read_t r = m_Read[m_CopyInfo.m_Profile];
		    if (!(this->*r)(rif, ri))
		    {
                throw INMAGE_EX(rif.GetErrorMessage());
		    }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "For volume %s, from offset " LLSPEC ", length %u, clusters are not in use\n", 
                                      m_CopyInfo.m_SourceVolumeName.c_str(), lastReadOffset, cbToRead);
        }
        cbread = cbToRead;
        prwbuf->m_Offset = lastReadOffset;
        prwbuf->m_Length = cbread;
        prwbuf->m_SourcePhysicalOffset = physicalLastReadOffset;
        SendReadWriteBuffer(prwbuf);

        INM_SAFE_ARITHMETIC(cbRemaining -= InmSafeInt<SV_UINT>::Type(cbread), INMAGE_EX(cbRemaining)(cbread))
    }

    //Release the previous hold buffer by fadvise.
    posix_fadvise_wrapper(m_pSourceVolume->GetHandle(), m_StartOffset+m_CopyInfo.m_SourceStartOffset, m_EndOffset-m_StartOffset, INM_POSIX_FADV_DONTNEED);
}


/* NOTE: startoffset is the physical one (added with getsrcstartoffset) */
bool VolumesCopierReaderStage::GetClusterBitmapIfRequired(const SV_LONGLONG startoffset,
                                                          const SV_UINT &lengthtocheck,
                                                          const SV_ULONGLONG &prereadlength,
                                                          const SV_UINT &maxlength)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bgot = false;
    bool bquery = false;
    std::stringstream msg;

    msg << "source " << m_CopyInfo.m_SourceVolumeName
        << ", startoffset: " << startoffset
        << ", counttocheck: " << lengthtocheck
        << ", prereadcount: " << prereadlength
        << ", maxcount: " << maxlength;

	/* TODO: This printing of bitmap causes lot of slowness
    DebugPrintf(SV_LOG_DEBUG, "For %s, existing volume cluster info in reader writer task:\n", msg.str().c_str());
    m_pSourceVolume->PrintVolumeClusterInfo(*m_pCurrentVci);
	*/

    if (m_CdpVolumeUtil.IsOffsetLengthCoveredInClusterInfo(startoffset, lengthtocheck, *m_pCurrentVci, m_ClusterSize))
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, covered\n", msg.str().c_str());
        bgot = true;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, not covered. Hence querying cluster bitmap\n", msg.str().c_str());
        SetCurrentVci_t setvci = m_SetCurrentVci[m_CopyInfo.m_PipelineReadWrite];
        (this->*setvci)();
        bquery = true;
    }

    if (bquery)
    {
        SV_UINT length =  (prereadlength > maxlength) ? maxlength : prereadlength;
        DebugPrintf(SV_LOG_DEBUG, "querying cluster bitmap from offset " LLSPEC ", length %u\n", 
                                  startoffset, length);
        bgot = m_CdpVolumeUtil.GetClusterBitmap(startoffset, length,
                                                *m_pCurrentVci, m_pSourceVolume, m_ClusterSize);
        if (bgot)
        {
            bgot = m_CdpVolumeUtil.IsOffsetLengthCoveredInClusterInfo(startoffset, lengthtocheck, *m_pCurrentVci, m_ClusterSize);
            if (bgot)
            {
                m_CdpVolumeUtil.CustomizeClusterBitmap(*m_CopyInfo.m_pClusterBitmapCustomizationInfos, *m_pCurrentVci);
				DebugPrintf(SV_LOG_DEBUG, "Got cluster bitmap:\n");
				m_pSourceVolume->PrintVolumeClusterInfo(*m_pCurrentVci);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, got cluster bitmap is less than even immediately required\n", msg.str().c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "For %s, could not get cluster bitmap\n", msg.str().c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bgot;
}


void VolumesCopierReaderStage::SetCurrentVciInApp(void)
{
    m_pCurrentVci = m_pVcis;
}


void VolumesCopierReaderStage::SetCurrentVciInStage(void)
{
    m_pCurrentVci = (m_pCurrentVci==m_pVcis) ? (m_pVcis+1) : m_pVcis;
}


bool VolumesCopierReaderStage::NoProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri)
{
	return rif.Read(ri);
}


bool VolumesCopierReaderStage::ProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri)
{
    ACE_Time_Value TimeBeforeRead, TimeAfterRead;

    TimeBeforeRead = ACE_OS::gettimeofday();
    bool rval = rif.Read(ri);
    TimeAfterRead = ACE_OS::gettimeofday();

	/* incase of retry, retry wait interval is also clubbed in */
    m_TimeForRead += (TimeAfterRead-TimeBeforeRead);

    return rval;
}


ACE_Time_Value VolumesCopierReaderStage::GetTimeForRead(void)
{
	return m_TimeForRead;
}


void VolumesCopierReaderStage::InformStatus(const std::string &s)
{
    if (m_CopyInfo.m_PipelineReadWrite)
    {
        SetExceptionStatus(s);
    }
    else
    {
        m_CopyInfo.m_pVolumesCopier->InformError(s, WAITSECS);
    }
}


bool VolumesCopierReaderStage::QuitRequested(long int seconds)
{
    QuitRequested_t p = m_QuitRequest[m_CopyInfo.m_PipelineReadWrite];
    return (this->*p)(seconds);
}


bool VolumesCopierReaderStage::QuitRequestedInApp(long int seconds)
{
    return m_CopyInfo.m_pVolumesCopier->ShouldQuit(seconds);
}


bool VolumesCopierReaderStage::QuitRequestedInStage(long int seconds)
{
    return ShouldQuit(seconds);
}


VolumesCopier::ReaderWriterBuffer_t * VolumesCopierReaderStage::GetReadWriteBuffer(void)
{
    GetReadWriteBuffer_t p = m_RWBufGetter[m_CopyInfo.m_PipelineReadWrite];
    return (this->*p)();
}


VolumesCopier::ReaderWriterBuffer_t * VolumesCopierReaderStage::GetReadWriteBufferInApp(void)
{
    return m_RWBufs.m_pReaderWriterBuffers;
}


VolumesCopier::ReaderWriterBuffer_t * VolumesCopierReaderStage::GetReadWriteBufferInStage(void)
{
    VolumesCopier::ReaderWriterBuffer_t *p = 0;
    if (m_RWBufs.m_FreeIndex < m_RWBufs.m_NumberOfBuffers)
    {
        p = m_RWBufs.m_pReaderWriterBuffers+m_RWBufs.m_FreeIndex;
        m_RWBufs.m_FreeIndex++;
    }
    else
    {
        void *pv = ReadFromLastStage(WAITSECS);
        p = (VolumesCopier::ReaderWriterBuffer_t *)pv;
    }

    return p;
}


void VolumesCopierReaderStage::SendReadWriteBuffer(VolumesCopier::ReaderWriterBuffer_t *prwbuf)
{
    SendReadWriteBuffer_t p = m_RWBufSender[m_CopyInfo.m_PipelineReadWrite];
    (this->*p)(prwbuf);
}


void VolumesCopierReaderStage::SendReadWriteBufferInApp(VolumesCopier::ReaderWriterBuffer_t *prwbuf)
{
    m_pVolumesCopierWriterStage->Process(prwbuf);
}


void VolumesCopierReaderStage::SendReadWriteBufferInStage(VolumesCopier::ReaderWriterBuffer_t *prwbuf)
{
    if (!WriteToForwardStage(prwbuf, sizeof (VolumesCopier::ReaderWriterBuffer_t), WAITSECS))
    {
        std::stringstream msg;
        msg << "For source volume " << m_CopyInfo.m_SourceVolumeName 
            << ", forwarding read write buffer to writer stage failed";
        throw INMAGE_EX(msg.str());
    }
}


VolumesCopierReaderStage::CopyInfo::CopyInfo(const std::string &sourcevolumename, VolumesCopier::VolumeInstantiator_t &volumeinstantiator,
                                             VolumesCopier::VolumeDestroyer_t &volumedestroyer, const std::string &filesystem, 
                                             const SV_ULONGLONG &sourcesize, const SV_ULONGLONG &sourcestartoffset,
                                             const bool &pipelinereadwrite, const unsigned int &readbuffercount,
                                             const unsigned int &readbuffersize, const unsigned int &readbufferalignment,
                                             const E_FS_SUPPORT_ACTIONS &fssupportaction, 
                                             const ReadInfo::ReadRetryInfo_t &sourcereadretryinfo,
                                             cdp_volume_util::ClusterBitmapCustomizationInfos_t * pcbcustinfos,
                                             const SV_UINT &lengthforfilesystemclustersquery,
                                             const bool &profile, VolumesCopier::ChkptInfos_t *pchkptinfos, 
                                             VolumesCopier *pvolumescopier)
 : m_SourceVolumeName(sourcevolumename),
   m_VolumeInstantiator(volumeinstantiator),
   m_VolumeDestroyer(volumedestroyer),
   m_Filesystem(filesystem),
   m_SourceSize(sourcesize),
   m_SourceStartOffset(sourcestartoffset),
   m_PipelineReadWrite(pipelinereadwrite),
   m_ReadBufferCount(readbuffercount),
   m_ReadBufferSize(readbuffersize),
   m_ReadBufferAlignment(readbufferalignment),
   m_FsSupportAction(fssupportaction),
   m_SourceReadRetryInfo(sourcereadretryinfo),
   m_pClusterBitmapCustomizationInfos(pcbcustinfos),
   m_LengthForFileSystemClustersQuery(lengthforfilesystemclustersquery),
   m_Profile(profile),
   m_pChkptInfos(pchkptinfos),
   m_pVolumesCopier(pvolumescopier)
{
}


VolumesCopierReaderStage::CopyInfo::CopyInfo(const CopyInfo &rhs) :
 m_SourceVolumeName(rhs.m_SourceVolumeName),
 m_VolumeInstantiator(rhs.m_VolumeInstantiator),
 m_VolumeDestroyer(rhs.m_VolumeDestroyer),
 m_Filesystem(rhs.m_Filesystem),
 m_SourceSize(rhs.m_SourceSize),
 m_SourceStartOffset(rhs.m_SourceStartOffset),
 m_PipelineReadWrite(rhs.m_PipelineReadWrite),
 m_ReadBufferCount(rhs.m_ReadBufferCount),
 m_ReadBufferSize(rhs.m_ReadBufferSize),
 m_ReadBufferAlignment(rhs.m_ReadBufferAlignment),
 m_FsSupportAction(rhs.m_FsSupportAction),
 m_SourceReadRetryInfo(rhs.m_SourceReadRetryInfo),
 m_pClusterBitmapCustomizationInfos(rhs.m_pClusterBitmapCustomizationInfos),
 m_LengthForFileSystemClustersQuery(rhs.m_LengthForFileSystemClustersQuery),
 m_Profile(rhs.m_Profile),
 m_pChkptInfos(rhs.m_pChkptInfos),
 m_pVolumesCopier(rhs.m_pVolumesCopier)
{
}


bool VolumesCopierReaderStage::CopyInfo::IsValid(std::ostream &error)
{
    bool isvalid = true;

    if (m_SourceVolumeName.empty())
    {
        isvalid = false;
        error << "No source volume supplied.";
    }

    if (!m_VolumeInstantiator)
    {
        isvalid = false;
        error << " No volume instantiator function specified.";
    }

    if (!m_VolumeDestroyer)
    {
        isvalid = false;
        error << " No volume destroyer function specified.";
    }

    if (0 == m_SourceSize)
    {
        isvalid = false;
        error << " Source volume size is zero.";
    }

	if (0 == m_ReadBufferCount)
	{
		isvalid = false;
        error << " Read buffer count is zero.";
	}

    if (0 == m_ReadBufferSize)
    {
        isvalid = false;
        error << " Read buffer size is zero.";
    }

    if (0 == m_pClusterBitmapCustomizationInfos)
    {
        isvalid = false;
        error << " Cluster bitmap customization information not supplied.";
    }

	if (0 == m_LengthForFileSystemClustersQuery)
	{
		isvalid = false;
		error << " Length for filesystem clusters query is zero.";
	}

    if (0 == m_pChkptInfos)
    {
        isvalid = false;
        error << " Checkpoint information not supplied.";
    }

    if (0 == m_pVolumesCopier)
    {
        isvalid = false;
        error << " Volumes copier is not supplied.";
    }

    return isvalid;
}
