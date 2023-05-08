#ifndef _VOLUMEREPORTER__H_
#define _VOLUMEREPORTER__H_

#include <map>
#include <set>
#include <vector>
#include <string>
#include "volumegroupsettings.h"

/* interface class that gives volume report */
class VolumeReporter
{
public:
    /* volume write state; writable is 0, all errors are non zero */
    typedef enum eWriteState
    {
        Writable = 0, ErrUnknown,
        NotCollected, ExtendedPartition,
        Mounted, SystemVolume,
        CacheVolume, Locked,
        ContainsPageFile, StartAtBlockZero,
        UnderVolumeManager
         
    }WriteState_t;

    typedef std::vector<WriteState_t> WriteStates_t;
    typedef WriteStates_t::const_iterator ConstWriteStateIter_t;
     
    struct Entity_t
    {
        /* entity because of which write state is not writable */
        typedef enum eEntityType
        {
            EntityUnknown,
            EntityVolume,
            EntityParent,
            EntityChild,
            EntityMultipath

        } EntityType_t;
    
        EntityType_t m_Type;
        std::string m_Name;
    
        Entity_t()
        {
            m_Type = EntityUnknown;
        }

        Entity_t(const EntityType_t type, const std::string &name)
        {
            m_Type = type;
            m_Name = name;
        }
    };
  
    struct EntityLess
    {
        bool operator()(const Entity_t &lhs, const Entity_t &rhs) const
        {
            if (lhs.m_Type == rhs.m_Type)
            {
                return lhs.m_Name < rhs.m_Name;
            }
            else
            {
                return lhs.m_Type < rhs.m_Type;
            }
        }
    };
 
    typedef std::map<Entity_t, WriteStates_t, EntityLess> EntityWriteStates_t;
    typedef EntityWriteStates_t::const_iterator ConstEntityWriteStatesIter_t;
    typedef EntityWriteStates_t::iterator EntityWriteStatesIter_t;

    typedef struct stTopLv
    {
        std::string m_Name;
        std::string m_Vg;
        VolumeSummary::Vendor m_Vendor;
 
        stTopLv()
        {
            m_Vendor = VolumeSummary::UNKNOWN_VENDOR;
        }

        stTopLv(const std::string &name, const std::string &vg, 
                const VolumeSummary::Vendor vendor): m_Name(name),
                                                     m_Vg(vg),
                                                     m_Vendor(vendor)
        {
        }

    }TopLv_t;
    typedef std::vector<TopLv_t> TopLvs_t;
    typedef TopLvs_t::const_iterator ConstTopLvsIter_t;

    typedef enum eReportLevel
    {
        ReportFullLevel,
        ReportVolumeLevel

    } ReportLevel_t;

    /* write information for a volume */
    typedef struct stVolumeReport
    {
        ReportLevel_t m_ReportLevel;
        bool m_bIsReported;
        EntityWriteStates_t m_EntityWriteStates;
        VolumeSummary::Devicetype m_VolumeType;
        VolumeSummary::FormatLabel m_FormatLabel;
        VolumeSummary::Vendor m_Vendor;
        std::string m_Vg;
        VolumeSummary::Vendor m_VgVendor;
        std::string m_FileSystem;
        bool m_IsMounted;
        std::string m_Mountpoint;
        SV_ULONGLONG m_Size;
        SV_ULONGLONG m_StartOffset;
        bool m_bIsMultipathNode;
        std::set<std::string> m_Multipaths;
        std::set<std::string> m_Parents;
        std::set<std::string> m_Children;
        std::set<std::string> m_LvComponents;        /* disks/partitions inside an lv */
        TopLvs_t m_TopLvs;                           /* lvs made out of the volume */
        std::string m_Id;
		std::string m_DeviceName;                    ///< device name for a disk (of form \\.\PhysicalDrive)
		std::string m_StorageType;                   ///< basic or dynamic for windows
		bool m_IsShared;                             ///< true if this disk is shared
		VolumeSummary::NameBasedOn m_nameBasedOn;    ///< SIGNATURE if the device name used for identifying (on CS) is prepared using disk guid
		                                             ///< SCSIID if the device name used for identifying (on CS) is prepared using scsi id
    
        stVolumeReport()
        {
            reset();
        }

        void clear(void)
        {
            reset();
            m_EntityWriteStates.clear();
            m_Vg.clear();
            m_FileSystem.clear();
            m_Mountpoint.clear();
            m_Multipaths.clear();
            m_Parents.clear();
            m_Children.clear();
            m_LvComponents.clear();
            m_TopLvs.clear();
            m_Id.clear();
			m_DeviceName.clear();
			m_StorageType.clear();
        }
    
		bool usingScsiId() const
		{
			return (m_nameBasedOn == VolumeSummary::SCSIID);
		}
    
    private:
        void reset(void)
        {
            m_ReportLevel = ReportFullLevel;
            m_bIsReported = false;
            m_VolumeType = VolumeSummary::UNKNOWN_DEVICETYPE;
            m_FormatLabel = VolumeSummary::LABEL_UNKNOWN;
            m_Vendor = VolumeSummary::UNKNOWN_VENDOR;
            m_VgVendor = VolumeSummary::UNKNOWN_VENDOR;
			m_nameBasedOn = VolumeSummary::SIGNATURE;
            m_bIsMultipathNode = false;
            m_Size = 0;
            m_StartOffset = 0;
            m_IsMounted = false;
			m_IsShared = false;
        }

    } VolumeReport_t;

