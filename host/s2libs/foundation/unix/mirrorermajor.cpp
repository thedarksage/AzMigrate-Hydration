#include <cstdio>
#include <set>
#include <string>
#include <sstream>
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "mirrorermajor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "deviceidinformer.h"
#include "inmsafecapis.h"

MirrorerMajor::MirrorerMajor()
{
    InitMirrorConfInfo();
}


MirrorerMajor::~MirrorerMajor()
{
    DeInitMirrorConfInfo();
}


void MirrorerMajor::InitMirrorConfInfo(void)
{
    m_MirrorConfInfo.src_guid_list = NULL;
    m_MirrorConfInfo.dst_guid_list = NULL;
}


void MirrorerMajor::DeInitMirrorConfInfo(void)
{
    if (m_MirrorConfInfo.src_guid_list)
    {
        free(m_MirrorConfInfo.src_guid_list);
        m_MirrorConfInfo.src_guid_list = NULL;
    }

    if (m_MirrorConfInfo.dst_guid_list)
    {
        free(m_MirrorConfInfo.dst_guid_list);
        m_MirrorConfInfo.dst_guid_list = NULL;
    }
}


void MirrorerMajor::RemoveMirrorSetupInput(void)
{
    DeInitMirrorConfInfo();
}


int MirrorerMajor::FormMirrorSetupInput(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                                        const ATLunNames_t &atlunnames,
                                        const VolumeReporter::VolumeReport_t &vr,
                                        PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror)
{
    int iStatus = SV_FAILURE;
    std::stringstream msg;

    DeInitMirrorConfInfo();
    memset(&m_MirrorConfInfo, 0, sizeof(mirror_conf_info_t));

    bool bissrcattrsfilled = FillSrcAttrsInSetupInput(prismvolumeinfo, vr);
    std::string atid;
    if (bissrcattrsfilled)
    {
        m_MirrorConfInfo.d_type = FILTER_DEV_MIRROR_SETUP;
        if (PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING_RESYNC_CLEARED == prismvolumeinfo.mirrorState)
        {
            m_MirrorConfInfo.d_flags |= MIRROR_SETUP_PENDING_RESYNC_CLEARED_FLAG;
        }
        m_MirrorConfInfo.d_bsize = VOL_BSIZE;
        m_MirrorConfInfo.d_nblks = prismvolumeinfo.sourceLUNCapacity / m_MirrorConfInfo.d_bsize;
        m_MirrorConfInfo.nsources = prismvolumeinfo.sourceLUNNames.size();
        m_MirrorConfInfo.ndestinations = atlunnames.size();
        /* TODO: this is not done / 512 for now */
        m_MirrorConfInfo.startoff = prismvolumeinfo.sourceLUNStartOffset;

        const ATLunNames_t::const_iterator beginatiter = atlunnames.begin();
        const std::string &firstatlun = *beginatiter;
        DebugPrintf(SV_LOG_DEBUG, "Trying to find LUN ID of first AT Lun %s\n", firstatlun.c_str());
        DeviceIDInformer f;
        atid = f.GetID(firstatlun);
        if (atid.empty())
        {
            iStatus = ERR_AGAIN;
            *pmirrorerror = PRISM_VOLUME_INFO::AtlunIdFailed;
            msg << "could not get device id for at lun " << firstatlun << " with error " << f.GetErrorMessage();
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Lun ID is %s for first AT Lun %s\n", atid.c_str(), firstatlun.c_str());
            if (m_MirrorConfInfo.nsources && m_MirrorConfInfo.ndestinations)
            {
                m_MirrorConfInfo.src_guid_list = (char *)calloc(m_MirrorConfInfo.nsources, INM_GUID_LEN_MAX);
                if (m_MirrorConfInfo.src_guid_list)
                {
                    m_MirrorConfInfo.dst_guid_list = (char *)calloc(m_MirrorConfInfo.ndestinations, INM_GUID_LEN_MAX);
                    if (m_MirrorConfInfo.dst_guid_list)
                    {
                        iStatus = SV_SUCCESS;
                    }
                    else
                    {
                        iStatus = ERR_AGAIN;
                        *pmirrorerror = PRISM_VOLUME_INFO::AgentMemErr;
                        msg << "memory allocation failed for giving at lun list to involflt driver";
                    }
                }
                else
                {
                    iStatus = ERR_AGAIN;
                    *pmirrorerror = PRISM_VOLUME_INFO::AgentMemErr;
                    msg << "memory allocation failed for giving source list to involflt driver";
                }
            }
            else
            {
                *pmirrorerror = m_MirrorConfInfo.nsources ? PRISM_VOLUME_INFO::AtlunInvalid : PRISM_VOLUME_INFO::SrcLunInvalid;
                msg << "number of " 
                    << (m_MirrorConfInfo.nsources ? "at luns" : "sources") 
                    << " is zero";
            }
        }
    }
    else
    {
        *pmirrorerror = PRISM_VOLUME_INFO::SrcLunInvalid;
        msg << "could not fill source luns attributes";
    }
   
    if (SV_SUCCESS == iStatus)
    {
        std::list<std::string>::const_iterator siter = prismvolumeinfo.sourceLUNNames.begin();
        for (char *p = m_MirrorConfInfo.src_guid_list; siter != prismvolumeinfo.sourceLUNNames.end(); siter++, p += INM_GUID_LEN_MAX)
        {
            const std::string &src = *siter;
            inm_strncpy_s(p, INM_GUID_LEN_MAX-1, src.c_str(), src.length());
        }
    
        ATLunNames_t::const_iterator diter = atlunnames.begin();
        for (char *p = m_MirrorConfInfo.dst_guid_list; diter != atlunnames.end(); diter++, p += INM_GUID_LEN_MAX)
        {
            const std::string &dst = *diter;
            inm_strncpy_s(p, INM_GUID_LEN_MAX-1, dst.c_str(), dst.length());
        }

        inm_strncpy_s(m_MirrorConfInfo.src_scsi_id, sizeof(m_MirrorConfInfo.src_scsi_id), prismvolumeinfo.sourceLUNID.c_str(), prismvolumeinfo.sourceLUNID.length());
        inm_strncpy_s(m_MirrorConfInfo.dst_scsi_id, sizeof(m_MirrorConfInfo.dst_scsi_id), atid.c_str(), atid.length());
        inm_strncpy_s(m_MirrorConfInfo.at_name, sizeof(m_MirrorConfInfo.at_name), prismvolumeinfo.applianceTargetLUNName.c_str(), prismvolumeinfo.applianceTargetLUNName.length());
        PrintMirrorConfInfo();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
    }
   
    return iStatus;
}


