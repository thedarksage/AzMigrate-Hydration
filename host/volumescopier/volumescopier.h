#ifndef VOLUMES_COPIER_H
#define VOLUMES_COPIER_H

#include <string>
#include <ace/Guard_T.h>
#include <ace/Thread_Mutex.h>
#include <ace/Recursive_Thread_Mutex.h>
#include <ace/Time_Value.h>
#include <boost/function.hpp>
#include "inmquitfunction.h"
#include "svtypes.h"
#include "cdpvolume.h"
#include "cdpvolumeutil.h"
#include "inmpipeline.h"
#include "fssupportactions.h"
#include "volumeapplier.h"
#include "readinfo.h"
#include "volumeclusterinfo.h"


const int WAITSECS = 5;

class VolumesCopier
{
public:
    typedef struct ReaderWriterBuffer
    {
        VolumeClusterInformer::VolumeClusterInfo *m_pVci;
        ReadInfo m_ReadInfo;     /* m_ReadInfo.length == 0 indicates that below offset to length is unused */
        SV_UINT m_PartID;
        SV_LONGLONG m_Offset;    /* carries the logical offset; These have to be used at target for all calculations, not read info */
        SV_UINT m_Length;
        SV_LONGLONG m_SourcePhysicalOffset;    /* needed to query for continuouse used clusters; To avoid add start offset 
                                                * operation in writer, this is needed */

        ReaderWriterBuffer();
        void Reset(void);
        bool Init(const SV_UINT &buflen, const SV_UINT &alignment); 
        void Print(void);
        ~ReaderWriterBuffer();

    } ReaderWriterBuffer_t;

    typedef struct ReaderWriterBuffers
    {
        VolumesCopier::ReaderWriterBuffer_t *m_pReaderWriterBuffers;
        unsigned int m_NumberOfBuffers;
        unsigned int m_FreeIndex;

        ReaderWriterBuffers();
        void Release(void);
        bool Init(const unsigned int &nbufs, const SV_UINT &buflen, const SV_UINT &alignment);
        ~ReaderWriterBuffers();

    } ReaderWriterBuffers_t;

    typedef struct CoveredOffsetInfo
    {
        SV_UINT m_PartitionID;
        SV_ULONGLONG m_Offset;
		SV_ULONGLONG m_FilesystemUnusedBytes;

        CoveredOffsetInfo(const SV_UINT &partitionid, const SV_ULONGLONG &offset, const SV_ULONGLONG &filesystemunusedbytes);

    } CoveredOffsetInfo_t;

	typedef struct CoveredBytesInfo
	{
		SV_ULONGLONG m_BytesCovered;
		SV_ULONGLONG m_FilesystemUnusedBytes;

        CoveredBytesInfo(const SV_ULONGLONG &bytescovered, const SV_ULONGLONG &filesystemunusedbytes);

	} CoveredBytesInfo_t;

    typedef boost::function<cdp_volume_t * (const std::string volumename)> VolumeInstantiator_t;
    typedef boost::function<void (cdp_volume_t *pvolume)> VolumeDestroyer_t;
	typedef boost::function<bool (const CoveredBytesInfo_t coveredbytesinfo)> ActionOnBytesCovered_t;
	typedef boost::function<VolumeApplier * (VolumeApplier::VolumeApplierFormationInputs_t inputs)> VolumeApplierInstantiator_t;
    typedef boost::function<void (VolumeApplier *pvolumeapplier)> VolumeApplierDestroyer_t;
    typedef boost::function<void (const CoveredOffsetInfo_t coveredoffsetinfo)> UpdateCoveredOffset_t;

    struct CopyInfo
    {
        std::string m_SourceVolumeName;
        std::string m_TargetVolumeName;
        VolumeInstantiator_t m_VolumeInstantiator;
        VolumeDestroyer_t m_VolumeDestroyer;
        std::string m_Filesystem;
        SV_ULONGLONG m_SourceSize;
        SV_ULONGLONG m_SourceStartOffset;
        bool m_CompareThenCopy;
        std::string m_SessionNumber;
        bool m_PipelineReadWrite;
		unsigned int m_IOBufferCount;
        unsigned int m_IOBufferSize;
		unsigned int m_IOBufferAlignment;
        E_FS_SUPPORT_ACTIONS m_FsSupportAction;
        ActionOnBytesCovered_t m_ActionOnNoBytesCovered;
        ActionOnBytesCovered_t m_ActionOnAllBytesCovered;
        ActionOnBytesCovered_t m_ActionOnBytesCovered;
        unsigned int m_CopyMonitorInterval;
        VolumeApplierInstantiator_t m_VolumeApplierInstantiator;
        VolumeApplierDestroyer_t m_VolumeApplierDestroyer;
        QuitFunction_t m_QuitFunction;
        ReadInfo::ReadRetryInfo_t m_SourceReadRetryInfo;
        bool m_UseUnBufferedIoForTarget;
        unsigned char m_ApplyPercentChangeForAction;
        cdp_volume_util::ClusterBitmapCustomizationInfos_t * m_pClusterBitmapCustomizationInfos;
		SV_UINT m_LengthForFileSystemClustersQuery;    /* independent of using or not the actual file system clusters, this has to be supplied 
													    * as the copy algorithm always uses clusters (for non ntfs, all cluster bits are
													    * made as in use) */
		std::string m_DirectoryForInternalOperations;  /* this is where check point file is created */
		bool m_Profile;

