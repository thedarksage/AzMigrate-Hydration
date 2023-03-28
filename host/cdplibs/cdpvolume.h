/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : cdpvolume.h
 *
 * Description: 
 */
#ifndef CDPVOLUME_H
#define CDPVOLUME_H 

#include <string>
#include <boost/shared_ptr.hpp>

#include "svtypes.h"
#include "cdprealvolumeimpl.h"
#include "cdpvolumeimpl.h"
#include "cdpemulatedvolumeimpl.h"
#include "basicio.h"
#include "volumeclusterinfo.h"
#include "inmdefines.h"

#include "volumegroupsettings.h"

class cdp_volume_t
{
public:
	typedef boost::shared_ptr<cdp_volume_t> Ptr;

	/// \brief volume type enums to specify required volume class
	enum CDP_VOLUME_TYPE 
	{
		CDP_REAL_VOLUME_TYPE,   ///< real volume class

		CDP_DUMMY_VOLUME_TYPE,  ///< dummy volume class

		CDP_DISK_TYPE,          ///< disk class

		CDP_EMULATED_VOLUME_TYPE, ///< volpack class

		//Must be always last
		CDP_VOLUME_UNKNOWN_TYPE ///< for error handling
	};

	/// \brief constructor: creates the required volume class
	///
	/// \exception throws INMAGE_EX if invalid volume type is specified
	cdp_volume_t(const std::string &name,                            ///< name
                 bool multisparse = false,                           ///< if true, create a volpack
				 const CDP_VOLUME_TYPE &type = CDP_REAL_VOLUME_TYPE, ///< volume types for non-volpack
				 const VolumeSummaries_t *pVolumeSummariesCache = 0  ///< volumes cache to pass to instantiated class
				 );
	
	~cdp_volume_t();
	
	/// \brief function to return disk type
	///
	/// \return
	/// \li \c CDP_DISK_TYPE: for windows
	/// \li \c CDP_REAL_VOLUME_TYPE: for unix
	static CDP_VOLUME_TYPE GetCdpVolumeTypeForDisk(void);
	static CDP_VOLUME_TYPE GetCdpVolumeType(const VolumeSummary::Devicetype & devType);

    static SV_LONGLONG GetFormattedVolumeSize(const std::string&,bool multisparse = false);
    static SV_ULONGLONG GetRawVolumeSize(const std::string&,bool multisparse = false);

	/// \brief function to return readable volume type
	///
	/// \return
	/// \li \c real volume, dummy or disk
	/// \li \c unknown: On invalid volume type
	static std::string GetStrVolumeType(const CDP_VOLUME_TYPE &type); ///< volume type

	/// \brief returns source volume's open mode required by s2/dataprotection
	///
	/// \return open mode
	static BasicIo::BioOpenMode GetSourceVolumeOpenMode(const int &sourcedevicetype); ///< Device type in volume settings (disk/volume etc,.)

	/// \brief returns source volume's open mode flags based on platform.
	///
	/// \return open mode based on platform
	static BasicIo::BioOpenMode PlatformSourceVolumeOpenModeFlags(const int &sourcedevicetype); ///< Device type in volume settings (disk/volume etc,.)

	VOLUME_STATE GetDeviceState();
	bool IsVolumeLocked();
    bool Hide();
	SV_ULONG LastError() const;
	bool OpenExclusive();
    const std::string& GetName() const{ return m_devicename;}
    // BASIC I/O uses BasicIo class to implement
    SV_INT Open(BasicIo::BioOpenMode mode);
    BasicIo::BioOpenMode OpenMode(void);
    SV_UINT Read(char* buffer, SV_UINT length);
    SV_UINT FullRead(char* buffer, SV_UINT length);
    SV_UINT Write(char const * buffer, SV_UINT length);
    SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from);
    SV_LONGLONG Tell() const;
    SV_INT Close();
    bool Good() const;
    bool Eof() const;
    void ClearError();
    bool isOpen();
    void SetLastError(SV_ULONG liError);
    ACE_HANDLE& GetHandle();

    void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay);
    void DisableRetry();
    bool IsInitialized(void);
    bool FlushFileBuffers();

    FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs);
    bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset);
    SV_ULONGLONG GetNumberOfClusters(void);
    bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci);
    void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci);
    SV_UINT GetPhysicalSectorSize(void);

	/// \brief function to return size
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	SV_ULONGLONG GetSize(void);

private:
	cdp_volume_t& operator=(const cdp_volume_t&);



protected:
	std::string m_devicename;
	bool m_newvirtualvolume;
    boost::shared_ptr<cdp_volumeimpl_t> m_imp;
};

#endif // LOCAL_VOLUME__H