    /* collected volumes list */
    typedef struct stCollectedVolumes
    {
        std::set<std::string> m_Volumes;

    } CollectedVolumes_t;

    typedef std::map<WriteState_t, const char *> WriteStateToMsg_t;
    typedef std::map<Entity_t::EntityType_t, const char *> EntityTypeToMsg_t;

    typedef struct stPartitionReport
    {
        std::string m_Name;
        SV_LONGLONG m_StartOffset;   /* in bytes */
        SV_LONGLONG m_Size;          /* in bytes */

        stPartitionReport()
        {
            m_StartOffset = 0;
            m_Size = 0;
        }

    } PartitionReport_t;
    typedef std::vector<PartitionReport_t> PartitionReports_t;
    typedef PartitionReports_t::const_iterator ConstPartitionReportsIter_t;

    typedef struct stDiskReport
    {
        std::string m_Id;
        std::string m_Name;
        SV_LONGLONG m_Size;          /* in bytes */
        SV_LONGLONG m_SectorSize;    /* in bytes */
        SV_LONGLONG m_RawCapacity;   /* in bytes */
        Attributes_t attributes;
        PartitionReports_t m_PartitionReports;

        stDiskReport()
        {
            m_Size = 0;
            m_SectorSize = 0;
        }
        
    } DiskReport_t;
    typedef std::vector<DiskReport_t> DiskReports_t;
    typedef DiskReports_t::const_iterator ConstDiskReportsIter_t;

    virtual void GetOsDiskReport(const std::string &diskId,
                                 const VolumeSummaries_t &vs,
                                 DiskReports_t &diskreports) = 0;
                                      
    virtual void GetBootDiskReport(const VolumeSummaries_t &vs,
                                 DiskReports_t &diskreports) = 0;
                                      
    virtual void GetVolumeReport(const std::string &volumename, 
                                 const VolumeSummaries_t &vs,
                                 VolumeReport_t &volumereport) = 0;
    virtual void GetCollectedVolumes(const VolumeSummaries_t &vs, CollectedVolumes_t &collectedvolumes) = 0;
    virtual std::string GetWriteStateStr(const WriteState_t ws) = 0;
    virtual std::string GetEntityTypeStr(const Entity_t::EntityType_t et) = 0;
    void PrintVolumeReport(const VolumeReport_t &volumereport);
    void PrintCollectedVolumes(const CollectedVolumes_t &collectedvolumes);
    virtual ~VolumeReporter() {}
	void PrintDiskReports(const DiskReports_t &diskreports);
    void PrintDiskReport(const DiskReport_t &diskreport);
    void PrintPartitionReports(const PartitionReports_t &partitionreports);
    void PrintPartitionReport(const PartitionReport_t &partitionreport);


};

#endif  /* _VOLUMEREPORTER__H_ */

