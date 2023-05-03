#include <string>
#include <sstream>
#include <algorithm>
#include <functional>

#include "ace/ACE.h"

#include "volumescopierwriter.h"
#include "logger.h"
#include "portablehelpersmajor.h"
#include "inm_md5.h"
#include "inmageex.h"

#ifdef SV_UNIX
#include "portablehelpersminor.h"
#endif

VolumesCopierWriterStage::VolumesCopierWriterStage(const CopyInfo &wci)
: InmPipeline::Stage(wci.m_Profile),
  m_CopyInfo(wci),
  m_PartID(0),
  m_pTargetVolume(0),
  m_pVolumeApplier(0),
  m_TimeForTargetRead(0),
  m_TimeForApply(0),
  m_TimeForApplyOverhead(0),
  m_TimeForChecksumsComputeAndCompare(0),
  m_ProcessedBuffersCountInPartition(0),
  m_FilesystemUnusedBytesInPartition(0),
  m_CoveredOffset(0)
{
    m_QuitRequest[0] = &VolumesCopierWriterStage::QuitRequestedInApp;
    m_QuitRequest[1] = &VolumesCopierWriterStage::QuitRequestedInStage;

    m_ConsumeSourceData[0] = &VolumesCopierWriterStage::ConsumeSourceDataForDirectApply;
    m_ConsumeSourceData[1] = &VolumesCopierWriterStage::ConsumeSourceDataForComparision;

	m_Read[0] = &VolumesCopierWriterStage::NoProfileRead;
	m_Read[1] = &VolumesCopierWriterStage::ProfileRead;

	m_ChecksumsMatch[0] = &VolumesCopierWriterStage::NoProfileChecksumsMatch;
	m_ChecksumsMatch[1] = &VolumesCopierWriterStage::ProfileChecksumsMatch;
}


bool VolumesCopierWriterStage::Init(std::ostream &excmsg)
{
    bool binit = true;

    if (!m_CopyInfo.IsValid(excmsg))
    {
        binit = false;
    }

	std::string formattedName = m_CopyInfo.m_TargetVolumeName;
#ifdef SV_AIX
	std::string dskname = formattedName;
	formattedName = GetRawVolumeName(dskname);
#endif

    if (binit && 
        ((m_pTargetVolume = m_CopyInfo.m_VolumeInstantiator(formattedName)) == 0))
    {
        excmsg << "Failed to instantiate target volume " << m_CopyInfo.m_TargetVolumeName;
        binit = false;
    }

	/*
    BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioSequentialAccess;
    if(m_CopyInfo.m_UseUnBufferedIoForTarget)
    {
#ifdef SV_WINDOWS
        openMode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
        openMode |= BasicIo::BioDirect;
#endif
    }
	*/

    if (!m_pTargetVolume->OpenExclusive())
	{
		excmsg << "failed to open volume " << m_pTargetVolume->GetName() << " in exclusive mode";
		if (!m_pTargetVolume->Good()) 
		{
			excmsg << ", with error: " << m_pTargetVolume->LastError();
		}
		binit = false;
	}
 
	VolumeApplier::VolumeApplierFormationInputs_t ip(m_pTargetVolume, m_CopyInfo.m_Profile, m_CopyInfo.m_TargetVolumeName);
    /* TODO: should we pass, volume class to applier instantiator ? */
    if (binit && 
        ((m_pVolumeApplier = m_CopyInfo.m_VolumeApplierInstantiator(ip)) == 0))
    {
        excmsg << "Failed to instantiate volume applier for " << m_CopyInfo.m_TargetVolumeName;
        binit = false;
    }

    if (binit)
    {
        SV_UINT quotient = m_CopyInfo.m_ReadBufferSize / m_CopyInfo.m_ClusterSize;
        if ((quotient*m_CopyInfo.m_ClusterSize) != m_CopyInfo.m_ReadBufferSize)
        {
            /* TODO: should this be WARNING ? */
            DebugPrintf(SV_LOG_ERROR, "For target %s, read buffer size (%u) is not multiple of cluster size. "
                                      " The cluster size is %u. Moving a head by making it as nearest multiple upwards\n", 
                                      m_CopyInfo.m_TargetVolumeName.c_str(), m_CopyInfo.m_ReadBufferSize, m_CopyInfo.m_ClusterSize);
            /* NOTE: going upwards takes care of m_CopyInfo.m_ReadBufferSize being
             * lesser than m_CopyInfo.m_ClusterSize */
            m_CopyInfo.m_ReadBufferSize = (m_CopyInfo.m_ClusterSize * (quotient+1));
        }

        if (m_CopyInfo.m_CompareThenCopy)
        {
            binit = m_ReadInfo.Init(m_CopyInfo.m_ReadBufferSize, m_CopyInfo.m_ReadBufferAlignment);
            if (!binit)
            {
                excmsg << "Could not allocate aligned target read buffer of " << m_CopyInfo.m_ReadBufferSize << " bytes.";
            }
        }
    }

    return binit;
}


