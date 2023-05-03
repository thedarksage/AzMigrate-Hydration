#include <string>
#include <cstring>
#include <sstream>
#include <set>
#include <functional>

#include <boost/shared_ptr.hpp>

#include "volumescopier.h"
#include "logger.h"
#include "inmpipelineimp.h"
#include "localconfigurator.h"
#include "portablehelpers.h"
#include "cdputil.h"
#include "cdpvolumeutil.h"
#include "inm_md5.h"
#include "volumescopierreader.h"
#include "volumescopierwriter.h"
#include "volumescopiertask.h"
#include "inmsafeint.h"
#include "inmageex.h"

static std::string const CheckPtFileName("checkpoint.dat");
static SV_UINT const DefaultPartitions = 128;

VolumesCopier::VolumesCopier(const CopyInfo &ci)
 : m_CopyInfo(ci),
   m_pInmPipeline(0),
   m_lastUpdatedOffset(0),
   m_totalCoveredBytes(0),
   m_totalFilesystemUnusedBytes(0),
   m_CoveredBytesChangeForAction(0),
   m_TimeForSourceRead(0),
   m_TimeForTargetRead(0),
   m_TimeForApply(0),
   m_TimeForApplyOverhead(0),
   m_TimeForChecksumsComputeAndCompare(0),
   m_WriterWaitTimeForBuffer(0),
   m_ReaderWaitTimeForBuffer(0),
   m_ShouldQuit(false),
   m_ClusterSize(0)
{
    m_GetStateOfWorker[0] = &VolumesCopier::GetStateOfTask;
    m_GetStateOfWorker[1] = &VolumesCopier::GetStateOfPipeline;
}


bool VolumesCopier::Init(void)
{
    std::string errormessage;
    bool binited = m_CopyInfo.IsValid(errormessage);

    if (binited)
    {
        m_CoveredBytesChangeForAction = (m_CopyInfo.m_SourceSize * m_CopyInfo.m_ApplyPercentChangeForAction) / 100;
    }
    else
    {
        m_ErrorMessage = "The copy information provided is invalid with error: ";
        m_ErrorMessage += errormessage;
    }
    
    return binited;
}


VolumesCopier::~VolumesCopier()
{
    if (m_pInmPipeline)
    {
        delete m_pInmPipeline;
        m_pInmPipeline = 0;
    }
}


bool VolumesCopier::Copy(void)
{
    bool bcopied = CreateChkptOffsetDir() && ReadChkptInfoFromChkptFile();

    if (bcopied)
    {
        SV_ULONGLONG totalSentBytes = 0;
        for (size_t index = 0; index < m_chkptInfos.size(); ++index)
        {
            totalSentBytes += m_chkptInfos[index].lastWroteOffset - m_chkptInfos[index].startOffset;
        }
        m_totalCoveredBytes = totalSentBytes; // Initialise it to the last sent bytes

        if (0 == totalSentBytes)
        {
            bcopied = m_CopyInfo.m_ActionOnNoBytesCovered(CoveredBytesInfo(m_totalCoveredBytes, m_totalFilesystemUnusedBytes));
            if (!bcopied)
            {
                m_ErrorMessage = "The action on no bytes covered failed.";
            }
        }
    }

    if (bcopied)
    {
        bcopied = StartAndWaitForWorker();
    }

    return bcopied;
}


