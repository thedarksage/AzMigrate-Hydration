//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : dpdrivercomm.h
//
// Description: 
//

#ifndef DPDRIVERCOMM_H
#define DPDRIVERCOMM_H

#include <string>

#ifdef SV_WINDOWS
#endif

#include "devicefilter.h"
#include "volumegroupsettings.h"
#include <ace/config-macros.h>
#include "volume.h"
#include <boost/shared_ptr.hpp>
#include "devicefilterfeatures.h"
#include "filterdrvif.h"
#include "filesystem.h"

#include "filterdrvvolmajor.h"
#include "devicestream.h"
#include "volumereporter.h"

class DpDriverComm {
public:
	/// \brief constructor
    DpDriverComm(std::string const & deviceName, ///< device being filtered
		const int &deviceType                    ///< device type (VOLUME/DISK etc,.)
		);

    DpDriverComm(std::string const & deviceName, ///< device being filtered
		const int &deviceType,                   ///< device type (VOLUME/DISK etc,.)
        std::string const & persistentName       ///< device being filtered
		);

    ~DpDriverComm();


	/// \brief Gives start filtering ioctl to driver
    void StartFiltering(const VOLUME_SETTINGS &pVolumeSettings, ///< Volume settings
                        const VolumeSummaries_t *pvs            ///< vic cache
                        );
    void StartFiltering(const VolumeReporter::VolumeReport_t &pVolumeSettings, ///< Volume report
        const VolumeSummaries_t *pvs            ///< vic cache
        );
    void StopFiltering();

    void ClearDifferentials();
    bool ResyncStartNotify(ResyncTimeSettings& rts);
    bool ResyncEndNotify(ResyncTimeSettings& rts);

    bool ResyncStartNotify(ResyncTimeSettings & rts, SV_ULONG&   ulErrorCode);
    bool ResyncEndNotify(ResyncTimeSettings & rts, SV_ULONG&   ulErrorCode);

    SV_ULONGLONG GetSrcStartOffset(const VOLUME_SETTINGS &pVolumeSettings)
    {
        SV_ULONGLONG off = 0;
        FilterDrvIf drvif;
        off = drvif.GetSrcStartOffset(m_pDeviceFilterFeatures.get(), pVolumeSettings);
        return off;
    }

    /* returns s2libs error */
    int GetFilesystemClusters(const std::string &filename, FileSystem::ClusterRanges_t &clusterranges);

#ifdef SV_WINDOWS
    bool SetClusterFiltering();
#endif
protected:

private:
    void Init(const int &deviceType);

    std::string m_DeviceName;

    std::string m_PersistentName;                       ///< a persistent name used for UNIX device name changes 

	FilterDrvVol_t m_DeviceNameForDriverInput;         ///< source device name for input to driver

	DeviceStream::Ptr m_pDeviceStream;                 ///< filter driver device stream 

	DeviceFilterFeatures::Ptr m_pDeviceFilterFeatures; ///< device filter features
};
#endif // ifndef DPDRIVERCOMM_H
