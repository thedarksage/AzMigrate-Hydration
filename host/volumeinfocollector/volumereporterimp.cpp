#include <utility>
#include "volumereporterimp.h"
#include "volumegroupsettings.h"
#include "volumeinfocollector.h"
#include "logger.h"
#include "volumedefines.h"

VolumeReporterImp::VolumeReporterImp()
{
    m_WriteStateToMsg.insert(std::make_pair(Writable,           "is writable"));
    m_WriteStateToMsg.insert(std::make_pair(ErrUnknown,         "is unknown"));
    m_WriteStateToMsg.insert(std::make_pair(NotCollected,       "is not collected by volumeinfocollector"));
    m_WriteStateToMsg.insert(std::make_pair(ExtendedPartition,  "is extended partition"));
    m_WriteStateToMsg.insert(std::make_pair(Mounted,            "is mounted"));
    m_WriteStateToMsg.insert(std::make_pair(SystemVolume,       "is system volume"));
    m_WriteStateToMsg.insert(std::make_pair(CacheVolume,        "is cache volume"));
    m_WriteStateToMsg.insert(std::make_pair(Locked,             "is locked"));
    m_WriteStateToMsg.insert(std::make_pair(ContainsPageFile,   "is contains page file"));
    m_WriteStateToMsg.insert(std::make_pair(StartAtBlockZero,   "is partition starting at block zero"));
    m_WriteStateToMsg.insert(std::make_pair(UnderVolumeManager, "is under control of volume manager"));

    m_EntityTypeToMsg.insert(std::make_pair(Entity_t::EntityUnknown,   "unknown"));
    m_EntityTypeToMsg.insert(std::make_pair(Entity_t::EntityVolume,    "volume"));
    m_EntityTypeToMsg.insert(std::make_pair(Entity_t::EntityParent,    "parent"));
    m_EntityTypeToMsg.insert(std::make_pair(Entity_t::EntityChild,     "child"));
    m_EntityTypeToMsg.insert(std::make_pair(Entity_t::EntityMultipath, "multipath"));
}


std::string VolumeReporterImp::GetWriteStateStr(const WriteState_t ws)
{
    std::string s = m_WriteStateToMsg[ErrUnknown];
    WriteStateToMsg_t::const_iterator wrtomsgiter = m_WriteStateToMsg.find(ws);
    if (m_WriteStateToMsg.end() != wrtomsgiter)
    {
        s = wrtomsgiter->second;
    }

    return s;
}


std::string VolumeReporterImp::GetEntityTypeStr(const Entity_t::EntityType_t et)
{
    std::string s = m_EntityTypeToMsg[Entity_t::EntityUnknown];
    EntityTypeToMsg_t::const_iterator ettomsgiter = m_EntityTypeToMsg.find(et);
    if (m_EntityTypeToMsg.end() != ettomsgiter)
    {
        s = ettomsgiter->second;
    }

    return s;
}


void VolumeReporterImp::GetVolumeReport(const std::string &volumename, 
                                        const VolumeSummaries_t &vs,
                                        VolumeReport_t &volumereport)
{
    m_IdToVs.clear();
    m_VgToVs.clear();
    const VolumeSummary *pvs = GetVolumeSummaryForVolume(volumename, vs);

    if (pvs)
    {
        volumereport.m_bIsReported = true;
        for_each(vs.begin(), vs.end(), ProcessVolumeSummary(&m_IdToVs, &m_VgToVs));
        DebugPrintf(SV_LOG_DEBUG, "volume name %s collected by volumeinfocollector\n", volumename.c_str());
        /*
        PrintVs(*pvs);
        PrintVolSummariesByID(pvs->id);
        */
        FillVolumeReport(vs, pvs, volumereport);
    }
    else
    {
        FillEntityWriteState(Entity_t::EntityVolume, volumename, NotCollected, volumereport);
    }
}