void VolumesCopierWriterStage::Flush(void)
{
    if (!m_pVolumeApplier->Flush())
    {
        std::stringstream msg;
        msg << "For target " << m_CopyInfo.m_TargetVolumeName 
            << " at last of partition coverage, the applier failed to flush with error message " 
            << m_pVolumeApplier->GetErrorMessage();
        throw INMAGE_EX(msg.str());
    }

    if (m_ProcessedBuffersCountInPartition)
    {
        VolumesCopier::CoveredOffsetInfo_t coi(m_PartID, m_CoveredOffset, m_FilesystemUnusedBytesInPartition);
        m_CopyInfo.m_UpdateCoveredOffset(coi);
        m_ProcessedBuffersCountInPartition = 0;
		m_FilesystemUnusedBytesInPartition = 0;
    }
}


VolumesCopierWriterStage::~VolumesCopierWriterStage()
{
    if (m_pTargetVolume)
    {
        m_CopyInfo.m_VolumeDestroyer(m_pTargetVolume);
        m_pTargetVolume = 0;
    }

    if (m_pVolumeApplier)
    {
		m_CopyInfo.m_VolumeApplierDestroyer(m_pVolumeApplier);
        m_pVolumeApplier = 0;
    }
}


int VolumesCopierWriterStage::svc(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

	try
	{
        std::stringstream excmsg;
        if (!Init(excmsg))
        {
		    throw INMAGE_EX(excmsg.str());
        }

        do
        {
            void *pv = ReadFromBackStage(WAITSECS);
            if (pv)
            {
                VolumesCopier::ReaderWriterBuffer_t *prwbuf = (VolumesCopier::ReaderWriterBuffer_t *)pv;

                /* throws INMAGE_EX */
                Process(prwbuf);

                /* throws INMAGE_EX */
                SendBackReadWriteBuffer(prwbuf);
            }
        } while (!QuitRequested());

        /* throws INMAGE_EX */
        Flush();

		m_TimeForApply = m_pVolumeApplier->GetTimeForApply();
		m_TimeForApplyOverhead = m_pVolumeApplier->GetTimeForOverhead();
	}
	catch (ContextualException& ce)
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", ce.what());
        SetExceptionStatus(ce.what());
	}
	catch (std::exception& e)
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", e.what());
        SetExceptionStatus(e.what());
	}

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0 ;
}


