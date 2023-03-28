#ifndef _FILTERDRVMAJOR__IF__H_
#define _FILTERDRVMAJOR__IF__H_

#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#include "devicefilter.h"
#include "volumegroupsettings.h"
#include "devicefilterfeatures.h"
#include "filterdrvif.h"
#include "filterdrvvolmajor.h"
#include "devicestream.h"
#include "volumereporter.h"

class FilterDrvIfMajor : public FilterDrvIf
{
public:
    FilterDrvIfMajor();
    int FillVolumeStats(const FilterDrvVol_t &volume, DeviceStream &devstream, VolumeStats &vs);
	int getDriverTime(DeviceStream &devStream, PDRIVER_GLOBAL_TIMESTAMP pTime);

	/// \brief gets filter driver's tracking size
	///
	/// \return
	/// \li \c true : if got successfully
	/// \li \c false : could not get
	bool GetVolumeTrackingSize(const std::string &devicename, ///< device name
		const int &devicetype,                                ///< device type (VOLUME/DISK etc,.)
		DeviceStream *pDeviceStream,                          ///< filter driver stream
		SV_ULONGLONG &size,                                   ///< out: size
		std::string &reasonifcant                             ///< reason if could not get tracking size
		);

	/// \brief deletes filter driver's tracking size of a filtered device
	///
	/// \return
	/// \li \c true : deleted
	/// \li \c false : not deleted
	bool DeleteVolumeTrackingSize(const std::string &devicename, ///< device name
		const int &devicetype,                                   ///< device type (VOLUME/DISK etc,.)  
		DeviceStream *pDeviceStream,                             ///< filter driver stream
		std::string &reasonifcant                                ///< reason if could not delete
		);

	/// \brief compares driver's tracking size of device to its current size
	///
	/// \return
	/// \li \c true : match
	/// \li \c false : unmatch
	bool DoesVolumeTrackingSizeMatch(const int &devicetype, ///< device type (VOLUME/DISK etc,.)  
		const SV_ULONGLONG &trackingsize,                   ///< driver's tracking size of device
		const SV_ULONGLONG &capacity,                       ///< capacity
		const SV_ULONGLONG &rawsize                         ///< raw capacity
		);

	/// \brief returns driver name to use based on source device type
	std::string GetDriverName(const int &deviceType); ///< device type (VOLUME/DISK etc,.)

	/// \brief returns formatted source device input name required by driver
	FilterDrvVol_t GetFormattedDeviceNameForDriverInput(const std::string &deviceName, ///< device name
		const int &deviceType                                                          ///< device type (VOLUME/DISK etc,.)
		);

	/// \brief fills the start filtering input
	///
	/// \return
	/// \li \c true: filled successfully
	/// \li \c true: not filled
	bool FillStartFilteringInput(const FilterDrvVol_t &driverInputDeviceName, ///< driver input device name
		const DeviceFilterFeatures *pDeviceFilterFeatures,                    ///< driver features
		const VolumeSummaries_t *pvolumesummaries,                            ///< volumes cache
		const VOLUME_SETTINGS &volumesettings,                                ///< volume protection settings
		START_FILTERING_INPUT &sfi                                            ///< out: start filtering input to pass to ioctl
		);

    bool FillStartFilteringInput(const FilterDrvVol_t &driverInputDeviceName, ///< driver input device name
        const VolumeReporter::VolumeReport_t &volumereport,                   ///< volume report 
        START_FILTERING_INPUT &sfi                                            ///< out: start filtering input to pass to ioctl
        );
	/// \brief prints the start filtering input
	void PrintStartFilteringInput(const START_FILTERING_INPUT &sfi);  ///< start filtering input

    bool GetDriverStats(const std::string &devicename, ///< device name
        const int &devicetype,                         ///< device type (VOLUME/DISK etc,.)  
        DeviceStream *pDeviceStream,
        void *buffer,
        DWORD buflen);

    bool GetDriverVersion(DeviceStream *pDeviceStream,
        DRIVER_VERSION &dv);

    /// \brief this is a blocking call, the caller should cancel the event
    /// for this call to return while quitting.
    bool GetCxStats(DeviceStream *pDeviceStream,
        GET_CXFAILURE_NOTIFY *pCxNotify,
        uint32_t ulInputSize,
        VM_CXFAILURE_STATS *pCxStats,
        uint32_t ulOutputSize);

    /// \brief cancels a cx stats call that is waiting on this device
    bool CancelCxStats(DeviceStream *pDeviceStream);

    /// \brief this is a blocking call, the caller should cancel the event
/// for this call to return while quitting.
    bool NotifyTagCommit(DeviceStream *pDeviceStream,
        TAG_COMMIT_NOTIFY_INPUT *pTagCommitNotifyInput,
        uint32_t ulInputSize,
        TAG_COMMIT_NOTIFY_OUTPUT *pTagCommitNotifyOutput,
        uint32_t ulOutputSize);

    /// \brief cancels a cx stats call that is waiting on this device
    bool CancelTagCommitNotify(DeviceStream *pDeviceStream);

    /// \brief Ioctl to unblock draining for multiple disks
    bool UnblockDrain(DeviceStream *pDeviceStream,
        SET_DRAIN_STATE_INPUT *pSetDrainStateInput,
        uint32_t ulInputSize,
        SET_DRAIN_STATE_OUTPUT *pSetDrainStateOutput,
        uint32_t ulOutputSize);

    /// \brief Ioctl to get draining status for multiple disks
    bool GetDrainState(DeviceStream *pDeviceStream,
        GET_DISK_STATE_INPUT *pGetDiskStateInput,
        uint32_t ulInputSize,
        GET_DISK_STATE_OUTPUT *pGetDiskStateOutput,
        uint32_t ulOutputSize);

    /// \brief gets flags for the given device
    bool
    GetVolumeFlags(
        const std::string &devicename,
        int devicetype,
        DeviceStream *pDeviceStream,
        ULONG &flags);

    /// \brief notify system state to the driver
    bool
    NotifySystemState(
        DeviceStream *pDeviceStream,
        bool bAreBitmapFilesEncryptedOnDisk);
};

#endif /* _FILTERDRV__IF__H_ */