void VolumeReporterImp::FillVolumeReport(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport)
{
    volumereport.m_VolumeType = pvs->type;
    volumereport.m_FormatLabel = pvs->formatLabel;
    volumereport.m_Vendor = pvs->vendor;
    volumereport.m_Vg = pvs->volumeGroup;
    volumereport.m_VgVendor = pvs->volumeGroupVendor;
    volumereport.m_bIsMultipathNode = pvs->isMultipath;
    volumereport.m_Id = pvs->id;
    volumereport.m_FileSystem = pvs->fileSystem;
    volumereport.m_IsMounted = pvs->isMounted;
    volumereport.m_Mountpoint = pvs->mountPoint;
    volumereport.m_Size = pvs->capacity;
    volumereport.m_StartOffset = pvs->physicalOffset;
	
	ConstAttributesIter_t cit = pvs->attributes.find(NsVolumeAttributes::DEVICE_NAME);
	if (cit != pvs->attributes.end())
		volumereport.m_DeviceName = cit->second;

	cit = pvs->attributes.find(NsVolumeAttributes::STORAGE_TYPE);
	if (cit != pvs->attributes.end())
		volumereport.m_StorageType = cit->second;

	cit = pvs->attributes.find(NsVolumeAttributes::IS_SHARED);
	if (cit != pvs->attributes.end())
		volumereport.m_IsShared = ("true" == cit->second);

	cit = pvs->attributes.find(NsVolumeAttributes::NAME_BASED_ON);
	if (cit != pvs->attributes.end()){
		volumereport.m_nameBasedOn = 
            (NsVolumeAttributes::SCSIID == cit->second) ? VolumeSummary::SCSIID : 
            (NsVolumeAttributes::SCSIHCTL == cit->second) ? VolumeSummary::SCSIHCTL : VolumeSummary::SIGNATURE;
	}

    if (ReportFullLevel == volumereport.m_ReportLevel)
    {
        UpdateReportFromMPaths(pvs, volumereport);
        UpdateReportFromHierarchy(vs, pvs, volumereport);
        FillLvComponents(vs, pvs, volumereport);
        FillTopLvs(pvs, volumereport);
    }
}


void VolumeReporterImp::FillLvComponents(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport)
{
    if ((VolumeSummary::LOGICAL_VOLUME == pvs->type) && (!pvs->volumeGroup.empty()))
    {
        ConstVgToVSIterPair_t p = m_VgToVs.equal_range(pvs->volumeGroup);
        for (ConstVgToVSIter_t iter = p.first; iter != p.second; iter++)
        {
            const VolumeSummary *pvseach = iter->second;
            if (((VolumeSummary::DISK == pvseach->type) || 
                (VolumeSummary::DISK_PARTITION == pvseach->type) || 
                (VolumeSummary::PARTITION == pvseach->type) || 
                (VolumeSummary::EXTENDED_PARTITION == pvseach->type)) &&
                (pvs->vendor == pvseach->volumeGroupVendor))
            {
                volumereport.m_LvComponents.insert(pvseach->name);
                VolumeSummaryVec_t mpvec;
                FillMPaths(pvseach, mpvec);
                for (ConstVsVecIter_t miter = mpvec.begin(); miter != mpvec.end(); miter++)
                {
                    const VolumeSummary *mpvs = *miter;
                    volumereport.m_LvComponents.insert(mpvs->name);
                }
                if ((VolumeSummary::PARTITION == pvseach->type) || 
                    (VolumeSummary::DISK_PARTITION == pvseach->type) || 
                    (VolumeSummary::EXTENDED_PARTITION == pvseach->type))
                {
                    VolumeSummaryVec_t vsvec;
                    FillParents(vs, pvs, vsvec);
                    for (ConstVsVecIter_t vsiter = vsvec.begin(); vsiter != vsvec.end(); vsiter++)
                    {
                        const VolumeSummary *pparentvs = *vsiter;
                        volumereport.m_LvComponents.insert(pparentvs->name);
                    }
                }
            }
        }
    }
}


