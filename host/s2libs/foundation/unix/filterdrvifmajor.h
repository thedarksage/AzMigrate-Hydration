#ifndef _FILTERDRVMAJOR__IF__H_
#define _FILTERDRVMAJOR__IF__H_

#include <vector>
#include <string>
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
    bool FillInmDevInfo(const DeviceFilterFeatures *pinvolfltfeatures, const VolumeSummaries_t *pvolumesummaries,
                        const VOLUME_SETTINGS &volumesettings, inm_dev_info_t &devinfo, 
                        VolumeReporter::VolumeReport_t &vr);
    bool FillInmDevInfo(inm_dev_info_t &devinfo, 
                        const VolumeReporter::VolumeReport_t &vr);
    void PrintInmDevInfo(const inm_dev_info_t &devinfo);
    int FillVolumeStats(const FilterDrvVol_t &volume, DeviceStream &devstream, VolumeStats &vs);

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

    bool GetVolumeAttribute(const std::string &devicename, DeviceStream *pDeviceStream, char *buf, 
                            const size_t &buflen, const unsigned int &attribute, int &errorcode);
    void PrintInmAttributeToStream(const inm_attribute_t &ia, std::ostream &o);
    bool GetVolumeBlockSize(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &blksz, std::string &reasonifcant);
    bool GetNumberOfBlocksInVolume(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &nblks, std::string &reasonifcant);

	/// \brief returns driver name to use based on source device type
	std::string GetDriverName(const int &deviceType); ///< device type (VOLUME/DISK etc,.)

	/// \brief returns formatted source device input name required by driver
	FilterDrvVol_t GetFormattedDeviceNameForDriverInput(const std::string &deviceName, ///< device name
		const int &deviceType                                                          ///< device type (VOLUME/DISK etc,.)
		);

    bool GetDriverStats(const std::string &devicename,
        const int &devicetype,
        DeviceStream *pDeviceStream,
        void* stats,
        int   statslen);

    bool GetDriverVersion(DeviceStream *pDeviceStream, DRIVER_VERSION &stVersion);

    /// \brief waits for the CX session
    /// this is a blocking call, the caller should cancel the event
    /// for this call to return while quitting.
    bool GetCxStats(DeviceStream *pDeviceStream,
        void *pCxNotify,
        uint32_t ulInputSize);

    /// \brief cancels a cx stats call that is waiting on this device
    bool CancelCxStats(DeviceStream *pDeviceStream);

    /// \brief waits for the issued tags to be commited/drained
    /// this is a blocking call, the caller should cancel the event
    /// for this call to return while quitting.
    bool NotifyTagCommit(DeviceStream *pDeviceStream,
        void *pCxNotify,
        uint32_t ulInputSize);

    /// \brief cancels a notify tag commit call that is waiting on this device
    bool CancelTagCommitNotify(DeviceStream *pDeviceStream);

    /// \brief Ioctl to unblock draining for multiple disks
    bool UnblockDrain(DeviceStream *pDeviceStream,
        void *unblockDrainBuffer,
        uint32_t ulInputSize);

    /// \brief Ioctl to get draining status for multiple disks
    bool GetDrainState(DeviceStream *pDeviceStream,
        void *drainStateBuffer,
        uint32_t ulInputSize);

    /// \brief Ioctl to modify persistent device name of a disk
    bool ModifyPersistentDeviceName(DeviceStream *pDeviceStream,
        void *modifyPnameBuffer,
        uint32_t ulInputSize);

    /// \brief Ioctl to get volume name map
    bool GetVolNameMap(DeviceStream *pDeviceStream,
        void *volNameMapBuffer,
        uint32_t ulInputSize);

private:
    bool FillVolumeAttrs(const char *dev, const VolumeSummaries_t *pvolumesummaries, inm_dev_info_t &devinfo, VolumeReporter::VolumeReport_t &vr);
};

#endif /* _FILTERDRV__IF__H_ */

