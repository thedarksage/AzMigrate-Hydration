#include "cdpsnapshot.h"
#include "error.h"
#include "cdpv1database.h"
#include "cdpv1globals.h"
#include "cdputil.h"
#include "cdpplatform.h"

#include "VsnapUser.h"
#include "inmcommand.h"
#include "portablehelpersmajor.h"

#include "inmageex.h"

#include "configwrapper.h"
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"

bool SnapshotTask::IsValidDestination(const std::string & volumename, std::string &errormsg)
{
	DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", FUNCTION_NAME);
	std::string devname = volumename;
    bool rv = true;
    boost::shared_ptr<VolumeReporter> volinfo(new VolumeReporterImp());
    VolumeReporter::VolumeReport_t volume_write_info;
	VolumeSummaries_t volumeSummaries;
    VolumeDynamicInfos_t volumeDynamicInfos;
	VolumeInfoCollector volumeCollector;
	volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false); 
    volinfo->GetVolumeReport(devname,volumeSummaries,volume_write_info);
    std::stringstream out;
	do
	{
        VolumeReporter::EntityWriteStatesIter_t entityIter = volume_write_info.m_EntityWriteStates.begin();
        VolumeReporter::EntityWriteStatesIter_t entityEndIter = volume_write_info.m_EntityWriteStates.end();
        for(;entityIter != entityEndIter; ++entityIter)
        {
            VolumeReporter::WriteStates_t::iterator writeStateIter = entityIter->second.begin();
            VolumeReporter::WriteStates_t::iterator writeStateEndIter = entityIter->second.end();
            for(;writeStateIter != writeStateEndIter;++writeStateIter)
            {
                if(((*writeStateIter) != VolumeReporter::Mounted) &&
                    ((*writeStateIter) != VolumeReporter::Locked) &&
                    ((*writeStateIter) != VolumeReporter::Writable))
                {
                    out << volinfo->GetEntityTypeStr(entityIter->first.m_Type) << " " << entityIter->first.m_Name;
                    if(entityIter->first.m_Type != VolumeReporter::Entity_t::EntityVolume)
                        out << " of device " << devname;
                    out << " " << volinfo->GetWriteStateStr((*writeStateIter)) << "\n";
                    errormsg = out.str();
                    rv = false;
                    break;
						}
						}
            if(!rv)
						break;
						}
        if(!rv)
            break;

        if(!volume_write_info.m_TopLvs.empty())
						{
							errormsg = "snapshot cannot be taken on ";
							errormsg += devname;
            errormsg += ". It contains logical volumes.\n";
						rv = false;
						break;
						}
        if(!IsDiskOrPartitionValidDestination(volume_write_info.m_Parents,errormsg))
				{
            rv = false;
						break;
					}
        if(!IsDiskOrPartitionValidDestination(volume_write_info.m_Children,errormsg))
					{
						rv = false;
						break;
					}
        if(!IsValidDeviceToProceed(volumename,errormsg))
				{
            rv = false;
				break;
			}
        std::set<std::string>::const_iterator volumeIterator(volume_write_info.m_Multipaths.begin());
        std::set<std::string>::const_iterator volumeIteratorEnd(volume_write_info.m_Multipaths.end());
        for(; volumeIterator != volumeIteratorEnd; volumeIterator++)
		{
            if(!IsValidDeviceToProceed((*volumeIterator),errormsg))
		{
			rv = false;
			break;
		}
        }

	}while(false);

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
	return rv;
}

/*
 * Description: Not supported for non windows platforms. Return false.
 */
bool SnapshotTask::	get_used_block_information(std::string devicename,std::string destname, BlockUsageInfo &blockusageinfo)
{
	return false;
}