bool VolumesCopier::StartAndWaitForWorker(void)
{
    bool brval = false;

    UpdateCoveredOffset_t uco = std::bind1st(std::mem_fun(&VolumesCopier::updateInMomoryChkptInfo), this);
    VolumesCopierReaderStage::CopyInfo rci(m_CopyInfo.m_SourceVolumeName,  m_CopyInfo.m_VolumeInstantiator,
                                           m_CopyInfo.m_VolumeDestroyer, m_CopyInfo.m_Filesystem,
                                           m_CopyInfo.m_SourceSize, m_CopyInfo.m_SourceStartOffset,
										   m_CopyInfo.m_PipelineReadWrite, m_CopyInfo.m_IOBufferCount,
										   m_CopyInfo.m_IOBufferSize, m_CopyInfo.m_IOBufferAlignment,
										   m_CopyInfo.m_FsSupportAction, m_CopyInfo.m_SourceReadRetryInfo, 
										   m_CopyInfo.m_pClusterBitmapCustomizationInfos, 
										   m_CopyInfo.m_LengthForFileSystemClustersQuery,
                                           m_CopyInfo.m_Profile, &m_chkptInfos, this);
    VolumesCopierReaderStage rs(rci);

    VolumesCopierWriterStage::CopyInfo wci(m_CopyInfo.m_TargetVolumeName, m_CopyInfo.m_VolumeInstantiator,
                                           m_CopyInfo.m_VolumeDestroyer, m_CopyInfo.m_CompareThenCopy,
                                           m_CopyInfo.m_PipelineReadWrite, m_CopyInfo.m_IOBufferSize, 
										   m_CopyInfo.m_IOBufferAlignment, m_CopyInfo.m_VolumeApplierInstantiator, 
                                           m_CopyInfo.m_VolumeApplierDestroyer, uco, m_CopyInfo.m_UseUnBufferedIoForTarget, 
                                           m_CopyInfo.m_Profile, &m_chkptInfos, this, m_ClusterSize);
    VolumesCopierWriterStage ws(wci);
    boost::shared_ptr<VolumesCopierTask> vct;

    if (m_CopyInfo.m_PipelineReadWrite)
    {
        InmPipeline::Stages_t s;
        s.push_back(&rs);
        s.push_back(&ws);
        m_pInmPipeline = new (std::nothrow) InmPipelineImp();
        if (m_pInmPipeline)
        {
            brval = m_pInmPipeline->Create(s, InmPipeline::CIRCULAR);
            if (!brval)
            {
                /* TODO: get error message from pipeline */
                m_ErrorMessage = "could not create read write pipeline";
            }
        }
        else
        {
            m_ErrorMessage = "could not allocate read write pipeline";
        }
    }
    else
    {
        vct.reset(new VolumesCopierTask(&m_ThreadManager, this, &rs, &ws));
        brval = (-1!=vct->open());
        if (!brval)
        {
            m_ErrorMessage = "could not start volumes copier task";
        }
    }

    if (brval)
    {
        brval = WaitForCopy();
    }

    if (brval)
    {
        if (m_CopyInfo.m_Profile)
        {
            m_TimeForSourceRead = rs.GetTimeForRead();
            m_TimeForTargetRead = ws.GetTimeForTargetRead();
            m_TimeForApply = ws.GetTimeForApply();
			m_TimeForApplyOverhead = ws.GetTimeForApplyOverhead();
            m_TimeForChecksumsComputeAndCompare = ws.GetTimeForChecksumsComputeAndCompare();
			m_WriterWaitTimeForBuffer = ws.GetTimeForReadWait();
			m_ReaderWaitTimeForBuffer = rs.GetTimeForReadWait();
        }

        if (m_CopyInfo.m_SourceSize == m_totalCoveredBytes)
        {
            brval = m_CopyInfo.m_ActionOnAllBytesCovered(CoveredBytesInfo(m_totalCoveredBytes, m_totalFilesystemUnusedBytes));
            if (!brval)
            {
                m_ErrorMessage = "The action on all bytes covered failed.";
            }
        }
        else
        {
            brval = WriteChkptInfoToChkptFile();
        }
    }

    return brval;
}


bool VolumesCopier::WaitForCopy(void)
{
    bool bwait = true;
    State_t st;

    /* TODO: on copy completion, there is a subtle issue here 
     * as this will wait unnecessary for defined seconds even
     * though there is no need. This will be fixed later; currently
     * the interval is made much smaller */
    while (bwait && (!m_CopyInfo.m_QuitFunction(m_CopyInfo.m_CopyMonitorInterval)))
    {
        st = GetStateOfWorker();
        if (STATE_NORMAL != st)
        {
            bwait = false;
        }
        else
        {
            ACE_Guard<ACE_Recursive_Thread_Mutex> guard( m_Lock );
            SV_ULONGLONG totalCoveredBytes = m_totalCoveredBytes;
			SV_ULONGLONG totalFilesystemUnusedBytes = m_totalFilesystemUnusedBytes;
            guard.release();
 
            if (((totalCoveredBytes-m_lastUpdatedOffset) >= m_CoveredBytesChangeForAction) || (totalCoveredBytes == m_CopyInfo.m_SourceSize))
            {
                if (m_CopyInfo.m_ActionOnBytesCovered(CoveredBytesInfo(totalCoveredBytes, totalFilesystemUnusedBytes)))
                {
                    m_lastUpdatedOffset = totalCoveredBytes;
                }
                else
                {
                    /* should we abort as next time, the update can be sent */
                    DebugPrintf(SV_LOG_ERROR, "The action on bytes covered failed for " ULLSPEC " bytes covered for source volume %s\n",
                                              totalCoveredBytes, m_CopyInfo.m_SourceVolumeName.c_str());
                }

                bwait = WriteChkptInfoToChkptFile();
                if (bwait)
                {
                    if (totalCoveredBytes == m_CopyInfo.m_SourceSize)
                    {
                        break;
                    }
                }
            }
        }
    }
     
    m_ShouldQuit = true;
    if (m_pInmPipeline)
    {
        DebugPrintf(SV_LOG_DEBUG, "Waiting for pipe line to finish\n");
        m_pInmPipeline->Destroy();
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Waiting for copier task to finish\n");
        m_ThreadManager.wait();

        /* now drain the message queue for task; 
         * just in case we might have got quit 
         * from application, but task has queued an (only one outstanding) error */
        bwait = (STATE_NORMAL==GetStateOfWorker());
    }
   
    return bwait;
}


ACE_Time_Value VolumesCopier::GetTimeForSourceRead(void)
{
	return m_TimeForSourceRead;
}


ACE_Time_Value VolumesCopier::GetTimeForTargetRead(void)
{
	return m_TimeForTargetRead;
}


ACE_Time_Value VolumesCopier::GetTimeForApply(void)
{
	return m_TimeForApply;
}


ACE_Time_Value VolumesCopier::GetTimeForApplyOverhead(void)
{
	return m_TimeForApplyOverhead;
}


ACE_Time_Value VolumesCopier::GetTimeForChecksumsComputeAndCompare(void)
{
    return m_TimeForChecksumsComputeAndCompare;
}