void VolumesCopierWriterStage::Process(VolumesCopier::ReaderWriterBuffer_t *prwbuf)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with logical offset " LLSPEC ", length %u, physical offset " LLSPEC ", data: %p\n",
		        FUNCTION_NAME, prwbuf->m_Offset, prwbuf->m_Length, prwbuf->m_SourcePhysicalOffset, prwbuf->m_ReadInfo.GetData());
    m_PartID = prwbuf->m_PartID;
    std::stringstream msg;
    std::string errmsg;

    if (prwbuf->m_ReadInfo.GetLength())
    {
        SV_LONGLONG logicaloffset = prwbuf->m_Offset;
        SV_LONGLONG physicaloffsettoask = prwbuf->m_SourcePhysicalOffset;
        SV_UINT lengthcoverediniteration = 0;
        SV_UINT continuousclusters;
        SV_UINT lengthremaining = prwbuf->m_Length;
        char *pdata = prwbuf->m_ReadInfo.GetData();
        bool bisused;
    
        while (lengthremaining)
        {
            /* TODO: use same logic in ntfs fast sync */
            continuousclusters = m_CdpVolumeUtil.GetContinuousClusters(*prwbuf->m_pVci, physicaloffsettoask, lengthremaining, m_CopyInfo.m_ClusterSize, 
                                                                       bisused);
            lengthcoverediniteration = m_CdpVolumeUtil.GetLengthCovered(continuousclusters, physicaloffsettoask, lengthremaining, m_CopyInfo.m_ClusterSize);
		    DebugPrintf(SV_LOG_DEBUG, "physical offset " LLSPEC 
			                          ", logical offset " LLSPEC
								      ", pdata = %p "
								      ", length remaining %u, continuous clusters %u, length covered %u is %s\n",
								      physicaloffsettoask, logicaloffset, pdata, lengthremaining, 
								      continuousclusters, lengthcoverediniteration, bisused ? "used" : "unused");
            if (bisused)
            {
                if (!(this->*m_ConsumeSourceData[m_CopyInfo.m_CompareThenCopy])(&logicaloffset, lengthcoverediniteration, pdata, errmsg))
                {
                    msg << "For target volume " << m_CopyInfo.m_TargetVolumeName
                        << ", consumption of source read data failed "
                        << ", with error " << errmsg << ". "
                        << "Physical offset: " << prwbuf->m_SourcePhysicalOffset
                        << ", logical offset: " << prwbuf->m_Offset
                        << ", length: " << prwbuf->m_Length;
                    throw INMAGE_EX(msg.str());
                }
            }
			else
			{
				m_FilesystemUnusedBytesInPartition += lengthcoverediniteration;
			}
            physicaloffsettoask += lengthcoverediniteration;
            logicaloffset += lengthcoverediniteration;
            lengthremaining -= lengthcoverediniteration;
            pdata += lengthcoverediniteration;
        }
    }
	else
	{
		/* no data in m_ReadInfo indicates that is fully unused */
		m_FilesystemUnusedBytesInPartition += prwbuf->m_Length;
	}

    m_ProcessedBuffersCountInPartition++;
    m_CoveredOffset = prwbuf->m_Offset+prwbuf->m_Length;

    if( ((m_ProcessedBuffersCountInPartition%64) == 0) || (m_CoveredOffset == (*m_CopyInfo.m_pChkptInfos)[m_PartID].endOffset) )
    {
        /* TODO: better algorithm to avoid small files to retention as some bytes
         * may be accumulated and some may be matched / unused */
        if (!m_pVolumeApplier->Flush())
        {
            msg << "For target " << m_CopyInfo.m_TargetVolumeName 
                << " the applier failed to flush with error message " 
                << m_pVolumeApplier->GetErrorMessage();
            throw INMAGE_EX(msg.str());
        }
        DebugPrintf(SV_LOG_DEBUG, ULLSPEC " Blocks Processed succesfully to the volume %s.\n", 
                                  m_ProcessedBuffersCountInPartition, m_CopyInfo.m_TargetVolumeName.c_str());
        VolumesCopier::CoveredOffsetInfo_t coi(m_PartID, m_CoveredOffset, m_FilesystemUnusedBytesInPartition);
        m_CopyInfo.m_UpdateCoveredOffset(coi);
        m_ProcessedBuffersCountInPartition = 0;
		m_FilesystemUnusedBytesInPartition = 0;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool VolumesCopierWriterStage::ConsumeSourceDataForDirectApply(const SV_LONGLONG *offset,    /* logical one not physical */
                                                               const SV_UINT &length,        /* length of data */
                                                               char *data,
                                                               std::string &errmsg)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with offset " LLSPEC " length %u, data %p\n",FUNCTION_NAME, *offset, length, data);

    bool bapplied = m_pVolumeApplier->Apply(*offset, length, data);
    if (!bapplied)
    {
        errmsg = "CRITICAL " + m_pVolumeApplier->GetErrorMessage();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bapplied;
}


bool VolumesCopierWriterStage::ConsumeSourceDataForComparision(const SV_LONGLONG *offset,    /* logical one not physical */
                                                               const SV_UINT &length,        /* length of data */
                                                               char *data,
                                                               std::string &errmsg)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with offset " LLSPEC " length %u, data %p\n",FUNCTION_NAME, *offset, length, data);
    bool bapplied = true;
    std::stringstream msg;

    do 
    {
		QuitFunction_t qf = std::bind1st(std::mem_fun(&VolumesCopierWriterStage::ShouldQuit), this);
        ReadInfo::ReadInputs_t ri(m_pTargetVolume, *offset, length,
			                      true, ReadInfo::ReadRetryInfo_t(), qf);
		Read_t r = m_Read[m_CopyInfo.m_Profile];
		if (!(this->*r)(m_ReadInfo, ri))
		{
            errmsg = m_ReadInfo.GetErrorMessage();
			bapplied = false;
			break;
		}

        // Writing to the target device if data is mismatching.
		ChecksumsMatch_t cm = m_ChecksumsMatch[m_CopyInfo.m_Profile];
		if (!(this->*cm)(data, m_ReadInfo.GetData(), length))
        {
            DebugPrintf(SV_LOG_DEBUG, "For target %s, from offset " LLSPEC ", length %u, data does not match with source.\n",
                                      m_CopyInfo.m_TargetVolumeName.c_str(), *offset, length);
            if (!m_pVolumeApplier->Apply(*offset, length, data))
            {
                errmsg = "CRITICAL " + m_pVolumeApplier->GetErrorMessage();
                bapplied = false;
                break;
            }
        }
    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bapplied;
}


bool VolumesCopierWriterStage::NoProfileChecksumsMatch(char *sourcedata, char *targetdata, const SV_UINT &length)
{
	return DoChecksumsMatch(sourcedata, targetdata, length);
}


bool VolumesCopierWriterStage::ProfileChecksumsMatch(char *sourcedata, char *targetdata, const SV_UINT &length)
{
	ACE_Time_Value TimeBeforeRead, TimeAfterRead;

    TimeBeforeRead = ACE_OS::gettimeofday();
    bool rval = DoChecksumsMatch(sourcedata, targetdata, length);
    TimeAfterRead = ACE_OS::gettimeofday();

	/* incase of retry, retry wait interval is also clubbed in */
    m_TimeForChecksumsComputeAndCompare += (TimeAfterRead-TimeBeforeRead);

    return rval;
}


bool VolumesCopierWriterStage::DoChecksumsMatch(char *sourcedata, char *targetdata, const SV_UINT &length)
{
	unsigned char srcHash[DIGEST_LEN];
	unsigned char tgtHash[DIGEST_LEN];
	INM_MD5_CTX ctx[2];
	INM_MD5Init(&ctx[0]);
	INM_MD5Init(&ctx[1]);
	INM_MD5Update(&ctx[0], (unsigned char*)sourcedata, length );
	INM_MD5Update(&ctx[1], (unsigned char*)targetdata, length );
	INM_MD5Final(srcHash, &ctx[0]);
	INM_MD5Final(tgtHash, &ctx[1]);

	return (0==memcmp(srcHash, tgtHash, DIGEST_LEN));
}


ACE_Time_Value VolumesCopierWriterStage::GetTimeForTargetRead(void)
{
	return m_TimeForTargetRead;
}


bool VolumesCopierWriterStage::NoProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri)
{
	return rif.Read(ri);
}


