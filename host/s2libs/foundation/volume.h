/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volume.h
 *
 * Description: 
 */
#ifndef LOCAL_VOLUME__H
#define LOCAL_VOLUME__H 

#ifdef SV_WINDOWS
#pragma once
#include <windows.h>
#endif

#include "devicefilter.h"

#include <boost/shared_ptr.hpp>

#include "file.h"
#include "svtypes.h"
#include "volumeclusterinfo.h"
#include "inmdefines.h"

class Volume: public File
{
public:
	typedef boost::shared_ptr<Volume> Ptr;
	Volume();
	virtual ~Volume();
	Volume(const Volume&);
	Volume(const std::string&,const std::string&);
	int Init();
	FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs);
    bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset);
	bool operator==(const Volume&);
	bool operator!=(const Volume&); 
    bool IsReady();
	bool IsFiltered();
    const unsigned long int GetBytesPerSector();
    static SV_LONGLONG GetFormattedVolumeSize(const std::string&);
    bool Hide();
	const std::string& GetMountPoint() const;
	SV_ULONG LastError() const;
#ifdef SV_WINDOWS
    const wchar_t* GetVolumeGUID() const;
#elif SV_UNIX
    const char* GetVolumeGUID() const;
#endif
    int VolumeGUIDLength() const; 
	bool OpenExclusive();
    static SV_ULONGLONG GetFreeBytes(const std::string&);
	static SV_ULONGLONG GetRawVolumeSize(const std::string&);
    SV_INT Open(BasicIo::BioOpenMode mode);
    /* returns 0 on failure */
    SV_ULONGLONG GetNumberOfClusters(void);
    /* returns false on failure for now */
    bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci);
    void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci);
    SV_UINT GetPhysicalSectorSize(void);

	/// \brief function to return size
	///
	/// \note on windows, it returns the file system size for volumes
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	SV_ULONGLONG GetSize(void);

private:
	Volume& operator=(const Volume&);
	static Lockable m_FreeByteLock;
    static Lockable m_TotalByteLock;
protected:
	std::string m_sMountPoint;
#ifdef SV_WINDOWS
    wchar_t m_sVolumeGUID[GUID_SIZE_IN_CHARS+1]; /* Need to include length for
	the null char. else wcslen will crash. */
#elif SV_UNIX
	char m_sVolumeGUID[GUID_SIZE_IN_CHARS];
#endif
	bool m_bIsFilteredVolume;
    VolumeClusterInformer *m_pVolumeClusterInformer;
    std::string m_sFileSystem;
};

#endif // LOCAL_VOLUME__H

