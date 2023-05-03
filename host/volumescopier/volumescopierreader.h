#ifndef VOLUMES_COPIER_READER_H
#define VOLUMES_COPIER_READER_H

#include <iostream>
#include <vector>
#include <iterator>

#include "ace/Time_Value.h"

#include "volumescopier.h"
#include "readinfo.h"
#include "inmpipeline.h"
#include "sharedalignedbuffer.h"
#include "cdpvolumeutil.h"
#include "portablehelpers.h"
#include "volumescopierwriter.h"


class VolumesCopierReaderStage : public InmPipeline::Stage
{
public:
    struct CopyInfo
    {
        std::string m_SourceVolumeName;
        VolumesCopier::VolumeInstantiator_t m_VolumeInstantiator;
        VolumesCopier::VolumeDestroyer_t m_VolumeDestroyer;
        std::string m_Filesystem;
        SV_ULONGLONG m_SourceSize;
        SV_ULONGLONG m_SourceStartOffset;
        bool m_PipelineReadWrite;
		unsigned int m_ReadBufferCount;
        unsigned int m_ReadBufferSize;
		unsigned int m_ReadBufferAlignment;
        E_FS_SUPPORT_ACTIONS m_FsSupportAction;
        ReadInfo::ReadRetryInfo_t m_SourceReadRetryInfo;
        cdp_volume_util::ClusterBitmapCustomizationInfos_t * m_pClusterBitmapCustomizationInfos;
		SV_UINT m_LengthForFileSystemClustersQuery;
		bool m_Profile;
        VolumesCopier::ChkptInfos_t *m_pChkptInfos;
        VolumesCopier *m_pVolumesCopier;
         
        CopyInfo(const std::string &sourcevolumename, VolumesCopier::VolumeInstantiator_t &volumeinstantiator,
                 VolumesCopier::VolumeDestroyer_t &volumedestroyer, const std::string &filesystem, 
                 const SV_ULONGLONG &sourcesize, const SV_ULONGLONG &sourcestartoffset,
                 const bool &pipelinereadwrite, const unsigned int &readbuffercount,
				 const unsigned int &readbuffersize, const unsigned int &readbufferalignment,
                 const E_FS_SUPPORT_ACTIONS &fssupportaction, 
                 const ReadInfo::ReadRetryInfo_t &sourcereadretryinfo,
                 cdp_volume_util::ClusterBitmapCustomizationInfos_t * pcbcustinfos,
				 const SV_UINT &lengthforfilesystemclustersquery,
				 const bool &profile, VolumesCopier::ChkptInfos_t *pchkptinfos, 
                 VolumesCopier *pvolumescopier);
        CopyInfo(const CopyInfo &rhs);
        bool IsValid(std::ostream &error);
    };

    VolumesCopierReaderStage(const CopyInfo &rci);
    ~VolumesCopierReaderStage();
    int svc(void);
	ACE_Time_Value GetTimeForRead(void);
    void SetWriter(VolumesCopierWriterStage *pwriter);

private:
    bool Init(std::ostream &excmsg);
    bool AllocateVolumeClusterInfos(const SV_UINT &nvcis);
    void ReleaseVolumeClusterInfos(void);
    void InformStatus(const std::string &s);
    void Copy(void);
    bool GetClusterBitmapIfRequired(const SV_LONGLONG startoffset,
                                    const SV_UINT &lengthtocheck,
                                    const SV_ULONGLONG &prereadlength,
                                    const SV_UINT &maxlength);

    typedef bool (VolumesCopierReaderStage::*QuitRequested_t)(long int seconds);
    typedef VolumesCopier::ReaderWriterBuffer_t * (VolumesCopierReaderStage::*GetReadWriteBuffer_t)(void);
    typedef void (VolumesCopierReaderStage::*SetCurrentVci_t)(void);
    typedef void (VolumesCopierReaderStage::*SendReadWriteBuffer_t)(VolumesCopier::ReaderWriterBuffer_t *prwbuf);

    bool QuitRequested(long int seconds = 0);
    bool QuitRequestedInApp(long int seconds);
    bool QuitRequestedInStage(long int seconds);

    VolumesCopier::ReaderWriterBuffer_t * GetReadWriteBuffer(void);
    VolumesCopier::ReaderWriterBuffer_t * GetReadWriteBufferInApp(void);
    VolumesCopier::ReaderWriterBuffer_t * GetReadWriteBufferInStage(void);

    void SetCurrentVci(void);
    void SetCurrentVciInApp(void);
    void SetCurrentVciInStage(void);

    void SendReadWriteBuffer(VolumesCopier::ReaderWriterBuffer_t *prwbuf);
    void SendReadWriteBufferInApp(VolumesCopier::ReaderWriterBuffer_t *prwbuf);
    void SendReadWriteBufferInStage(VolumesCopier::ReaderWriterBuffer_t *prwbuf);

	typedef bool (VolumesCopierReaderStage::*Read_t)(ReadInfo &rif, ReadInfo::ReadInputs_t &ri);
	bool NoProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri);
	bool ProfileRead(ReadInfo &rif, ReadInfo::ReadInputs_t &ri);

private:
    CopyInfo m_CopyInfo;
    SV_ULONGLONG m_StartOffset;
    SV_ULONGLONG m_EndOffset;
    SV_UINT m_PartID;
    cdp_volume_t *m_pSourceVolume;
    SV_UINT m_ClusterSize;
    VolumeClusterInformer::VolumeClusterInfo *m_pVcis;
    VolumeClusterInformer::VolumeClusterInfo *m_pCurrentVci;
    VolumesCopier::ReaderWriterBuffers_t m_RWBufs;
    cdp_volume_util m_CdpVolumeUtil;
	ACE_Time_Value m_TimeForRead;
    QuitRequested_t m_QuitRequest[NBOOLS];
    GetReadWriteBuffer_t m_RWBufGetter[NBOOLS];
    SendReadWriteBuffer_t m_RWBufSender[NBOOLS];
    VolumesCopierWriterStage *m_pVolumesCopierWriterStage;
	Read_t m_Read[NBOOLS];
    SetCurrentVci_t m_SetCurrentVci[NBOOLS];
};

#endif /* VOLUMES_COPIER_READER_H */
