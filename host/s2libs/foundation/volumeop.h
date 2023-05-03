#ifndef VOLUME__OP__H_
#define VOLUME__OP__H_

#include <iterator>
#include <string>
#include <map>

#include "devicestream.h"
#include "inmquitfunction.h"
#include "filterdrvvolmajor.h"
#include "configurator2.h"
#include "volumegroupsettings.h"
#include "basicio.h"

namespace VolumeOps
{
    typedef std::map<std::string, FilterDrvVol_t> VolToFilterDrvVol_t;
    typedef VolToFilterDrvVol_t::const_iterator ConstVolToFilterDrvVolIter_t;

    class VolumeOperator
    {
    public:
        VolumeOperator()
        {
        }
        int UpdateVolumesPendingChanges(const VolToFilterDrvVol_t &voltodrvvol, Configurator* cxConfigurator, 
			DeviceStream &devstream, QuitFunction_t qf, std::list<std::string>& failedvols);

        int GetPendingChanges(std::list<std::string> volumes,
            VolumesStats_t &vss,
            DeviceStream &devstream,
            QuitFunction_t qf,
            std::list<std::string>& failedvols);
		/// \brief verifies if the current source device capacity matches with that of settings
		///
		/// \return 
		/// \li \c true  : if settings capacity is valid
		/// \li \c false : if invalid
		bool IsSourceDeviceCapacityValidInSettings(const std::string &sourcedevicename,             ///< source name
                                                   const SV_ULONGLONG &currentsourcecapacity,       ///< current capacity
												   const SV_ULONGLONG &sourcecapacityinsettings,    ///< capacity from settings
												   const VOLUME_SETTINGS::PROTECTION_DIRECTION &pd, ///< protection direction: forward/reverse
												   std::string &messageifinvalid                    ///< error message if capacity is invalid
												   );

		//For Collecting OMS Statisitcs and DR Metrics
		int GetTotalTrackedBytes(DeviceStream& stream, const FilterDrvVol_t &sourceName, SV_ULONGLONG& ullTotalTrackedBytes);
		int GetDroppedTagsCount(DeviceStream& stream, const FilterDrvVol_t &sourceName, SV_ULONGLONG& ullDroppedTagsCount);
    };
}

#endif /* VOLUME__OP__H_ */