void VolumeReporterImp::FillTopLvs(const VolumeSummary *pvs, VolumeReport_t &volumereport)
{
    if ((VolumeSummary::DISK == pvs->type) || 
        (VolumeSummary::DISK_PARTITION == pvs->type) || 
        (VolumeSummary::PARTITION == pvs->type) || 
        (VolumeSummary::EXTENDED_PARTITION == pvs->type))
    {
        VolumeSummaryVec_t mpvec;
        mpvec.push_back(pvs);
        FillMPaths(pvs, mpvec);
        for (ConstVsVecIter_t miter = mpvec.begin(); miter != mpvec.end(); miter++)
        {
            const VolumeSummary *mpvs = *miter;
            if (!mpvs->volumeGroup.empty())
            {
                ConstVgToVSIterPair_t p = m_VgToVs.equal_range(mpvs->volumeGroup);
                for (ConstVgToVSIter_t iter = p.first; iter != p.second; iter++)
                {
                    const VolumeSummary *pvseach = iter->second;
                    if ((VolumeSummary::LOGICAL_VOLUME == pvseach->type) &&
                        (mpvs->volumeGroupVendor == pvseach->vendor))
                    {
                        TopLv_t tlv(pvseach->name, pvseach->volumeGroup, pvseach->vendor);
                        volumereport.m_TopLvs.push_back(tlv);
                    }
                }
       
                if ((VolumeSummary::DISK == mpvs->type) ||
                    (VolumeSummary::DISK_PARTITION == mpvs->type))
                {
                    VolumeSummaryVec_t vsvec;
                    FillChildren(mpvs, vsvec);
                    for (ConstVsVecIter_t vsiter = vsvec.begin(); vsiter != vsvec.end(); vsiter++)
                    {
                        const VolumeSummary *pchildvs = *vsiter;
                        /* since we are copying volume group of parent to child */
                        ConstVgToVSIterPair_t p = m_VgToVs.equal_range(pchildvs->volumeGroup);
                        for (ConstVgToVSIter_t iter = p.first; iter != p.second; iter++)
                        {
                            const VolumeSummary *pvseach = iter->second;
                            if ((VolumeSummary::LOGICAL_VOLUME == pvseach->type) &&
                                (pchildvs->volumeGroupVendor == pvseach->vendor))
                            {
                                TopLv_t tlv(pvseach->name, pvseach->volumeGroup, pvseach->vendor);
                                volumereport.m_TopLvs.push_back(tlv);
                            }
                        }
                    }
                }
            }
        }
    }
}


void VolumeReporterImp::UpdateReportFromHierarchy(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport)
{
    if ((VolumeSummary::DISK == pvs->type) ||
        (VolumeSummary::DISK_PARTITION == pvs->type))
    {
        UpdateReportFromChildren(pvs, volumereport);
    }

    if ((VolumeSummary::PARTITION == pvs->type) ||
        (VolumeSummary::DISK_PARTITION == pvs->type) ||
        (VolumeSummary::EXTENDED_PARTITION == pvs->type))
    {
        UpdateReportFromParents(vs, pvs, volumereport);
    }
}


void VolumeReporterImp::UpdateReportFromChildren(const VolumeSummary *pvs, VolumeReport_t &volumereport)
{
    VolumeSummaryVec_t vsvec;

    FillChildren(pvs, vsvec);
    ConstVsVecIter_t vsiter = vsvec.begin();
    for ( /* empty */ ; vsiter != vsvec.end(); vsiter++)
    {
        const VolumeSummary *pvseach = *vsiter;
        UpdateWriteStateFromChild(Entity_t::EntityChild, pvseach, volumereport);
        volumereport.m_Children.insert(pvseach->name);
        if ((volumereport.m_Vg.empty()) &&
            (!pvseach->volumeGroup.empty()))
        {
            volumereport.m_Vg = pvseach->volumeGroup;
            volumereport.m_VgVendor = pvseach->volumeGroupVendor;
        }
    }
}


void VolumeReporterImp::UpdateReportFromParents(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport)
{
    VolumeSummaryVec_t vsvec;

    FillParents(vs, pvs, vsvec);
    ConstVsVecIter_t vsiter = vsvec.begin();
    for ( /* empty */ ; vsiter != vsvec.end(); vsiter++)
    {
        const VolumeSummary *pvseach = *vsiter;
        UpdateWriteStateFromParent(Entity_t::EntityParent, pvseach, volumereport);
        volumereport.m_Parents.insert(pvseach->name);
    }
}


void VolumeReporterImp::UpdateWriteStateFromParent(const Entity_t::EntityType_t et,
                                                   const VolumeSummary *pvs, 
                                                   VolumeReport_t &volumereport)
{
    if (pvs->isMounted)
    {
        FillEntityWriteState(et, pvs->name, Mounted, volumereport);
    }
}