bool VolumesCopierWriterStage::ProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri)
{
    ACE_Time_Value TimeBeforeRead, TimeAfterRead;

    TimeBeforeRead = ACE_OS::gettimeofday();
    bool rval = rif.Read(ri);
    TimeAfterRead = ACE_OS::gettimeofday();

	/* incase of retry, retry wait interval is also clubbed in */
    m_TimeForTargetRead += (TimeAfterRead-TimeBeforeRead);

    return rval;
}


ACE_Time_Value VolumesCopierWriterStage::GetTimeForApply(void)
{
	return m_TimeForApply;
}


ACE_Time_Value VolumesCopierWriterStage::GetTimeForApplyOverhead(void)
{
	return m_TimeForApplyOverhead;
}


ACE_Time_Value VolumesCopierWriterStage::GetTimeForChecksumsComputeAndCompare(void)
{
	return m_TimeForChecksumsComputeAndCompare;
}


bool VolumesCopierWriterStage::QuitRequested(long int seconds)
{
    QuitRequested_t p = m_QuitRequest[m_CopyInfo.m_PipelineReadWrite];
    return (this->*p)(seconds);
}


bool VolumesCopierWriterStage::QuitRequestedInApp(long int seconds)
{
    return m_CopyInfo.m_pVolumesCopier->ShouldQuit(seconds);
}


bool VolumesCopierWriterStage::QuitRequestedInStage(long int seconds)
{
    return ShouldQuit(seconds);
}