ACE_Time_Value VolumesCopier::GetWriterWaitTimeForBuffer(void)
{
    return m_WriterWaitTimeForBuffer;
}


ACE_Time_Value VolumesCopier::GetReaderWaitTimeForBuffer(void)
{
    return m_ReaderWaitTimeForBuffer;
}


State_t VolumesCopier::GetStateOfWorker(void)
{
    GetStateOfWorker_t p = m_GetStateOfWorker[m_CopyInfo.m_PipelineReadWrite];
    return (this->*p)();
}


State_t VolumesCopier::GetStateOfPipeline(void)
{
    State_t st = m_pInmPipeline->GetState();
    if (STATE_NORMAL != st)
    {
        m_ErrorMessage = std::string("pipeline state is ") + StrState[st] + ", exception message is " + m_pInmPipeline->GetExceptionMsg() + ". ";
        Statuses_t ss;
        m_pInmPipeline->GetStatusOfStages(ss);
        GetStatuses(ss, m_ErrorMessage);
    }
 
    return st;
}


State_t VolumesCopier::GetStateOfTask(void)
{
    const char *emsg = 0;

    /* time is 0 since alread wait has been done earlier */
    ACE_Message_Block *pmb = DeQ(&m_QForTaskState, 0);
    if (pmb)
    {
        emsg = pmb->base();
        pmb->release();
    }

    State_t st = STATE_NORMAL;
    if (emsg)
    {
        m_ErrorMessage = emsg;
        delete [] emsg;
        st = STATE_EXCEPTION;
    }
 
    return st;
}


void VolumesCopier::InformError(const std::string &s, const int &waitsecs)
{
    size_t nserr;
    INM_SAFE_ARITHMETIC(nserr = InmSafeInt<size_t>::Type(s.size())+1, INMAGE_EX(s.size()))
    char *serr = new (std::nothrow) char[nserr];
    if (serr)
    {
        inm_strcpy_s(serr, s.size() + 1, s.c_str());
        if (!PostMsg(serr, s.size()+1, waitsecs))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to enque error message %s from volumescopier reader\n", serr);
            delete [] serr;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to allocate memory for volumescopier reader error\n");
    }
}


bool VolumesCopier::PostMsg(const char *msg, const size_t &size, const int &waitsecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Message_Block *pmb = new ACE_Message_Block((char *)msg, size);
    QuitFunction_t qf = std::bind1st(std::mem_fun(&VolumesCopier::ShouldQuit), this);
     
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return EnQ(&m_QForTaskState, pmb, waitsecs, qf);
}


bool VolumesCopier::ShouldQuit(long int seconds)
{
    return m_ShouldQuit ? true : m_CopyInfo.m_QuitFunction(seconds);
}


void VolumesCopier::updateInMomoryChkptInfo(const CoveredOffsetInfo_t coveredoffsetinfo)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_Lock);
    m_totalCoveredBytes +=  coveredoffsetinfo.m_Offset - m_chkptInfos[coveredoffsetinfo.m_PartitionID].lastWroteOffset; 
	m_totalFilesystemUnusedBytes += coveredoffsetinfo.m_FilesystemUnusedBytes;
    m_chkptInfos[coveredoffsetinfo.m_PartitionID].lastWroteOffset = coveredoffsetinfo.m_Offset;
}


bool VolumesCopier::WriteChkptInfoToChkptFile(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::stringstream chkData;
    SV_UINT size = m_chkptInfos.size();
    chkData << ' ' << size << ' ';
	chkData << ' ' << m_totalFilesystemUnusedBytes << ' ';

    ACE_Guard<ACE_Recursive_Thread_Mutex> guard( m_Lock );
    for (size_t i=0; i < size ; ++i)
    {
        chkData << m_chkptInfos[i].startOffset <<' '
            << m_chkptInfos[i].endOffset << ' '
            << m_chkptInfos[i].lastWroteOffset << ' ';
    }
    //Release explicitly.
    guard.release();

    unsigned char hash[DIGEST_LEN];

    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char *)chkData.str().c_str(), chkData.str().size());
    INM_MD5Final(hash, &ctx);

    DumpChkptInfo();
    BasicIo bio(m_ChkptFile, BasicIo::BioOpenCreate | BasicIo::BioRW | BasicIo::BioBinary);

    if (!bio.Good()) 
    {
        std::stringstream msg;
        msg << "VolumesCopier::WriteChkptInfoToChkptFile() open checkpoint offset file " 
            << m_ChkptFile 
            << " failed: " 
            << bio.LastError();
        m_ErrorMessage = msg.str();
        return false;
    }

    bio.Seek(0, BasicIo::BioBeg);

    if( DIGEST_LEN != bio.Write((const char*)hash, DIGEST_LEN))
    {
        std::stringstream msg;
        msg << "VolumesCopier::WriteChkptInfoToChkptFile() Failed to hash computed checksum to the file: "
            << m_ChkptFile <<"\n";
        m_ErrorMessage = msg.str();
        return false;
    }

    if( chkData.str().size() != bio.Write(chkData.str().c_str(), chkData.str().size()))
    {
        std::stringstream msg;
        msg << "VolumesCopier::WriteChkptInfoToChkptFile() Failed to write checksum Info to the file: "
            << m_ChkptFile <<"\n";
        m_ErrorMessage = msg.str();
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return true;
}