void VolumeReporterImp::UpdateWriteState(const Entity_t::EntityType_t et,
                                         const VolumeSummary *pvs, 
                                         VolumeReport_t &volumereport)
{
    if (pvs->isSystemVolume)
    {
        FillEntityWriteState(et, pvs->name, SystemVolume, volumereport);
    }
 
    if (pvs->isStartingAtBlockZero)
    {
        FillEntityWriteState(et, pvs->name, StartAtBlockZero, volumereport);
    }

    if (VolumeSummary::EXTENDED_PARTITION == pvs->type)
    {
        FillEntityWriteState(et, pvs->name, ExtendedPartition, volumereport);
    }

    if (pvs->isMounted)
    {
        FillEntityWriteState(et, pvs->name, Mounted, volumereport);
    }

    if (pvs->isCacheVolume)
    {
        FillEntityWriteState(et, pvs->name, CacheVolume, volumereport);
    }

    if (pvs->locked)
    {
        FillEntityWriteState(et, pvs->name, Locked, volumereport);
    }

    if (pvs->containPagefile)
    {
        FillEntityWriteState(et, pvs->name, ContainsPageFile, volumereport);
    }

    if ((!pvs->volumeGroup.empty()) && 
        ((VolumeSummary::DISK == pvs->type) || 
         (VolumeSummary::DISK_PARTITION == pvs->type) || 
         (VolumeSummary::PARTITION == pvs->type) || 
         (VolumeSummary::EXTENDED_PARTITION == pvs->type)))
    {
        /* TODO: print volume group name here */
        FillEntityWriteState(et, pvs->name, UnderVolumeManager, volumereport);
    }
}


void VolumeReporterImp::UpdateWriteStateFromChild(const Entity_t::EntityType_t et,
                                                  const VolumeSummary *pvs, 
                                                  VolumeReport_t &volumereport)
{
    if (pvs->isSystemVolume)
    {
        FillEntityWriteState(et, pvs->name, SystemVolume, volumereport);
    }
 
    if (pvs->isMounted)
    {
        FillEntityWriteState(et, pvs->name, Mounted, volumereport);
    }

    if (pvs->isCacheVolume)
    {
        FillEntityWriteState(et, pvs->name, CacheVolume, volumereport);
    }

    /* TODO: do not know should this be checked for hiers ? */
    if (pvs->locked)
    {
        FillEntityWriteState(et, pvs->name, Locked, volumereport);
    }

    /* TODO: do not know should this be checked for hiers ? */
    if (pvs->containPagefile)
    {
        FillEntityWriteState(et, pvs->name, ContainsPageFile, volumereport);
    }

    if ((!pvs->volumeGroup.empty()) && 
        ((VolumeSummary::DISK == pvs->type) || 
         (VolumeSummary::DISK_PARTITION == pvs->type) || 
         (VolumeSummary::PARTITION == pvs->type) || 
         (VolumeSummary::EXTENDED_PARTITION == pvs->type)))
    {
        /* TODO: print volume group name here */
        FillEntityWriteState(et, pvs->name, UnderVolumeManager, volumereport);
    }
}


const VolumeSummary *
 VolumeReporterImp::GetVolumeSummaryForVolume(const std::string &volumename, const VolumeSummaries_t &vs)
{
    const VolumeSummary *pvs = 0;
    VolumeSummaries_t::const_iterator vsiter = vs.begin();
    for ( /* empty */ ; vsiter != vs.end(); vsiter++)
    {
        const VolumeSummary &vs = *vsiter;
        pvs = (vs.name == volumename) ? &vs : GetVolumeSummaryForVolume(volumename, vs.children);
        if (pvs)
        {
            break;
        }
    }

    return pvs;
}


void VolumeReporterImp::FillEntityWriteState(const Entity_t::EntityType_t entitytype, 
                                             const std::string &entityname,
                                             const WriteState_t wrstate,
                                             VolumeReport_t &volumereport)
{
    Entity_t en(entitytype, entityname);
    EntityWriteStatesIter_t i = volumereport.m_EntityWriteStates.find(en);
    if (i != volumereport.m_EntityWriteStates.end())
    {
        WriteStates_t &rwss = i->second;
        rwss.push_back(wrstate);
    }
    else
    {
        WriteStates_t wss;
        wss.push_back(wrstate);
        volumereport.m_EntityWriteStates.insert(std::make_pair(en, wss));
    }
}