bool MirrorerMajor::FillSrcAttrsInSetupInput(const PRISM_VOLUME_INFO &prismvolumeinfo, const VolumeReporter::VolumeReport_t &vr)
{
    inm_vendor_t involfltvendor = FILTER_DEV_UNKNOWN_VENDOR;
    bool bfilled = GetVolumeVendorForInVolFlt(vr.m_Vendor, &involfltvendor);
    
    if (bfilled)
    {
        m_MirrorConfInfo.d_vendor = involfltvendor;
        if (VolumeSummary::DISK == vr.m_VolumeType)
        {
            m_MirrorConfInfo.d_flags |= FULL_DISK_FLAG;
        }
        else if (VolumeSummary::DISK_PARTITION == vr.m_VolumeType)
        {
            m_MirrorConfInfo.d_flags |= FULL_DISK_PARTITION_FLAG;
        }
    
        if (vr.m_bIsMultipathNode)
        {
            m_MirrorConfInfo.d_flags |= INM_IS_DEVICE_MULTIPATH;
        }
    
        if (VolumeSummary::SMI == vr.m_FormatLabel)
        {
            m_MirrorConfInfo.d_flags |= FULL_DISK_LABEL_VTOC;
        }
        else if (VolumeSummary::EFI == vr.m_FormatLabel)
        {
            m_MirrorConfInfo.d_flags |= FULL_DISK_LABEL_EFI;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "for source vendor %s, could not map to involflt vendor for source lun id %s\n", 
                                  StrVendor[vr.m_Vendor], prismvolumeinfo.sourceLUNID.c_str());
    }

    return bfilled;
}


int MirrorerMajor::SetupMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                               DeviceStream &stream, 
                               PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror)
{
    m_MirrorConfInfo.d_status = MIRROR_NO_ERROR;
    int iStatus = stream.IoControl(IOCTL_INMAGE_START_MIRRORING_DEVICE, &m_MirrorConfInfo);
    if (SV_SUCCESS != iStatus)
    {
        *pmirrorerror = m_DriverMirrorError.GetMirrorErrorFromDriver(m_MirrorConfInfo.d_status);
    }

    return iStatus;
}