std::string VolumesCopier::GetErrorMessage(void)
{
    return m_ErrorMessage;
}


bool VolumesCopier::CreateChkptOffsetDir(void)
{
    LocalConfigurator localConfigurator;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_ChkptOffsetDir = m_CopyInfo.m_DirectoryForInternalOperations;

	if (m_ChkptOffsetDir[m_ChkptOffsetDir.size()-1] != ACE_DIRECTORY_SEPARATOR_CHAR_A)
	{
		m_ChkptOffsetDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
	}

    m_ChkptOffsetDir += "copy";
    m_ChkptOffsetDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;

    std::string src = m_CopyInfo.m_SourceVolumeName;
    FormatVolumeName(src);
    FormatVolumeNameToGuid(src);
    ExtractGuidFromVolumeName(src);

    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(src);
    }

 
    /* TODO: instead of source, target names, use an
     * identifier so that remote volumes can also 
     * be used in path names mentioning there machine addresses */
    std::string tgt = m_CopyInfo.m_TargetVolumeName;
    FormatVolumeName(tgt);
    FormatVolumeNameToGuid(tgt);
    ExtractGuidFromVolumeName(tgt);

    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(tgt);
    }

    //Append the hash of src + tgt to the m_ChkptOffsetDir
    m_ChkptOffsetDir +=  CDPUtil::hash( src + tgt ); 
    m_ChkptOffsetDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;

    SVERROR rc = SVMakeSureDirectoryPathExists( m_ChkptOffsetDir.c_str());
    if (rc.failed()) {
        m_ErrorMessage = "Failed to make directory " + m_ChkptOffsetDir + " with error: " + rc.toString();
        return false;
    }

    if (m_CopyInfo.m_SessionNumber.empty())
    {
        /* TODO: for now, cleaning up of files that
         * exist is fine here. No need to clean up
         * any directories */
        CleanupDirectory(m_ChkptOffsetDir.c_str(), "*");
    }
    else
    {
        /* TODO: for now, cleaning up of directories that
         * do not equal session number is fine. No need
         * to clean up earlier check point files with no
         * session number */
        std::set<std::string> exceptionlist;
        exceptionlist.insert(m_CopyInfo.m_SessionNumber);
        RemoveDirectories(m_ChkptOffsetDir, exceptionlist);
        m_ChkptOffsetDir += m_CopyInfo.m_SessionNumber;
        m_ChkptOffsetDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    }

    rc = SVMakeSureDirectoryPathExists( m_ChkptOffsetDir.c_str());
    if (rc.failed()) {
        m_ErrorMessage = "Failed to make directory " + m_ChkptOffsetDir + " with error: " + rc.toString();
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return true;
}