void VolumesCopierWriterStage::SendBackReadWriteBuffer(VolumesCopier::ReaderWriterBuffer_t *prwbuf)
{
    if (!WriteToFirstStage(prwbuf, sizeof (VolumesCopier::ReaderWriterBuffer_t), WAITSECS))
    {
        std::stringstream msg;
        msg << "For target volume " << m_CopyInfo.m_TargetVolumeName 
            << ", sending back read write buffer to reader stage failed";
        throw INMAGE_EX(msg.str());
    }
}


VolumesCopierWriterStage::CopyInfo::CopyInfo(const std::string &targetvolumename, VolumesCopier::VolumeInstantiator_t &volumeinstantiator,
                                             VolumesCopier::VolumeDestroyer_t &volumedestroyer, const bool &comparethencopy, 
                                             const bool &pipelinereadwrite, const unsigned int &readbuffersize, 
											 const unsigned int &readbufferalignment,
                                             VolumesCopier::VolumeApplierInstantiator_t &volumeapplierinstantiator,
                                             VolumesCopier::VolumeApplierDestroyer_t &volumeapplierdestroyer, 
                                             VolumesCopier::UpdateCoveredOffset_t &updatecoveredoffset, const bool &useunbufferediofortarget, 
								             const bool &profile, VolumesCopier::ChkptInfos_t *pchkptinfos, VolumesCopier *pvolumescopier,
                                             const SV_UINT &clustersize) :
 m_TargetVolumeName(targetvolumename),
 m_VolumeInstantiator(volumeinstantiator),
 m_VolumeDestroyer(volumedestroyer),
 m_CompareThenCopy(comparethencopy),
 m_PipelineReadWrite(pipelinereadwrite),
 m_ReadBufferSize(readbuffersize),
 m_ReadBufferAlignment(readbufferalignment),
 m_VolumeApplierInstantiator(volumeapplierinstantiator),
 m_VolumeApplierDestroyer(volumeapplierdestroyer),
 m_UpdateCoveredOffset(updatecoveredoffset),
 m_UseUnBufferedIoForTarget(useunbufferediofortarget),
 m_Profile(profile),
 m_pChkptInfos(pchkptinfos),
 m_pVolumesCopier(pvolumescopier),
 m_ClusterSize(clustersize)
{
}


VolumesCopierWriterStage::CopyInfo::CopyInfo(const CopyInfo &rhs) :
 m_TargetVolumeName(rhs.m_TargetVolumeName),
 m_VolumeInstantiator(rhs.m_VolumeInstantiator),
 m_VolumeDestroyer(rhs.m_VolumeDestroyer),
 m_CompareThenCopy(rhs.m_CompareThenCopy),
 m_PipelineReadWrite(rhs.m_PipelineReadWrite),
 m_ReadBufferSize(rhs.m_ReadBufferSize),
 m_ReadBufferAlignment(rhs.m_ReadBufferAlignment),
 m_VolumeApplierInstantiator(rhs.m_VolumeApplierInstantiator),
 m_VolumeApplierDestroyer(rhs.m_VolumeApplierDestroyer),
 m_UpdateCoveredOffset(rhs.m_UpdateCoveredOffset),
 m_UseUnBufferedIoForTarget(rhs.m_UseUnBufferedIoForTarget),
 m_Profile(rhs.m_Profile),
 m_pChkptInfos(rhs.m_pChkptInfos),
 m_pVolumesCopier(rhs.m_pVolumesCopier),
 m_ClusterSize(rhs.m_ClusterSize)
{
}


bool VolumesCopierWriterStage::CopyInfo::IsValid(std::ostream &error)
{
    bool isvalid = true;

    if (m_TargetVolumeName.empty())
    {
        isvalid = false;
        error << " No target volume supplied.";
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

    if (0 == m_ReadBufferSize)
    {
        isvalid = false;
        error << " Read buffer size is zero.";
    }

    if (!m_VolumeApplierInstantiator)
    {
        isvalid = false;
        error << " Volume applier instantiator unspecified.";
    }

    if (!m_VolumeApplierDestroyer)
    {
        isvalid = false;
        error << " Volume applier destroyer unspecified.";
    }

    if (!m_UpdateCoveredOffset)
    {
        isvalid = false;
        error << " update covered offset function unspecified.";
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

    if (0 == m_ClusterSize)
    {
        isvalid = false;
        error << " cluster size specified is zero.";
    }
    
    return isvalid;
}
