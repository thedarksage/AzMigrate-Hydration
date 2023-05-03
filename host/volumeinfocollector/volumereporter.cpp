#include <string>
#include <sstream>
#include "volumereporter.h"
#include "logger.h"
#include "portablehelpers.h"

void VolumeReporter::PrintVolumeReport(const VolumeReport_t &volumereport)
{
    std::stringstream ssrl;

    ssrl << volumereport.m_ReportLevel;
    DebugPrintf(SV_LOG_DEBUG, "volume report information:\n");
    DebugPrintf(SV_LOG_DEBUG, "entity write states:\n");
    for (ConstEntityWriteStatesIter_t i = volumereport.m_EntityWriteStates.begin(); i != volumereport.m_EntityWriteStates.end(); i++)
    {
        const Entity_t &e = i->first;
        DebugPrintf(SV_LOG_DEBUG, "Entity:\n");
        std::stringstream sstype;
        sstype << e.m_Type;
        DebugPrintf(SV_LOG_DEBUG, "---------------\n");
        DebugPrintf(SV_LOG_DEBUG, "Type: %s\n", sstype.str().c_str());
        DebugPrintf(SV_LOG_DEBUG, "Name: %s\n", e.m_Name.c_str());
        DebugPrintf(SV_LOG_DEBUG, "Write States:\n");
        const WriteStates_t &wss = i->second;
        for (ConstWriteStateIter_t w = wss.begin(); w != wss.end(); w++)
        {
            std::stringstream ssws;
            ssws << *w;
            DebugPrintf(SV_LOG_DEBUG, "%s\n", ssws.str().c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "---------------\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "report level: %s\n", ssrl.str().c_str());
    DebugPrintf(SV_LOG_DEBUG, "is collected: %s\n", STRBOOL(volumereport.m_bIsReported));
    DebugPrintf(SV_LOG_DEBUG, "volume type: %s\n", StrDeviceType[volumereport.m_VolumeType]);
    DebugPrintf(SV_LOG_DEBUG, "format label: %s\n", StrFormatLabel[volumereport.m_FormatLabel]);
    DebugPrintf(SV_LOG_DEBUG, "vendor: %s\n", StrVendor[volumereport.m_Vendor]);
    DebugPrintf(SV_LOG_DEBUG, "volume group: %s\n", volumereport.m_Vg.c_str());
    DebugPrintf(SV_LOG_DEBUG, "volume group vendor: %s\n", StrVendor[volumereport.m_VgVendor]);
    DebugPrintf(SV_LOG_DEBUG, "file system: %s\n", volumereport.m_FileSystem.c_str());
    DebugPrintf(SV_LOG_DEBUG, "mounted: %s\n", volumereport.m_IsMounted ? "yes" : "no");
    DebugPrintf(SV_LOG_DEBUG, "mount point: %s\n", volumereport.m_Mountpoint.c_str());
    DebugPrintf(SV_LOG_DEBUG, "size: " ULLSPEC "\n", volumereport.m_Size);
    DebugPrintf(SV_LOG_DEBUG, "start offset: " ULLSPEC "\n", volumereport.m_StartOffset);
    DebugPrintf(SV_LOG_DEBUG, "is multipath node: %s\n", STRBOOL(volumereport.m_bIsMultipathNode));
    DebugPrintf(SV_LOG_DEBUG, "multipaths:\n");
    for_each(volumereport.m_Multipaths.begin(), volumereport.m_Multipaths.end(), PrintString);
    DebugPrintf(SV_LOG_DEBUG, "parents:\n");
    for_each(volumereport.m_Parents.begin(), volumereport.m_Parents.end(), PrintString);
    DebugPrintf(SV_LOG_DEBUG, "children:\n");
    for_each(volumereport.m_Children.begin(), volumereport.m_Children.end(), PrintString);
    DebugPrintf(SV_LOG_DEBUG, "components of logical volume:\n");
    for_each(volumereport.m_LvComponents.begin(), volumereport.m_LvComponents.end(), PrintString);
    DebugPrintf(SV_LOG_DEBUG, "top logical volumes formed:\n");
    for (ConstTopLvsIter_t iter = volumereport.m_TopLvs.begin(); iter != volumereport.m_TopLvs.end(); iter++)
    {
        const TopLv_t &tlv = *iter;
        DebugPrintf(SV_LOG_DEBUG, "====================\n");
        DebugPrintf(SV_LOG_DEBUG, "name: %s\n", tlv.m_Name.c_str());
        DebugPrintf(SV_LOG_DEBUG, "volume group: %s\n", tlv.m_Vg.c_str());
        DebugPrintf(SV_LOG_DEBUG, "vendor: %s\n", StrVendor[tlv.m_Vendor]);
        DebugPrintf(SV_LOG_DEBUG, "====================\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "id: %s\n", volumereport.m_Id.c_str());
	DebugPrintf(SV_LOG_DEBUG, "device name: %s\n", volumereport.m_DeviceName.c_str());
	DebugPrintf(SV_LOG_DEBUG, "storage type: %s\n", volumereport.m_StorageType.c_str());
	DebugPrintf(SV_LOG_DEBUG, "is shared: %s\n", STRBOOL(volumereport.m_IsShared));
}


void VolumeReporter::PrintCollectedVolumes(const CollectedVolumes_t &collectedvolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "list of collected volumes:\n");
    for_each(collectedvolumes.m_Volumes.begin(), collectedvolumes.m_Volumes.end(), PrintString);
}


void VolumeReporter::PrintDiskReports(const DiskReports_t &diskreports)
{
    DebugPrintf(SV_LOG_DEBUG, "list of collected disks:\n");
    for (ConstDiskReportsIter_t diter = diskreports.begin(); diter != diskreports.end(); diter++)
    {
        PrintDiskReport(*diter);
    }
}


void VolumeReporter::PrintDiskReport(const DiskReport_t &diskreport)
{
    std::stringstream msg;
    msg << "Disk: name = "   << diskreport.m_Name
        << ", size = " << diskreport.m_Size
        << ", sector size = " << diskreport.m_SectorSize;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
    PrintPartitionReports(diskreport.m_PartitionReports);
}


void VolumeReporter::PrintPartitionReports(const PartitionReports_t &partitionreports)
{
    DebugPrintf(SV_LOG_DEBUG, "list of collected partitions:\n");
    for (ConstPartitionReportsIter_t piter = partitionreports.begin(); piter != partitionreports.end(); piter++)
    {
        PrintPartitionReport(*piter);
    }
}


void VolumeReporter::PrintPartitionReport(const PartitionReport_t &partitionreport)
{
    std::stringstream msg;
    msg << "Partition: name = "   << partitionreport.m_Name
        << ", start offset = " << partitionreport.m_StartOffset
        << ", size = " << partitionreport.m_Size;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
}