void VolumeReporterImp::PrintVs(const VolumeSummary &vs)
{
    std::stringstream msg;
    msg << "----------------------------------------------------------\n"
        << "ID                               : " << vs.id << '\n'
        << "Name                             : " << vs.name << '\n'
        << "Type                             : " << StrDeviceType[vs.type] << '\n'
        << "Vendor                           : " << StrVendor[vs.vendor] << '\n'
        << "File System                      : " << vs.fileSystem << '\n'
        << "Mount Point                      : " << vs.mountPoint << '\n'
        << "Mounted                          : " << vs.isMounted << '\n'
        << "System Volume                    : " << vs.isSystemVolume << '\n'
        << "Cache Volume                     : " << vs.isCacheVolume << '\n'
        << "Capacity                         : " << vs.capacity << '\n'
        << "Locked                           : " << vs.locked << '\n'
        << "Physical Offset                  : " << vs.physicalOffset << '\n'
        << "Sector Size                      : " << vs.sectorSize << '\n'
        << "Volume Label                     : " << vs.volumeLabel << '\n'
        << "Contains Page File               : " << vs.containPagefile << '\n'
        << "Starting At Block Zero           : " << vs.isStartingAtBlockZero << '\n'
        << "Volume Group                     : " << vs.volumeGroup << '\n'
        << "Multipath                        : " << vs.isMultipath << '\n'
        << "Write Cache State                : " << StrWCState[vs.writeCacheState] << '\n'
        << "Format Label                     : " << StrFormatLabel[vs.formatLabel] << '\n'
        << "Raw Size                         : " << vs.rawcapacity << '\n'
        << "\n\n";
    DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
}


void VolumeReporterImp::PrintVolSummariesByID(const std::string &id)
{
    DebugPrintf(SV_LOG_DEBUG, "========\n");
    if (id.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "in PrintVolSummariesByID, input id is empty is empty\n");
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "ID: %s\n", id.c_str());
        ConstIdToVSIter_t iter = m_IdToVs.find(id);
        if (iter != m_IdToVs.end())
        {
            const VolumeSummaryVec_t &vsvec = iter->second;
            for (ConstVsVecIter_t viter = vsvec.begin(); viter != vsvec.end(); viter++)
            {
                const VolumeSummary *pvs = *viter;
                PrintVs(*pvs);
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "========\n");
}


void VolumeReporterImp::FillChildren(const VolumeSummary *pvs, VolumeSummaryVec_t &vsvec)
{
    if (pvs->id.empty())
    { 
        /* do not use recursion here; also only three levels are feasible */
        /* To make code easy (readable), always loop levels regardless 
         * of volume type */
        ConstVolumeSummariesIter iter = pvs->children.begin();
        for ( /* empty */ ; iter != pvs->children.end(); iter++)
        {
            const VolumeSummary &vschild = *iter;
            vsvec.push_back(&vschild);
            ConstVolumeSummariesIter citer = vschild.children.begin();
            for ( /* empty */ ; citer != vschild.children.end(); citer++)
            {
                const VolumeSummary &vsgrandchild = *citer;
                vsvec.push_back(&vsgrandchild);
            }
        }
    }
    else
    {
        if (VolumeSummary::DISK == pvs->type)
        {
            ConstIdToVSIter_t iter = m_IdToVs.find(pvs->id);
            if (iter != m_IdToVs.end())
            {
                const VolumeSummaryVec_t &mpvec = iter->second;
                for (ConstVsVecIter_t miter = mpvec.begin(); miter != mpvec.end(); miter++)
                {
                    const VolumeSummary *pmvseach = *miter;
                    if (VolumeSummary::DISK_PARTITION == pmvseach->type)
                    {
                        vsvec.push_back(pmvseach);
                    }
                }
            }
        }
        ConstIdToVSIter_t iditer = m_IdToVs.begin();
        for ( /* empty */ ; iditer != m_IdToVs.end() ; iditer++)
        {
            const std::string &id = iditer->first;
            if (IsChildOfParentId(pvs->id, id))
            {
                const VolumeSummaryVec_t &vsidvec = iditer->second;
                vsvec.insert(vsvec.end(), vsidvec.begin(), vsidvec.end());
            }
        }
    }
}