bool VolumesCopier::ReadChkptInfoFromChkptFile(void)
{
    LocalConfigurator localConfigurator;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_ChkptFile = m_ChkptOffsetDir + CheckPtFileName; //name of the check point offset file.
    SV_ULONGLONG const fileSize = File::GetNumberOfBytes(m_ChkptFile);

    std::string errmsg;
    m_ClusterSize = GetSourceClusterSize(errmsg);
    if (0 == m_ClusterSize)
    {
        m_ErrorMessage = std::string("VolumesCopier failed to get cluster size for source volume ") + m_CopyInfo.m_SourceVolumeName + 
                         " with error " + errmsg;
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG, "cluster size for source volume %s is %u\n", m_CopyInfo.m_SourceVolumeName.c_str(), m_ClusterSize);

    if (0 == fileSize)
    {
        SV_UINT  totalPartitions = 0; 
        SV_UINT partitionSize = localConfigurator.getDirectSyncPartitionSize();
        SV_ULONGLONG alignedBndryOffset = 0;

        // If partition size is invalid calculate the alignedBndryOffset from number of partitions.
        if(partitionSize <= 0)
        {
            totalPartitions = localConfigurator.getDirectSyncPartitions();
            if( 0 >= totalPartitions || DefaultPartitions < totalPartitions )
                totalPartitions = DefaultPartitions;

            // Make sure that start offset for read/write is always a boundry of cluster size
            // (which in turn is multiple of 512 on windows too and on linux)
            // man 2 open in Linux says "Under Linux 2.4 transfer sizes, 
            // and the alignment of user buffer and file offset must all be multiples of the 
            // logical block size of the file system." - It should be OK for Windows also.

            /* TODO: handle zero for alignedBndryOffset being 0 ? 
             * No need because, the except the last partition, 
             * all will have start, end and last wrote offsets as 0s */
            alignedBndryOffset = m_CopyInfo.m_SourceSize / (totalPartitions*m_ClusterSize);
            alignedBndryOffset *= m_ClusterSize;
        }
        else
        {
            SV_UINT quotient = partitionSize / m_ClusterSize;
            if ((quotient*m_ClusterSize) != partitionSize)
            {
                /* TODO: should this be WARNING ? */
                DebugPrintf(SV_LOG_ERROR, "For source %s, VolumesCopier Partition Size (%u) is not multiple of cluster size. "
                                          " The cluster size is %u. Moving a head by making it as nearest multiple upwards\n", 
                                          m_CopyInfo.m_SourceVolumeName.c_str(), partitionSize, m_ClusterSize);
                /* Upwards is done because to handle case where
                 * partition size is less than cluster size so that
                 * there is one partition always. */
                partitionSize = (m_ClusterSize * (quotient+1));
            }

            alignedBndryOffset = partitionSize;
            totalPartitions = (m_CopyInfo.m_SourceSize+partitionSize-1) / partitionSize;
        }
        DebugPrintf(SV_LOG_INFO, "VolumesCopier::ReadChkptInfoFromChkptFile Total partitions are (%u).\n", totalPartitions);
        DebugPrintf("alignedBndryOffset is " ULLSPEC "\n", alignedBndryOffset);

        /* to handle totalPartitions = 0 */
        SV_LONGLONG tp = totalPartitions;
        SV_LONGLONG index = 0;
        ChkptInfo ckptInfo;

        for (; index < tp - 1; ++index)
        {
            INM_SAFE_ARITHMETIC(ckptInfo.startOffset = index * InmSafeInt<SV_ULONGLONG>::Type(alignedBndryOffset), INMAGE_EX(index)(alignedBndryOffset))
            INM_SAFE_ARITHMETIC(ckptInfo.endOffset = InmSafeInt<SV_ULONGLONG>::Type(ckptInfo.startOffset)  + alignedBndryOffset, INMAGE_EX(ckptInfo.startOffset)(alignedBndryOffset))
            ckptInfo.lastWroteOffset = ckptInfo.startOffset;

            m_chkptInfos.push_back(ckptInfo);
        }

        // Make sure to include any leftover space after aligning the whole partitions to volume cluster size
        INM_SAFE_ARITHMETIC(ckptInfo.startOffset = index * InmSafeInt<SV_ULONGLONG>::Type(alignedBndryOffset), INMAGE_EX(index)(alignedBndryOffset))
        ckptInfo.endOffset = m_CopyInfo.m_SourceSize;
        ckptInfo.lastWroteOffset = ckptInfo.startOffset;

        m_chkptInfos.push_back(ckptInfo);

        DumpChkptInfo();
    }
    else
    {
        std::stringstream dataStr;
        BasicIo bio(m_ChkptFile, BasicIo::BioReadExisting | BasicIo::BioBinary);
        if (!bio.Good()) 
        {
            std::stringstream msg;
            msg << "VolumesCopier::ReadChkptInfoFromChkptFile() open checkpoint offset file " << m_ChkptFile << " failed: " << bio.LastError() << '\n';
            m_ErrorMessage = msg.str();
            return false;
        }

        size_t infolen;
        INM_SAFE_ARITHMETIC(infolen = InmSafeInt<SV_ULONGLONG>::Type(fileSize) + 1, INMAGE_EX(fileSize))
        boost::shared_array<char> info (new (std::nothrow) char[infolen]);

        if( !info )
        {
            m_ErrorMessage = "Error in VolumesCopier::ReadChkptInfoFromChkptFile() : insufficient memory to allocate buffers.";
            return false;
        }

        if (fileSize == bio.FullRead(info.get(), fileSize))
        {

            unsigned char hash[DIGEST_LEN];
            char * data  = info.get();
            data[ fileSize ] = '\0';

            INM_MD5_CTX ctx;
            INM_MD5Init(&ctx);
            INM_MD5Update(&ctx, (unsigned char*)data + DIGEST_LEN, fileSize - DIGEST_LEN);
            INM_MD5Final(hash, &ctx);
            /*
            char md5localcksum[INM_MD5TEXTSIGLEN + 1];

            // Get the string representation of the checksum
            for (int i = 0; i < INM_MD5TEXTSIGLEN/2; i++) {
            inm_sprintf_s((md5localcksum + i*2), (ARRAYSIZE(md5localcksum) - (i*2)), "%02X", hash[i]);
            }
            */
            if (0 !=  memcmp(data, hash, sizeof(hash)))
            {
                //Delete the  chkpt file & restart the resync
                std::stringstream msg;
                msg << "VolumesCopier::ReadChkptInfoFromChkptFile()- Check point offset file : "
                    << m_ChkptFile << " is corrupted. Deleting the file. Exiting and "
                    << "restarting the resync";
                m_ErrorMessage = msg.str();

                bio.Close();
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(m_ChkptFile.c_str()).c_str());
                /* TODO: there was setLastResyncOffsetForDirectSync call here. 
                 * setLastResyncOffsetForDirectSync(0);
                 * what is the equivalent ? */
                return false;
            }

            SV_UINT index = 0;

            SV_UINT totalPartitions = localConfigurator.getDirectSyncPartitions();

            data = data + DIGEST_LEN;
            dataStr << data;

            SV_UINT parts;
            dataStr >> parts;

            if ( parts != totalPartitions )
            {
                std::stringstream msg;
                msg << "directSyncPartitions is modified It will take effect only for next resync."
                    << " Partitions in checkpoint file are " << parts
                    << " Partitions in config are " << totalPartitions;
                DebugPrintf(SV_LOG_DEBUG,"%s\n", msg.str().c_str() );
            }

			dataStr >> m_totalFilesystemUnusedBytes;
			DebugPrintf(SV_LOG_DEBUG, "got file system unused bytes from check point file " ULLSPEC "\n", m_totalFilesystemUnusedBytes);

            m_chkptInfos.clear();
            ChkptInfo ckptInfo;
            for (; index < parts; ++index)
            {
                dataStr >>  ckptInfo.startOffset;
                dataStr >> ckptInfo.endOffset;
                dataStr >> ckptInfo.lastWroteOffset;
                m_chkptInfos.push_back(ckptInfo);
            }

            if (m_chkptInfos.size())
            {
                if (m_chkptInfos[0].endOffset % m_ClusterSize)
                {
                    bio.Close();
                    std::stringstream msg;
                    msg << "For source volume " << m_CopyInfo.m_SourceVolumeName 
                        << ", target volume " << m_CopyInfo.m_TargetVolumeName
                        << ", checkpoint file " << m_ChkptFile 
                        << " was created with old cluster size, since partition size in file "
                        << m_chkptInfos[0].endOffset << " is not multiple of current cluster size " << m_ClusterSize
                        << ". Deleting this old file. Copy for above volumes needs to be initiated again afresh, foregoing any previous progress.";
                    m_ErrorMessage = msg.str();
                    ACE_OS::unlink(getLongPathName(m_ChkptFile.c_str()).c_str());
                    return false;
                }
            }

            DumpChkptInfo();
        }
        else
        {
            std::stringstream msg;
            msg << "For source volume " << m_CopyInfo.m_SourceVolumeName 
                << ", target volume " << m_CopyInfo.m_TargetVolumeName
                << ", unable to read from check point file "
                << m_ChkptFile;
            m_ErrorMessage = msg.str();
            return false;
        }

    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return true;
}


