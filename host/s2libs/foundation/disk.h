///
///  \file  disk.h
///
///  \brief Disk class definition
///

#ifndef DISK_CLASS_H
#define DISK_CLASS_H

#include <boost/shared_ptr.hpp>
#include "file.h"
#include "volumegroupsettings.h"
#include "volumeclusterinfo.h"

/// \brief Disk class
class Disk : public File
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<Disk> Ptr;

	/// \brief Constructor
	Disk(const std::string &name,                        ///< disk name
         const VolumeSummaries_t *pVolumeSummariesCache  ///< volumes cache to get to disk device name from disk name
        ); 


	/// \brief Constructor
	Disk(const std::string &name,                        ///< disk name
		const std::string &displayName,                  ///< display name (\\.\physicaldrive<n>)
		const VolumeSummary::NameBasedOn & nameBasedOn = VolumeSummary::SIGNATURE  ///< whether disk name is signature or scsi id
		);

	/// \brief Destructor
	virtual ~Disk();

	/// \brief opens the disk
	///
	/// \return
	/// \li \c SV_SUCCESS : open succeeded
	/// \li \c SV_FAILURE : open failed; LastError() has the error code
	SV_INT Open(BasicIo::BioOpenMode mode); ///< mode to open

	/// \brief gets size of disk
	///
	/// \return
	/// \li \c non zero size on success
	/// \li \c 0 on failure
	SV_ULONGLONG GetSize(void);

	/// \brief Marks the disk offline in read write mode
	///
	/// \return
	/// \li \c non zero size on success
	/// \li \c 0 on failure
	bool OfflineRW(void);

	/// \brief marks the disk online in read write mode
	///
	/// \return
	/// \li \c non zero size on success
	/// \li \c 0 on failure
	bool OnlineRW(void);

	/// \brief marks the disk online in read write mode
	///
	/// \return
	/// \li \c non zero size on success
	/// \li \c 0 on failure
	bool InitializeAsRawDisk(void);

	/// \brief return the device state 
	/// \return
	///  VOLUME_OFFLINE - if the disk is in offline read write state
	///  VOLUME_VISIBLE_RO - if the disk is in online read only state
	///  VOLUME_VISIBLE_RW - if the disk is in online read write state
	///  VOLUME_UNKNOWN - otherwise
	///

	VOLUME_STATE GetDeviceState(void);

	/// \brief offline the disk and open
	///
	/// \return
	/// \li \c true : open succeeded
	/// \li \c false : open failed; LastError() has the error code
	bool OpenExclusive();


	/// \brief sets file system capabilities
	///
	/// \return 
	/// \li \c E_FEATURENOTSUPPORTED
	FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs); ///< file system
	
	/// \brief sets no file system capabilities
	///
	/// \return
	/// \li \c true on success
	/// \li \c false on failure
	bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset);

	/// \brief gets number of clusters
	///
	/// \return number of clusters
	SV_ULONGLONG GetNumberOfClusters(void);

	/// \brief fills cluster bitmap
	///
	/// \return
	/// \li \c true on success
	/// \li \c false on failure
	bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci);

	/// \brief prints cluster bitmap
	void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci);

	/// \brief returns physical sector size
	///
	/// \return
	/// \li \c physical sector size on success
	/// \li \c 0 on failure
	SV_UINT GetPhysicalSectorSize(void);

private:
	/// \brief verifies that disk signature/GUID got from deviceName matches with GetName()
	///
	/// \return
	/// \li \c SV_SUCCESS : open succeeded
	/// \li \c SV_FAILURE : open failed; LastError() has the error code
	SV_INT VerifyAndOpen(const std::string &diskDeviceName, ///< disk device name of for \\.\PhysicalDrive[n]
                         BasicIo::BioOpenMode &mode,         ///< mode to open
						 const VolumeSummary::NameBasedOn & nameBasedOn ///< what should be used (signature or scsiid or ...) for verification
                         );

	bool OnlineOffline(bool online, bool readOnly, bool persist);

private:
	const VolumeSummaries_t *m_pVolumeSummariesCache; ///< volumes cache to get to disk device name from disk name
	std::string m_displayName;                        /// device display name (\\.\physicaldrive<n>)
	VolumeSummary::NameBasedOn m_nameBasedOn;         /// whether disk name is scsi id or signature

	boost::shared_ptr<VolumeClusterInformer> m_pVolumeClusterInformer;  ///< cluster informer
};

#endif /* DISK_CLASS_H */
