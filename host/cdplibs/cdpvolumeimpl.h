/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : cdpvolumeimpl.h
 *
 * Description: 
 */
#ifndef CDPVOLUMEIMPL__H
#define CDPVOLUMEIMPL__H 

#include <boost/shared_ptr.hpp>
#include "basicio.h"
#include "svtypes.h"
#include "volumeclusterinfo.h"
#include "inmdefines.h"
#include "fssupportactions.h"
#include "portablehelpers.h"

#include <string>
#include <map>

class cdp_volumeimpl_t
{
public:
    cdp_volumeimpl_t(){}
    virtual ~cdp_volumeimpl_t(){}

	virtual VOLUME_STATE GetDeviceState() = 0;
	virtual bool Hide() = 0;	
	virtual SV_ULONG LastError() const = 0;
   	virtual bool OpenExclusive() = 0;
    virtual SV_INT Open(BasicIo::BioOpenMode mode) = 0;
    virtual BasicIo::BioOpenMode OpenMode(void) = 0;
    virtual SV_UINT Read(char* buffer, SV_UINT length) = 0;
    virtual SV_UINT FullRead(char* buffer, SV_UINT length) = 0;
    virtual SV_UINT Write(char const * buffer, SV_UINT length) = 0;
    virtual SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from) = 0;
    virtual SV_LONGLONG Tell() const = 0;
    virtual SV_INT Close() = 0;
    virtual bool Good() const = 0;
    virtual bool Eof() const = 0;
    virtual void ClearError() = 0;
    virtual bool isOpen() = 0;
    virtual void SetLastError(SV_ULONG liError) = 0;
    virtual ACE_HANDLE& GetHandle() = 0;
    virtual void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay) = 0;
    virtual void DisableRetry() = 0;
    virtual bool IsInitialized(void) = 0;
    virtual bool FlushFileBuffers() = 0;
    virtual FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs) = 0;
    virtual bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset) = 0;
    virtual SV_ULONGLONG GetNumberOfClusters(void) = 0;
    virtual bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci) = 0;
    virtual void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci) = 0;
    virtual SV_UINT GetPhysicalSectorSize(void) = 0;

	/// \brief function to return size
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	virtual SV_ULONGLONG GetSize(void) = 0;
private:
    cdp_volumeimpl_t(cdp_volumeimpl_t const & );
    cdp_volumeimpl_t& operator=(cdp_volumeimpl_t const & );
};

#endif 
