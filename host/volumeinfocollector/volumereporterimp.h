#ifndef _VOLUMEREPORTER__IMP__H_
#define _VOLUMEREPORTER__IMP__H_

#include <map>
#include <set>
#include <vector>
#include <string>
#include "volumereporter.h"
#include "volumegroupsettings.h"

#define ERR_INSTR_PREFIX " of volume "

class VolumeReporterImp : public VolumeReporter
{
    typedef std::vector<const VolumeSummary *> VolumeSummaryVec_t;
    typedef VolumeSummaryVec_t::const_iterator ConstVsVecIter_t;
    typedef std::map<std::string, VolumeSummaryVec_t> IdToVolumeSummaries_t;
    typedef std::multimap<std::string, const VolumeSummary *> VgToVolumeSummary_t;
    typedef IdToVolumeSummaries_t::const_iterator ConstIdToVSIter_t;
    typedef IdToVolumeSummaries_t::iterator IdToVSIter_t;
    typedef VgToVolumeSummary_t::const_iterator ConstVgToVSIter_t;
    typedef std::pair<ConstVgToVSIter_t, ConstVgToVSIter_t> ConstVgToVSIterPair_t;
    typedef std::vector<VolumeSummary::Devicetype> DeviceTypes_t;

    class EqVolume : public std::unary_function<VolumeSummary, bool>
    {
        std::string m_Volume;
    public:
        explicit EqVolume(const std::string &volume) : m_Volume(volume) { }
        bool operator()(const VolumeSummary &vs)
        {
            bool bfound = (m_Volume == vs.name) ? 
                          true : 
                          (vs.children.end() != find_if(vs.children.begin(), vs.children.end(), EqVolume(m_Volume)));
            return bfound;
        }
    };

    class ProcessVolumeSummary : public std::unary_function<VolumeSummary, void>
    {
        IdToVolumeSummaries_t *m_pIdToVs;
        VgToVolumeSummary_t *m_pVgToVs;

    public:
        explicit ProcessVolumeSummary(IdToVolumeSummaries_t *pidtovs, VgToVolumeSummary_t *pvgtovs) : 
                                      m_pIdToVs(pidtovs), m_pVgToVs(pvgtovs) { }
        void operator()(const VolumeSummary &vs)
        {
            if (!vs.id.empty())
            {
                IdToVSIter_t iter = m_pIdToVs->find(vs.id);
                if (iter != m_pIdToVs->end())
                {
                    VolumeSummaryVec_t &vsvec = iter->second;
                    vsvec.push_back(&vs);
                }
                else
                {
                    VolumeSummaryVec_t vsvec;
                    vsvec.push_back(&vs);
                    m_pIdToVs->insert(std::make_pair(vs.id, vsvec));
                }
            }
            if (!vs.volumeGroup.empty())
            {
                m_pVgToVs->insert(std::make_pair(vs.volumeGroup, &vs));
            }
            for_each(vs.children.begin(), vs.children.end(), ProcessVolumeSummary(m_pIdToVs, m_pVgToVs));
        }
    };

public:
    VolumeReporterImp();
    void GetOsDiskReport(const std::string &diskId,
                         const VolumeSummaries_t &vs,
                             DiskReports_t &diskreports);
    void GetBootDiskReport(const VolumeSummaries_t &vs,
                                 DiskReports_t &diskreports);
    void GetVolumeReport(const std::string &volumename, 
                         const VolumeSummaries_t &vs,
                         VolumeReport_t &volumereport);
    void GetCollectedVolumes(const VolumeSummaries_t &vs, CollectedVolumes_t &collectedvolumes);
    std::string GetWriteStateStr(const WriteState_t ws);
    std::string GetEntityTypeStr(const Entity_t::EntityType_t et);

    void GetOsDiskReportByScsiLunNumber(const std::string &scsiLunNumber,
        const VolumeSummaries_t &vs,
        DiskReports_t &diskreports);

    void GetNumberOfNativeDisks(const VolumeSummaries_t &vs, uint32_t &numDisks);

private:
    WriteStateToMsg_t m_WriteStateToMsg;
    EntityTypeToMsg_t m_EntityTypeToMsg;
    IdToVolumeSummaries_t m_IdToVs;
    VgToVolumeSummary_t m_VgToVs;

private:
    const VolumeSummary *GetVolumeSummaryForVolume(const std::string &volumename, const VolumeSummaries_t &vs);
    void FillEntityWriteState(const Entity_t::EntityType_t entitytype, 
                              const std::string &entityname,
                              const WriteState_t wrstate,
                              VolumeReport_t &volumereport);
    void FillVolumeReport(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void PrintVs(const VolumeSummary &vs);
    void PrintVolSummariesByID(const std::string &id);
    void UpdateWriteState(const Entity_t::EntityType_t et,
                          const VolumeSummary *pvs, 
                          VolumeReport_t &volumereport);
    void UpdateWriteStateFromChild(const Entity_t::EntityType_t et,
                                   const VolumeSummary *pvs, 
                                   VolumeReport_t &volumereport);
    void UpdateWriteStateFromParent(const Entity_t::EntityType_t et,
                                    const VolumeSummary *pvs, 
                                    VolumeReport_t &volumereport);
    void UpdateReportFromHierarchy(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void UpdateReportFromChildren(const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void UpdateReportFromParents(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void FillLvComponents(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void FillTopLvs(const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void FillChildren(const VolumeSummary *pvs, VolumeSummaryVec_t &vsvec);
    void FillParents(const VolumeSummaries_t &vs, const VolumeSummary *pvs, VolumeSummaryVec_t &vsvec);
    void GetVolumeSummaryVolumes(const VolumeSummaries_t &vs, CollectedVolumes_t &collectedvolumes);
    void UpdateReportFromMPaths(const VolumeSummary *pvs, VolumeReport_t &volumereport);
    void FillMPaths(const VolumeSummary *pvs, VolumeSummaryVec_t &vsvec);
    bool IsChildOfParentId(const std::string &parentid, const std::string &childid);
    std::string GetParentIDFromChild(const std::string &childid);
    void FillMpathsType(const VolumeSummary::Devicetype type, DeviceTypes_t &devicetypes);
    std::string GetScsiLunNumber(const VolumeSummary &vs);
    bool IsUnsupportedDisk(const VolumeSummary &vs);
    bool HasBootablePartition(const VolumeSummary &vs);
};

#endif /* _VOLUMEREPORTER__IMP__H_ */
