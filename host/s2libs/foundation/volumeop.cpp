#include "volumeop.h"
#include "error.h"
#include "cxmediator.h"
#include "logger.h"
#include "filterdrvifmajor.h"
#include "volume.h"

namespace VolumeOps
{
    int VolumeOperator::UpdateVolumesPendingChanges(const VolToFilterDrvVol_t &volumes, Configurator* cxConfigurator, 
                                                    DeviceStream &devstream, QuitFunction_t qf,
													std::list<std::string>& failedvols)
    {
        int iStatus = volumes.empty() ? SV_FAILURE : SV_SUCCESS;
        FilterDrvIfMajor drvif;
        VolumesStats_t vss;
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
        for (ConstVolToFilterDrvVolIter_t iter = volumes.begin(); (iter != volumes.end()) && !(qf(0)); iter++)
        {
            VolumeStats vs;
            int iFillStatus = drvif.FillVolumeStats(iter->second, devstream, vs);
            /* TODO: even if some ioctls fail, go ahead and report for
             *       the other source volumes */
            if (SV_SUCCESS != iFillStatus)
            {
                DebugPrintf(SV_LOG_ERROR, "For volume %s, fill volume stats failed\n", iter->first.c_str());
				failedvols.push_back( iter->first ) ;
                iStatus = SV_FAILURE;
            }
            else
            {
                //TODO: print the vs ?
                vss.insert(std::make_pair(iter->first, vs));
            }
        }

        if (qf(0))
        {
            DebugPrintf(SV_LOG_DEBUG, "quit is requested when updating volume pending changes\n");
            iStatus = SV_FAILURE;
        }
        else if (!vss.empty())
        {
            CxMediator cxmr;
            int iUpdateStatus = cxmr.UpdateVolumesPendingChanges(cxConfigurator, vss, failedvols);
            if (SV_SUCCESS != iUpdateStatus)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to update pending changes count\n");
                iStatus = SV_FAILURE;
            }
        }
    
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
        return iStatus;
    } 

    int VolumeOperator::GetPendingChanges(std::list<std::string> volumes,
        VolumesStats_t &vss,
        DeviceStream &devstream,
        QuitFunction_t qf,
        std::list<std::string>& failedvols)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        int iStatus = SV_SUCCESS;
        FilterDrvIfMajor drvif;
        std::list<std::string>::iterator volIter = volumes.begin();
        for (; volIter != volumes.end() && !(qf(0)); volIter++)
        {
            VolumeStats vs;
            int iFillStatus = drvif.FillVolumeStats(
                drvif.GetFormattedDeviceNameForDriverInput(*volIter, VolumeSummary::DISK),
                devstream,
                vs);
            if (SV_SUCCESS != iFillStatus)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "For volume %s, fill volume stats failed with %d\n",
                    volIter->c_str(),
                    iFillStatus);
                iStatus = iFillStatus;
                failedvols.push_back(volIter->c_str());
            }
            else
            {
                vss.insert(std::make_pair(*volIter, vs));
            }
        }
        if (qf(0))
        {
            DebugPrintf(SV_LOG_DEBUG, "quit is requested when updating volume pending changes\n");
            iStatus = SV_FAILURE;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return iStatus;
    }
	bool VolumeOperator::IsSourceDeviceCapacityValidInSettings(const std::string &sourcedevicename, const SV_ULONGLONG &currentsourcecapacity, const SV_ULONGLONG &sourcecapacityinsettings,
                                                               const VOLUME_SETTINGS::PROTECTION_DIRECTION &pd, std::string &messageifinvalid)
    {
	    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	    bool isvalid = true;
    
	    if (VOLUME_SETTINGS::PROTECTION_DIRECTION_UNKNOWN == pd) {
		    messageifinvalid = "Protection direction is unknown.";
            isvalid = false;
        } else if (VOLUME_SETTINGS::PROTECTION_DIRECTION_FORWARD == pd) {
		    DebugPrintf(SV_LOG_DEBUG, "For volume %s, capacity found is " ULLSPEC ".\n", sourcedevicename.c_str(), currentsourcecapacity);
		    if (0 == currentsourcecapacity) {
			    messageifinvalid = "Failed to find capacity or it is zero(OK if volume is shared and not online on this node).";
                isvalid = false;
            } else if (currentsourcecapacity != sourcecapacityinsettings) {
			    std::stringstream ss;
			    ss << "Capacity found " << currentsourcecapacity << " does not match capacity in settings " << sourcecapacityinsettings << '.';
			    messageifinvalid = ss.str();
                isvalid = false;
		    } else {
			    DebugPrintf(SV_LOG_DEBUG, "For volume %s, found capacity same as that in settings.\n", sourcedevicename.c_str());
		    }
	    } else {
		    DebugPrintf(SV_LOG_DEBUG, "For volume %s, reverse direction protection is present. "
                                      "In this case, considering the settings capacity are valid.\n", sourcedevicename.c_str());
        }

	    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	    return isvalid;
    }
}