SV_UINT VolumesCopier::GetSourceClusterSize(std::string &errmsg)
{
    /* TODO: check in ntfs fastsync for using 
     * raw or not ? */
    cdp_volume_t *pvolume = m_CopyInfo.m_VolumeInstantiator(GetRawVolumeName(m_CopyInfo.m_SourceVolumeName));
    if (!pvolume)
    {
        m_ErrorMessage = "Could not instantiate source volume.";
        return 0;
    }

    cdp_volume_util u;
    /* NOTE: open is needed to get cluster size */
    pvolume->Open(BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioDirect);
    if (!pvolume->Good())
    {
        std::stringstream msg;
        msg << "VolumesCopier::GetSourceClusterSize() : Failed to open source volume :"
            << pvolume->GetName()<< " error: " << pvolume->LastError() << '\n';
        m_ErrorMessage = msg.str();
        return 0;
    }

    SV_UINT size = u.GetClusterSize(*pvolume, m_CopyInfo.m_Filesystem, m_CopyInfo.m_SourceSize, m_CopyInfo.m_SourceStartOffset, 
                                    m_CopyInfo.m_FsSupportAction, m_ErrorMessage);
    m_CopyInfo.m_VolumeDestroyer(pvolume);
    return size;
}


void VolumesCopier::DumpChkptInfo(void)
{
    std::stringstream chkData;
    chkData << "StartOffset :"
        << "\t\t EndOffset:"
        << "\t\t LastWroteOffset\n";

    for (size_t i=0; i<m_chkptInfos.size(); ++i)
    {
        if(m_chkptInfos[i].endOffset != m_chkptInfos[i].lastWroteOffset && m_chkptInfos[i].startOffset != m_chkptInfos[i].lastWroteOffset)
        {
            chkData << i << " " << m_chkptInfos[i].startOffset
                << "\t\t"
                << m_chkptInfos[i].endOffset
                << "\t\t"
                << m_chkptInfos[i].lastWroteOffset
                << "\n";		
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Check Point Infos are :\n %s ", chkData.str().c_str());
}


VolumesCopier::CopyInfo::CopyInfo(const std::string &sourcevolumename, const std::string &targetvolumename, 
								 VolumeInstantiator_t &volumeinstantiator, VolumeDestroyer_t &volumedestroyer, 
								 const std::string &filesystem, const SV_ULONGLONG &sourcesize, 
								 const SV_ULONGLONG &sourcestartoffset, const bool &comparethencopy, 
								 const std::string &sessionnumber, const bool &pipelinereadwrite,
								 const unsigned int &iobuffercount, const unsigned int &iobuffersize,
								 const unsigned int &iobufferalignment, const E_FS_SUPPORT_ACTIONS &fssupportaction, 
								 ActionOnBytesCovered_t &actiononnobytescovered, ActionOnBytesCovered_t &actiononallbytescovered, 
								 ActionOnBytesCovered_t &actiononbytescovered, const unsigned int &copymonitorinterval, 
								 VolumeApplierInstantiator_t &volumeapplierinstantiator, VolumeApplierDestroyer_t &volumeapplierdestroyer, 
								 QuitFunction_t &quitfunction, const ReadInfo::ReadRetryInfo_t &sourcereadretryinfo, 
								 const bool &useunbufferediofortarget, const unsigned char &applypercentchangeforaction, 
								 cdp_volume_util::ClusterBitmapCustomizationInfos_t *pcbcustinfos, const SV_UINT &lengthforfilesystemclustersquery, 
								 const std::string &directoryforinternaloperations, const bool &profile) :
 m_SourceVolumeName(sourcevolumename),
 m_TargetVolumeName(targetvolumename),
 m_VolumeInstantiator(volumeinstantiator),
 m_VolumeDestroyer(volumedestroyer),
 m_Filesystem(filesystem),
 m_SourceSize(sourcesize),
 m_SourceStartOffset(sourcestartoffset),
 m_CompareThenCopy(comparethencopy),
 m_SessionNumber(sessionnumber),
 m_PipelineReadWrite(pipelinereadwrite),
 m_IOBufferCount(iobuffercount),
 m_IOBufferSize(iobuffersize),
 m_IOBufferAlignment(iobufferalignment),
 m_FsSupportAction(fssupportaction),
 m_ActionOnNoBytesCovered(actiononnobytescovered),
 m_ActionOnAllBytesCovered(actiononallbytescovered),
 m_ActionOnBytesCovered(actiononbytescovered),
 m_CopyMonitorInterval(copymonitorinterval),
 m_VolumeApplierInstantiator(volumeapplierinstantiator),
 m_VolumeApplierDestroyer(volumeapplierdestroyer),
 m_QuitFunction(quitfunction),
 m_SourceReadRetryInfo(sourcereadretryinfo),
 m_UseUnBufferedIoForTarget(useunbufferediofortarget),
 m_ApplyPercentChangeForAction(applypercentchangeforaction),
 m_pClusterBitmapCustomizationInfos(pcbcustinfos),
 m_LengthForFileSystemClustersQuery(lengthforfilesystemclustersquery),
 m_DirectoryForInternalOperations(directoryforinternaloperations),
 m_Profile(profile)
{
}


VolumesCopier::CopyInfo::CopyInfo(const CopyInfo &rhs)
 : m_SourceVolumeName(rhs.m_SourceVolumeName),
   m_TargetVolumeName(rhs.m_TargetVolumeName),
   m_VolumeInstantiator(rhs.m_VolumeInstantiator),
   m_VolumeDestroyer(rhs.m_VolumeDestroyer),
   m_Filesystem(rhs.m_Filesystem),
   m_SourceSize(rhs.m_SourceSize),
   m_SourceStartOffset(rhs.m_SourceStartOffset),
   m_CompareThenCopy(rhs.m_CompareThenCopy),
   m_SessionNumber(rhs.m_SessionNumber),
   m_PipelineReadWrite(rhs.m_PipelineReadWrite),
   m_IOBufferCount(rhs.m_IOBufferCount),
   m_IOBufferSize(rhs.m_IOBufferSize),
   m_IOBufferAlignment(rhs.m_IOBufferAlignment),
   m_FsSupportAction(rhs.m_FsSupportAction),
   m_ActionOnNoBytesCovered(rhs.m_ActionOnNoBytesCovered),
   m_ActionOnAllBytesCovered(rhs.m_ActionOnAllBytesCovered),
   m_ActionOnBytesCovered(rhs.m_ActionOnBytesCovered),
   m_CopyMonitorInterval(rhs.m_CopyMonitorInterval),
   m_VolumeApplierInstantiator(rhs.m_VolumeApplierInstantiator),
   m_VolumeApplierDestroyer(rhs.m_VolumeApplierDestroyer),
   m_QuitFunction(rhs.m_QuitFunction),
   m_SourceReadRetryInfo(rhs.m_SourceReadRetryInfo),
   m_UseUnBufferedIoForTarget(rhs.m_UseUnBufferedIoForTarget),
   m_ApplyPercentChangeForAction(rhs.m_ApplyPercentChangeForAction),
   m_pClusterBitmapCustomizationInfos(rhs.m_pClusterBitmapCustomizationInfos),
   m_LengthForFileSystemClustersQuery(rhs.m_LengthForFileSystemClustersQuery),
   m_DirectoryForInternalOperations(rhs.m_DirectoryForInternalOperations),
   m_Profile(rhs.m_Profile)
{
}


bool VolumesCopier::CopyInfo::IsValid(std::string &errormessage)
{
    bool isvalid = true;

    if (m_SourceVolumeName.empty())
    {
        isvalid = false;
        errormessage = "No source volume supplied.";
    }

    if (m_TargetVolumeName.empty())
    {
        isvalid = false;
        errormessage += " No target volume supplied.";
    }

    if (!m_VolumeInstantiator)
    {
        isvalid = false;
        errormessage += " No volume instantiator function specified.";
    }

    if (!m_VolumeDestroyer)
    {
        isvalid = false;
        errormessage += " No volume destroyer function specified.";
    }

    if (0 == m_SourceSize)
    {
        isvalid = false;
        errormessage += " Source volume size is zero.";
    }

	if (0 == m_IOBufferCount)
	{
		isvalid = false;
		errormessage += " IO buffer count is zero.";
	}

    if (0 == m_IOBufferSize)
    {
        isvalid = false;
        errormessage += " IO buffer size is zero.";
    }

    if (!m_ActionOnNoBytesCovered)
    {
        isvalid = false;
        errormessage += " Action for no bytes covered unspecified.";
    }

    if (!m_ActionOnAllBytesCovered)
    {
        isvalid = false;
        errormessage += " Action for all bytes covered unspecified.";
    }

    if (!m_ActionOnBytesCovered)
    {
        isvalid = false;
        errormessage += " Action for bytes covered unspecified.";
    }

    if (0 == m_CopyMonitorInterval)
    {
        isvalid = false;
        errormessage += " Copy monitor interval unspecified.";
    }

    if (!m_VolumeApplierInstantiator)
    {
        isvalid = false;
        errormessage += " Volume applier instantiator unspecified.";
    }

    if (!m_VolumeApplierDestroyer)
    {
        isvalid = false;
        errormessage += " Volume applier destroyer unspecified.";
    }

    if (!m_QuitFunction)
    {
        isvalid = false;
        errormessage += " Quit function unspecified.";
    }

    if ((m_ApplyPercentChangeForAction==0) || (m_ApplyPercentChangeForAction>100))
    {
        isvalid = false;
        errormessage += " Apply percentage change for covered bytes action is improper.";
    }

    if (0 == m_pClusterBitmapCustomizationInfos)
    {
        isvalid = false;
        errormessage += " Cluster bitmap customization information not supplied.";
    }

	if (0 == m_LengthForFileSystemClustersQuery)
	{
		isvalid = false;
		errormessage += " Length for file system clusters query is zero.";
	}

	if (m_DirectoryForInternalOperations.empty())
	{
		isvalid = false;
		errormessage += " Director for internal operations is not provided.";
	}
    
    return isvalid;
}


VolumesCopier::CoveredOffsetInfo::CoveredOffsetInfo(const SV_UINT &partitionid, const SV_ULONGLONG &offset, const SV_ULONGLONG &filesystemunusedbytes)
: m_PartitionID(partitionid),
  m_Offset(offset),
  m_FilesystemUnusedBytes(filesystemunusedbytes)
{
}


VolumesCopier::CoveredBytesInfo::CoveredBytesInfo(const SV_ULONGLONG &bytescovered, const SV_ULONGLONG &filesystemunusedbytes)
: m_BytesCovered(bytescovered),
  m_FilesystemUnusedBytes(filesystemunusedbytes)
{
}


VolumesCopier::ReaderWriterBuffer::ReaderWriterBuffer()
 : m_pVci(0),
   m_PartID(0),
   m_Offset(0),
   m_Length(0),
   m_SourcePhysicalOffset(0)
{
}


void VolumesCopier::ReaderWriterBuffer::Reset(void)
{
    m_pVci = 0;
    m_ReadInfo.Reset();
    m_PartID = 0;
    m_Offset = 0;
    m_Length = 0;
    m_SourcePhysicalOffset = 0;
}


bool VolumesCopier::ReaderWriterBuffer::Init(const SV_UINT &buflen, const SV_UINT &alignment)
{
    return m_ReadInfo.Init(buflen, alignment);
}


void VolumesCopier::ReaderWriterBuffer::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "Reader Writer  Buffer:\n");
    DebugPrintf(SV_LOG_DEBUG, "volume cluster information address: %p\n", m_pVci);
    DebugPrintf(SV_LOG_DEBUG, "partition id: %u\n", m_PartID);
    DebugPrintf(SV_LOG_DEBUG, "offset: " LLSPEC "\n", m_Offset);
    DebugPrintf(SV_LOG_DEBUG, "length: %u\n", m_Length);
    DebugPrintf(SV_LOG_DEBUG, "source physical offset: " LLSPEC "\n", m_SourcePhysicalOffset);

    /* TODO: printing this is not needed
    if (m_pVci)
    {
        m_pVci->Print();
    }
    */

    m_ReadInfo.Print();
}