void VolumeReporterImp::FillParents(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeSummaryVec_t &vsvec)
{
    if (pvs->id.empty())
    { 
        ConstVolumeSummariesIter piter = vs.begin(); 
        bool bfound = false;
        for ( /* empty */ ; piter != vs.end(); piter++)
        {
            if (bfound)
            {
                break;
            }
            const VolumeSummary &parent = *piter;
            ConstVolumeSummariesIter citer = parent.children.begin();
            for ( /* empty */ ; citer != parent.children.end(); citer++)
            {
                if (bfound)
                {
                    break;
                }
                const VolumeSummary &child = *citer;
                if (child.name == pvs->name)
                {
                    bfound = true;
                    vsvec.push_back(&parent);
                    break;
                }
                ConstVolumeSummariesIter gciter = child.children.begin();
                for ( /* empty */ ; gciter != child.children.end(); gciter++)
                {
                    const VolumeSummary &gchild = *gciter;
                    if (gchild.name == pvs->name)
                    {
                        bfound = true;
                        vsvec.push_back(&parent);
                        vsvec.push_back(&child);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (VolumeSummary::DISK_PARTITION == pvs->type)
        {
            ConstIdToVSIter_t iter = m_IdToVs.find(pvs->id);
            if (iter != m_IdToVs.end())
            {
                const VolumeSummaryVec_t &mpvec = iter->second;
                for (ConstVsVecIter_t miter = mpvec.begin(); miter != mpvec.end(); miter++)
                {
                    const VolumeSummary *pmvseach = *miter;
                    if (VolumeSummary::DISK == pmvseach->type)
                    {
                        vsvec.push_back(pmvseach);
                    }
                }
            }
        }
        else
        {
            std::string parentid = GetParentIDFromChild(pvs->id);
            if (!parentid.empty())
            {
                ConstIdToVSIter_t iditer = m_IdToVs.find(parentid);
                if (m_IdToVs.end() != iditer)
                {
                    const VolumeSummaryVec_t &vsidvec = iditer->second;
                    vsvec.insert(vsvec.end(), vsidvec.begin(), vsidvec.end());
                }
            }
        }
    }
}


void VolumeReporterImp::GetCollectedVolumes(const VolumeSummaries_t &vs, CollectedVolumes_t &collectedvolumes)
{
    GetVolumeSummaryVolumes(vs, collectedvolumes);
}


void VolumeReporterImp::GetVolumeSummaryVolumes(const VolumeSummaries_t &vs, CollectedVolumes_t &collectedvolumes)
{
    ConstVolumeSummariesIter iter = vs.begin();
    for ( /* empty */ ; iter != vs.end(); iter++)
    {
        const VolumeSummary &vseach = *iter;
        collectedvolumes.m_Volumes.insert(vseach.name);
        GetVolumeSummaryVolumes(vseach.children, collectedvolumes);
    }
}


void VolumeReporterImp::UpdateReportFromMPaths(const VolumeSummary *pvs, VolumeReport_t &volumereport) 
{
    UpdateWriteState(Entity_t::EntityVolume, pvs, volumereport);
    VolumeSummaryVec_t vsvec;
    FillMPaths(pvs, vsvec);

    for (ConstVsVecIter_t viter = vsvec.begin(); viter != vsvec.end(); viter++)
    {
        const VolumeSummary *pvseach = *viter;
        UpdateWriteState(Entity_t::EntityMultipath, pvseach, volumereport);
        volumereport.m_Multipaths.insert(pvseach->name);
        if ((volumereport.m_Vg.empty()) &&
            (!pvseach->volumeGroup.empty()))
        {
            volumereport.m_Vg = pvseach->volumeGroup;
            volumereport.m_VgVendor = pvseach->volumeGroupVendor;
        }
    }
}


void VolumeReporterImp::FillMPaths(const VolumeSummary *pvs, VolumeSummaryVec_t &vsvec)
{
    DeviceTypes_t devicetypes;
    FillMpathsType(pvs->type, devicetypes);
    if (!devicetypes.empty() && !pvs->id.empty())
    {
        ConstIdToVSIter_t iter = m_IdToVs.find(pvs->id);
        if (iter != m_IdToVs.end())
        {
            const VolumeSummaryVec_t &mpvec = iter->second;
            for (ConstVsVecIter_t miter = mpvec.begin(); miter != mpvec.end(); miter++)
            {
                const VolumeSummary *pvseach = *miter;
                if ((pvseach->name != pvs->name) &&
                    (devicetypes.end() != find(devicetypes.begin(), devicetypes.end(), pvseach->type)))
                {
                    vsvec.push_back(pvseach);
                }
            }
        }
    }
}


bool VolumeReporterImp::IsChildOfParentId(const std::string &parentid, const std::string &childid)
{
    bool bischildid = false;
    if (!parentid.empty() && !childid.empty())
    {
        std::string remdchildsep = GetParentIDFromChild(childid);
        bischildid = (remdchildsep == parentid);
    }

    return bischildid;
}


std::string VolumeReporterImp::GetParentIDFromChild(const std::string &childid)
{
    std::string parentid;

    size_t sepidx = childid.rfind(SEPINPARTITIONID);
    if ((std::string::npos != sepidx) &&
        (sepidx < (childid.length() - 1)))
    {
        if (AreAllDigits(childid.c_str() + sepidx + 1))
        {
            parentid = childid.substr(0, sepidx);
        }
    }

    return parentid;
}


void VolumeReporterImp::FillMpathsType(const VolumeSummary::Devicetype type, DeviceTypes_t &devicetypes)
{
    if ((VolumeSummary::DISK == type) ||
        (VolumeSummary::DISK_PARTITION == type))
    {
        devicetypes.push_back(type);
    }
    else if ((VolumeSummary::PARTITION == type) ||
             (VolumeSummary::EXTENDED_PARTITION == type))
    {
        devicetypes.push_back(VolumeSummary::PARTITION);
        devicetypes.push_back(VolumeSummary::EXTENDED_PARTITION);
    }
}


void VolumeReporterImp::GetOsDiskReport(const std::string& diskId,
                                        const VolumeSummaries_t &vs,
                                        DiskReports_t &diskreports)
{
    ConstVolumeSummariesIter diter = vs.begin();
    for ( /* empty */ ; diter != vs.end(); diter++)
    {
        const VolumeSummary &dvseach = *diter;
        if ((VolumeSummary::DISK == dvseach.type) &&
            (VolumeSummary::NATIVE == dvseach.vendor) &&
            ((dvseach.id == diskId) || (dvseach.name == diskId)))
        {
            DiskReport_t dr;
            dr.m_Id = dvseach.id;
            dr.m_Name = dvseach.name;
            dr.m_Size = dvseach.capacity;
            dr.attributes = dvseach.attributes;
            dr.m_SectorSize = dvseach.sectorSize;
            dr.m_RawCapacity = dvseach.rawcapacity;
            ConstVolumeSummariesIter piter = dvseach.children.begin();
            for ( /* empty */ ; piter != dvseach.children.end(); piter++)
            {
                const VolumeSummary &pvseach = *piter;
                if ((VolumeSummary::PARTITION == pvseach.type) ||
                    (VolumeSummary::EXTENDED_PARTITION == pvseach.type))
                {
                    PartitionReport_t pr;
                    pr.m_Name = pvseach.name;
                    pr.m_StartOffset = pvseach.physicalOffset;
                    pr.m_Size = pvseach.capacity;
                    dr.m_PartitionReports.push_back(pr);
                }
            }
            diskreports.push_back(dr);
        }
    }
}

void VolumeReporterImp::GetBootDiskReport(const VolumeSummaries_t &vs,
                                        DiskReports_t &diskreports)
{
    ConstVolumeSummariesIter diter = vs.begin();
    for ( /* empty */ ; diter != vs.end(); diter++)
    {
        const VolumeSummary &dvseach = *diter;
        if ((VolumeSummary::DISK == dvseach.type) &&
            (VolumeSummary::NATIVE == dvseach.vendor) &&
            HasBootablePartition(dvseach))
        {
            DiskReport_t dr;
            dr.m_Id = dvseach.id;
            dr.m_Name = dvseach.name;
            dr.m_Size = dvseach.capacity;
            dr.attributes = dvseach.attributes;
            dr.m_SectorSize = dvseach.sectorSize;
            dr.m_RawCapacity = dvseach.rawcapacity;
            ConstVolumeSummariesIter piter = dvseach.children.begin();
            for ( /* empty */ ; piter != dvseach.children.end(); piter++)
            {
                const VolumeSummary &pvseach = *piter;
                if ((VolumeSummary::PARTITION == pvseach.type) ||
                    (VolumeSummary::EXTENDED_PARTITION == pvseach.type))
                {
                    PartitionReport_t pr;
                    pr.m_Name = pvseach.name;
                    pr.m_StartOffset = pvseach.physicalOffset;
                    pr.m_Size = pvseach.capacity;
                    dr.m_PartitionReports.push_back(pr);
                }
            }
            diskreports.push_back(dr);
        }
    }
}

void VolumeReporterImp::GetOsDiskReportByScsiLunNumber(const std::string& scsiLunNumber,
    const VolumeSummaries_t &vs,
    DiskReports_t &diskreports)
{
    ConstVolumeSummariesIter diter = vs.begin();
    for ( /* empty */; diter != vs.end(); diter++)
    {
        const VolumeSummary &dvseach = *diter;
        if ((VolumeSummary::DISK == dvseach.type) &&
            (VolumeSummary::NATIVE == dvseach.vendor) &&
            (GetScsiLunNumber(dvseach) == scsiLunNumber) &&
            !IsUnsupportedDisk(dvseach) &&
            !HasBootablePartition(dvseach))
        {
            DiskReport_t dr;
            dr.m_Id = dvseach.id;
            dr.m_Name = dvseach.name;
            dr.m_Size = dvseach.capacity;
            dr.attributes = dvseach.attributes;
            dr.m_SectorSize = dvseach.sectorSize;
            dr.m_RawCapacity = dvseach.rawcapacity;
            ConstVolumeSummariesIter piter = dvseach.children.begin();
            for ( /* empty */; piter != dvseach.children.end(); piter++)
            {
                const VolumeSummary &pvseach = *piter;
                if ((VolumeSummary::PARTITION == pvseach.type) ||
                    (VolumeSummary::EXTENDED_PARTITION == pvseach.type))
                {
                    PartitionReport_t pr;
                    pr.m_Name = pvseach.name;
                    pr.m_StartOffset = pvseach.physicalOffset;
                    pr.m_Size = pvseach.capacity;
                    dr.m_PartitionReports.push_back(pr);
                }
            }
            diskreports.push_back(dr);
        }
    }

    return;
}

void VolumeReporterImp::GetNumberOfNativeDisks(const VolumeSummaries_t &vs,
    uint32_t &numDisks)
{
    ConstVolumeSummariesIter diter = vs.begin();
    for ( /* empty */; diter != vs.end(); diter++)
    {
        if ((VolumeSummary::DISK == diter->type) &&
            (VolumeSummary::NATIVE == diter->vendor))
        {
            numDisks++;
        }
    }

    return;
}

std::string VolumeReporterImp::GetScsiLunNumber(const VolumeSummary &vs)
{
    std::string scsiLunNumber;
    ConstAttributesIter_t   cit;
    cit = vs.attributes.find(NsVolumeAttributes::SCSI_LOGICAL_UNIT);
    if (vs.attributes.end() != cit){
        scsiLunNumber = cit->second;
    }
    return scsiLunNumber;
}

bool VolumeReporterImp::IsUnsupportedDisk(const VolumeSummary &vs)
{
    std::string strTempDisk;
    std::string strBekDisk;
    std::string strIscsiDisk;
    std::string strNvmeDisk;
    ConstAttributesIter_t   cit;
    cit = vs.attributes.find(NsVolumeAttributes::IS_RESOURCE_DISK);
    if (vs.attributes.end() != cit){
        strTempDisk = cit->second;
    }
    cit = vs.attributes.find(NsVolumeAttributes::IS_BEK_VOLUME);
    if (vs.attributes.end() != cit){
        strBekDisk = cit->second;
    }
    cit = vs.attributes.find(NsVolumeAttributes::IS_ISCSI_DISK);
    if (vs.attributes.end() != cit){
        strIscsiDisk = cit->second;
    }
    cit = vs.attributes.find(NsVolumeAttributes::IS_NVME_DISK);
    if (vs.attributes.end() != cit){
        strNvmeDisk = cit->second;
    }
    return (0 == strTempDisk.compare("true") || 0 == strBekDisk.compare("true") ||
            0 == strIscsiDisk.compare("true") || 0 == strNvmeDisk.compare("true"));
}

bool VolumeReporterImp::HasBootablePartition(const VolumeSummary &vs)
{
    std::string strBootDisk;
    ConstAttributesIter_t   cit;
    cit = vs.attributes.find(NsVolumeAttributes::HAS_BOOTABLE_PARTITION);
    if (vs.attributes.end() != cit){
        strBootDisk = cit->second;
    }
    return (0 == strBootDisk.compare("true"));
}
