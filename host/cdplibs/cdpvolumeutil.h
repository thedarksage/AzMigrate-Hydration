#ifndef CDP_VOLUME_UTIL_H
#define CDP_VOLUME_UTIL_H

#include "cdpvolume.h"
#include "fssupportactions.h"
#include "inmquitfunction.h"
#include "volumeapplier.h"
#include "portablehelpers.h"

class cdp_volume_util
{
public:
	cdp_volume_util();
    bool IsOffsetLengthCoveredInClusterInfo(const SV_ULONGLONG offsettocheck, const SV_UINT &lengthtocheck, 
                                            const VolumeClusterInformer::VolumeClusterInfo &vci,
                                            const SV_UINT &clustersize);
    bool GetClusterBitmap(const SV_ULONGLONG offset, const SV_UINT &length, 
                          VolumeClusterInformer::VolumeClusterInfo &vci, 
                          cdp_volume_t *volume,
                          const SV_UINT &clustersize);
    bool IsAnyClusterUsed(const VolumeClusterInformer::VolumeClusterInfo &vci,
                          const SV_ULONGLONG offset,
                          const SV_UINT &length,
                          const SV_UINT &clustersize);
    SV_UINT GetContinuousUsedClusters(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                      const SV_ULONGLONG offset,
                                      const SV_UINT &length,
                                      const SV_UINT &clustersize);
    SV_UINT GetLengthCovered(const SV_UINT &continuoususedclusters,
                             const SV_ULONGLONG offsetasked,
                             const SV_UINT &lengthasked,
                             const SV_UINT &clustersize);
    SV_UINT GetContinuousClusters(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                  const SV_ULONGLONG offset,
                                  const SV_UINT &length,
                                  const SV_UINT &clustersize, 
                                  bool &bisused);
    SV_UINT GetCountOfContinuousClusters(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                         const SV_UINT &clusternumber,
                                         const SV_UINT &nclusters,
                                         const bool &bisused);
    SV_UINT GetClusterSize(cdp_volume_t &volume, const std::string &fstype, 
                           const SV_ULONGLONG totalsize, const SV_ULONGLONG startoffset,
                           const E_FS_SUPPORT_ACTIONS &efssupportaction,
                           std::string &errmsg);
    bool GetPhysicalSectorSize(const std::string& volumename, SV_UINT& sectorSize) ;

    typedef struct ClusterBitmapCustomizationInfo
    {
        SV_ULONGLONG m_Start;
        SV_UINT m_Count;
        bool m_Set;

        ClusterBitmapCustomizationInfo(const SV_ULONGLONG &start, const SV_UINT &count, const bool &set)
        : m_Start(start),
          m_Count(count),
          m_Set(set)
        {
        }
 
        ClusterBitmapCustomizationInfo(const ClusterBitmapCustomizationInfo &rhs)
        : m_Start(rhs.m_Start),
          m_Count(rhs.m_Count),
          m_Set(rhs.m_Set)
        {
        }

		bool operator==(const ClusterBitmapCustomizationInfo &rhs) const
		{
			return (m_Start==rhs.m_Start) && (m_Count==rhs.m_Count) && (m_Set==rhs.m_Set);
		}

        void Print(void);
    } ClusterBitmapCustomizationInfo_t;
    typedef std::vector<ClusterBitmapCustomizationInfo_t> ClusterBitmapCustomizationInfos_t;
    typedef ClusterBitmapCustomizationInfos_t::const_iterator ConstClusterBitmapCustomizationInfosIter_t;
    typedef ClusterBitmapCustomizationInfos_t::iterator ClusterBitmapCustomizationInfosIter_t;

    void CustomizeClusterBitmap(ClusterBitmapCustomizationInfos_t &infos, VolumeClusterInformer::VolumeClusterInfo &vci);
    void CustomizeClusterBitmap(ClusterBitmapCustomizationInfo_t &info, VolumeClusterInformer::VolumeClusterInfo &vci);
    void SetClusterBits(const SV_UINT &relativeclusternumber, const SV_UINT &count, VolumeClusterInformer::VolumeClusterInfo &vci);
    void UnsetClusterBits(const SV_UINT &relativeclusternumber, const SV_UINT &count, VolumeClusterInformer::VolumeClusterInfo &vci);
    typedef void (cdp_volume_util::*ClusterBitOp_t)(const SV_UINT &relativeclusternumber, const SV_UINT &count, 
                  VolumeClusterInformer::VolumeClusterInfo &vci);

    /* TODO: do not pass volumename using reference since
     * boost::function is unable to store */
    cdp_volume_t *CreateVolume(const std::string volumename);
    void DestroyVolume(cdp_volume_t *pvolume);
	VolumeApplier *CreateSimpleVolumeApplier(VolumeApplier::VolumeApplierFormationInputs_t inputs);
    void DestroySimpleVolumeApplier(VolumeApplier *pvolumeapplier);
    void FormClusterBitmapCustomizationInfos(const svector_t &filenames, const std::string &filesystem, const bool &recurse,
                                             const bool &exclude, ClusterBitmapCustomizationInfos_t &cbcustinfos);
    void CollectClusterBitmapCustomizationInfo(ClusterBitmapCustomizationInfos_t &cbcustinfos,
                                               const FileSystem::ClusterRange_t &cr,
                                               const bool &set);
	bool DoesDataMatch(cdp_volume_t &v, const SV_LONGLONG &offset, const SV_UINT &length, const char *datatocompare);

private:
	ClusterBitOp_t m_ClusterBitOps[NBOOLS];
};

#endif /* CDP_VOLUME_UTIL_H */