        CopyInfo(const std::string &sourcevolumename, const std::string &targetvolumename, 
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
				 const std::string &directoryforinternaloperations, const bool &profile);
        CopyInfo(const CopyInfo &rhs);
        bool IsValid(std::string &errormessage);
    };

    typedef struct ChkptInfo
    {
        SV_ULONGLONG startOffset;
        SV_ULONGLONG endOffset;
        SV_ULONGLONG lastWroteOffset;
    } ChkptInfo;
    typedef std::vector<ChkptInfo> ChkptInfos_t;
    typedef ChkptInfos_t::const_iterator ConstChkptInfosIter_t;
    typedef ChkptInfos_t::iterator ChkptInfosIter_t;

public:
    ChkptInfos_t m_chkptInfos;

public:
    VolumesCopier(const CopyInfo &ci);
    ~VolumesCopier();
    bool Init(void);
    bool Copy(void);
    std::string GetErrorMessage(void);
	ACE_Time_Value GetTimeForSourceRead(void);
	ACE_Time_Value GetTimeForTargetRead(void);
	ACE_Time_Value GetTimeForApply(void);
	ACE_Time_Value GetTimeForApplyOverhead(void);
	ACE_Time_Value GetTimeForChecksumsComputeAndCompare(void);
	ACE_Time_Value GetWriterWaitTimeForBuffer(void);
	ACE_Time_Value GetReaderWaitTimeForBuffer(void);
    bool ShouldQuit(long int seconds);
    void InformError(const std::string &s, const int &waitsecs);
    bool PostMsg(const char *msg, const size_t &size, const int &waitsecs);

public:
    void updateInMomoryChkptInfo(const CoveredOffsetInfo_t coveredoffsetinfo);

private:
    /* worker is either a pipeline or volumes copier task */
    typedef State_t (VolumesCopier::*GetStateOfWorker_t)(void);

    bool CreateChkptOffsetDir(void);
    bool ReadChkptInfoFromChkptFile(void);
    SV_UINT GetSourceClusterSize(std::string &errmsg);
    void DumpChkptInfo(void);
    bool WriteChkptInfoToChkptFile(void);
    bool WaitForCopy(void);
    State_t GetStateOfPipeline(void);
    State_t GetStateOfTask(void);
    State_t GetStateOfWorker(void);
    bool StartAndWaitForWorker(void);

private:
    CopyInfo m_CopyInfo;
    InmPipeline *m_pInmPipeline;
    std::string m_ChkptOffsetDir;
    std::string m_ChkptFile;
    SV_ULONGLONG m_lastUpdatedOffset;
    SV_ULONGLONG m_totalCoveredBytes;
	SV_ULONGLONG m_totalFilesystemUnusedBytes;
    ACE_Recursive_Thread_Mutex m_Lock;
    SV_ULONGLONG m_CoveredBytesChangeForAction;
	ACE_Time_Value m_TimeForSourceRead;
	ACE_Time_Value m_TimeForTargetRead;
	ACE_Time_Value m_TimeForApply;
	ACE_Time_Value m_TimeForApplyOverhead;
	ACE_Time_Value m_TimeForChecksumsComputeAndCompare;
	ACE_Time_Value m_WriterWaitTimeForBuffer;
	ACE_Time_Value m_ReaderWaitTimeForBuffer;
    ACE_Thread_Manager m_ThreadManager;
    GetStateOfWorker_t m_GetStateOfWorker[2];
    ACE_Message_Queue<ACE_MT_SYNCH> m_QForTaskState;
    bool m_ShouldQuit;
    SV_UINT m_ClusterSize;
    std::string m_ErrorMessage;
};

#endif /* VOLUMES_COPIER_H */