int MirrorerMajor::DeleteMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                                DeviceStream &stream, 
                                PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror)
{
    SCSI_ID id = {""};
    inm_strncpy_s(id.scsi_id,sizeof(id.scsi_id), prismvolumeinfo.sourceLUNID.c_str(), INM_MAX_SCSI_ID_SIZE - 1);
    int iStatus = stream.IoControl(IOCTL_INMAGE_STOP_MIRRORING_DEVICE, &id);
    if (SV_SUCCESS != iStatus)
    {
        *pmirrorerror = (ENOTSUP == stream.GetErrorCode()) ? 
                        PRISM_VOLUME_INFO::MirrorNotSupported :
                        PRISM_VOLUME_INFO::MirrorDeleteIoctlFailed;
    }
    return iStatus;
}


void MirrorerMajor::PrintMirrorConfInfo(void)
{
    std::stringstream msg;
   
    msg << "d_type: " << m_MirrorConfInfo.d_type
        << ", d_flags: " << m_MirrorConfInfo.d_flags 
        << ", d_nblks: " << m_MirrorConfInfo.d_nblks
        << ", d_bsize: " << m_MirrorConfInfo.d_bsize
        << ", startoff: " << m_MirrorConfInfo.startoff
        << ", d_vendor: " << m_MirrorConfInfo.d_vendor
        << ", number of sources: " << m_MirrorConfInfo.nsources
        << ", number of at luns: " << m_MirrorConfInfo.ndestinations;
    DebugPrintf(SV_LOG_DEBUG, "Mirror set up information:\n");
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
    DebugPrintf(SV_LOG_DEBUG, "source volumes:\n");
    inm_u32_t sourcelimit = m_MirrorConfInfo.nsources * INM_GUID_LEN_MAX;
    for (inm_u32_t i = 0; i < sourcelimit; i += INM_GUID_LEN_MAX)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s\n", m_MirrorConfInfo.src_guid_list + i);
    }
    DebugPrintf(SV_LOG_DEBUG, "at luns:\n");
    inm_u32_t destlimit = m_MirrorConfInfo.ndestinations * INM_GUID_LEN_MAX;
    for (inm_u32_t i = 0; i < destlimit; i += INM_GUID_LEN_MAX)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s\n", m_MirrorConfInfo.dst_guid_list + i);
    }
    DebugPrintf(SV_LOG_DEBUG, "source scsi id: %s\n", m_MirrorConfInfo.src_scsi_id);
    DebugPrintf(SV_LOG_DEBUG, "at lun scsi id: %s\n", m_MirrorConfInfo.dst_scsi_id);
    DebugPrintf(SV_LOG_DEBUG, "at lun name: %s\n", m_MirrorConfInfo.at_name);
}


bool MirrorerMajor::GetVolumeVendorForInVolFlt(const VolumeSummary::Vendor uservendor, inm_vendor_t *pinvolfltvendor)
{
    UserToInVolFltVendor_t usertoinvolfltvendor;

    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::UNKNOWN_VENDOR, FILTER_DEV_UNKNOWN_VENDOR));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::NATIVE,         FILTER_DEV_NATIVE));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::DEVMAPPER,      FILTER_DEV_DEVMAPPER));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::MPXIO,          FILTER_DEV_MPXIO));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::EMC,            FILTER_DEV_EMC));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::HDLM,           FILTER_DEV_HDLM));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::DEVDID,         FILTER_DEV_DEVDID));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::DEVGLOBAL,      FILTER_DEV_DEVGLOBAL));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::VXDMP,          FILTER_DEV_VXDMP));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::SVM,            FILTER_DEV_SVM));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::VXVM,           FILTER_DEV_VXVM));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::LVM,            FILTER_DEV_LVM));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::INMVOLPACK,     FILTER_DEV_INMVOLPACK));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::INMVSNAP,       FILTER_DEV_INMVSNAP));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::CUSTOMVENDOR,   FILTER_DEV_CUSTOMVENDOR));
    usertoinvolfltvendor.insert(std::make_pair(VolumeSummary::ASM,            FILTER_DEV_ASM));

    bool bretval = false;
    ConstUserToInVolFltVendorIter_t iter = usertoinvolfltvendor.find(uservendor);
    if (iter != usertoinvolfltvendor.end())
    {
        *pinvolfltvendor = iter->second;
        bretval = true;
    }

    return bretval;
}
