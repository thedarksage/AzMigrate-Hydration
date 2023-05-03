#ifndef VOLUMES_COPIER_WRITER_H
#define VOLUMES_COPIER_WRITER_H

#include <iostream>
#include <vector>
#include <iterator>

#include "ace/Time_Value.h"

#include "volumescopier.h"
#include "inmpipeline.h"
#include "sharedalignedbuffer.h"
#include "localconfigurator.h"
#include "cdpvolumeutil.h"
#include "portablehelpers.h"
#include "readinfo.h"

/* For the resync case, this also
 * does target read and checksum 
 * compare. A separate checksum computation
 * stage should not be used because, read and
 * write on target may be parallel leading to
 * reduced throughput */
class VolumesCopierWriterStage : public InmPipeline::Stage
{
public:

    struct CopyInfo
    {
        std::string m_TargetVolumeName;
        VolumesCopier::VolumeInstantiator_t m_VolumeInstantiator;
        VolumesCopier::VolumeDestroyer_t m_VolumeDestroyer;
        bool m_CompareThenCopy;
        bool m_PipelineReadWrite;
        unsigned int m_ReadBufferSize;
		unsigned int m_ReadBufferAlignment;
        VolumesCopier::VolumeApplierInstantiator_t m_VolumeApplierInstantiator;
        VolumesCopier::VolumeApplierDestroyer_t m_VolumeApplierDestroyer;
        VolumesCopier::UpdateCoveredOffset_t m_UpdateCoveredOffset;
        bool m_UseUnBufferedIoForTarget;
		bool m_Profile;
        VolumesCopier::ChkptInfos_t *m_pChkptInfos;
        VolumesCopier *m_pVolumesCopier;
        SV_UINT m_ClusterSize;

        CopyInfo(const std::string &targetvolumename,
                 VolumesCopier::VolumeInstantiator_t &volumeinstantiator,
                 VolumesCopier::VolumeDestroyer_t &volumedestroyer,
                 const bool &comparethencopy, const bool &pipelinereadwrite,
                 const unsigned int &readbuffersize, const unsigned int &readbufferalignment,
                 VolumesCopier::VolumeApplierInstantiator_t &volumeapplierinstantiator,
                 VolumesCopier::VolumeApplierDestroyer_t &volumeapplierdestroyer, 
                 VolumesCopier::UpdateCoveredOffset_t &updatecoveredoffset, 
                 const bool &useunbufferediofortarget,
                 const bool &profile, VolumesCopier::ChkptInfos_t *pchkptinfos,
                 VolumesCopier *pvolumescopier, const SV_UINT &clusterzize);
        CopyInfo(const CopyInfo &rhs);
        bool IsValid(std::ostream &error);
    };

    VolumesCopierWriterStage(const CopyInfo &wci);
    ~VolumesCopierWriterStage();
    bool Init(std::ostream &excmsg);
    void Flush(void);
    int svc(void);
    void Process(VolumesCopier::ReaderWriterBuffer_t *prwbuf);
	ACE_Time_Value GetTimeForApply(void);
	ACE_Time_Value GetTimeForApplyOverhead(void);
	ACE_Time_Value GetTimeForTargetRead(void);
    ACE_Time_Value GetTimeForChecksumsComputeAndCompare(void);

private:

    typedef bool (VolumesCopierWriterStage::*ConsumeSourceData_t)(const SV_LONGLONG *offset,    /* logical one not physical */
                                                      const SV_UINT &length,        /* length of data */
                                                      char *data,
                                                      std::string &errmsg);

	typedef bool (VolumesCopierWriterStage::*Read_t)(ReadInfo &rif, ReadInfo::ReadInputs_t &ri);
	typedef bool (VolumesCopierWriterStage::*ChecksumsMatch_t)(char *sourcedata, char *targetdata, const SV_UINT &length);

    bool ConsumeSourceDataForDirectApply(const SV_LONGLONG *offset,    /* logical one not physical */
                                         const SV_UINT &length,        /* length of data */
                                         char *data,
                                         std::string &errmsg);
    bool ConsumeSourceDataForComparision(const SV_LONGLONG *offset,    /* logical one not physical */
                                         const SV_UINT &length,        /* length of data */
                                         char *data,
                                         std::string &errmsg);
	bool NoProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri);
	bool ProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri);

	bool DoChecksumsMatch(char *sourcedata, char *targetdata, const SV_UINT &length);
	bool NoProfileChecksumsMatch(char *sourcedata, char *targetdata, const SV_UINT &length);
	bool ProfileChecksumsMatch(char *sourcedata, char *targetdata, const SV_UINT &length);
     
    typedef bool (VolumesCopierWriterStage::*QuitRequested_t)(long int seconds);

    bool QuitRequested(long int seconds = 0);
    bool QuitRequestedInApp(long int seconds);
    bool QuitRequestedInStage(long int seconds);

    void SendBackReadWriteBuffer(VolumesCopier::ReaderWriterBuffer_t *prwbuf);

private:
    CopyInfo m_CopyInfo;
    SV_UINT m_PartID;
    cdp_volume_t *m_pTargetVolume;
    VolumeApplier *m_pVolumeApplier;
    cdp_volume_util m_CdpVolumeUtil;
    ReadInfo m_ReadInfo;                               /* for target */
	ACE_Time_Value m_TimeForTargetRead;
	ACE_Time_Value m_TimeForApply;
	ACE_Time_Value m_TimeForApplyOverhead;
	ACE_Time_Value m_TimeForChecksumsComputeAndCompare;
    QuitRequested_t m_QuitRequest[NBOOLS];
    SV_ULONGLONG m_ProcessedBuffersCountInPartition;
	SV_ULONGLONG m_FilesystemUnusedBytesInPartition;
    SV_LONGLONG m_CoveredOffset;
    ConsumeSourceData_t m_ConsumeSourceData[NBOOLS];
	Read_t m_Read[NBOOLS];
	ChecksumsMatch_t m_ChecksumsMatch[NBOOLS];
};


#endif /* VOLUMES_COPIER_WRITER_H */