VolumesCopier::ReaderWriterBuffer::~ReaderWriterBuffer()
{
}


VolumesCopier::ReaderWriterBuffers::ReaderWriterBuffers()
 : m_pReaderWriterBuffers(0),
   m_NumberOfBuffers(0),
   m_FreeIndex(0)
{
}


bool VolumesCopier::ReaderWriterBuffers::Init(const unsigned int &nbufs, const SV_UINT &buflen, const SV_UINT &alignment)
{
    bool binitialized = false;

    Release();
    m_pReaderWriterBuffers = new (std::nothrow) ReaderWriterBuffer[nbufs];
    if (m_pReaderWriterBuffers)
    {
        for (unsigned int i = 0; i < nbufs; i++)
        {
            binitialized = m_pReaderWriterBuffers[i].Init(buflen, alignment);
            if (!binitialized)
            {
                DebugPrintf(SV_LOG_ERROR, "could not initialize reader writer  buffer\n");
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate reader writer  buffers\n");
    }

    if (binitialized)
    {
        m_NumberOfBuffers = nbufs;
    }

    return binitialized;
}


void VolumesCopier::ReaderWriterBuffers::Release(void)
{
    if (m_pReaderWriterBuffers)
    {
        delete [] m_pReaderWriterBuffers;
        m_pReaderWriterBuffers = 0;
    }
}


VolumesCopier::ReaderWriterBuffers::~ReaderWriterBuffers()
{
    Release();
    m_NumberOfBuffers = 0;
}
