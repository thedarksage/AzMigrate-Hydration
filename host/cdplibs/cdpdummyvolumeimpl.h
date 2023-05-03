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
#ifndef CDPDUMMYVOLUMEIMPL__H
#define CDPDUMMYVOLUMEIMPL__H 

#include "basicio.h"
#include "cdpvolumeimpl.h"
#include "svtypes.h"
#include "cdpdrtd.h"

#include <boost/shared_ptr.hpp>
#include <string>
#include <map>

class cdp_dummyvolumeimpl_t :public cdp_volumeimpl_t
{
public:
	cdp_dummyvolumeimpl_t() :m_invalidhandle(ACE_INVALID_HANDLE) {}
	virtual ~cdp_dummyvolumeimpl_t(){}

	virtual VOLUME_STATE GetDeviceState()    { return VOLUME_HIDDEN; }
	virtual bool Hide() { return true;  }
	virtual SV_ULONG LastError() const { return 0; }
	virtual bool OpenExclusive() { return true;  }
	virtual SV_INT Open(BasicIo::BioOpenMode mode) { return SV_SUCCESS; }
	virtual BasicIo::BioOpenMode OpenMode(void) { return 0; }
	virtual SV_UINT Read(char* buffer, SV_UINT length) { return 0; }
	virtual SV_UINT FullRead(char* buffer, SV_UINT length) { return 0; }
	virtual SV_UINT Write(char const * buffer, SV_UINT length) { return 0; }
	virtual SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from) { return 0; }
	virtual SV_LONGLONG Tell() const { return 0; }
	virtual SV_INT Close() { return 0; }
	virtual bool Good() const { return true; }
	virtual bool Eof() const { return true; }
	virtual void ClearError() { return; }
	virtual bool isOpen() { return true; }
	virtual void SetLastError(SV_ULONG liError) { return; }
	virtual ACE_HANDLE& GetHandle() { return m_invalidhandle; }
	virtual void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay) { return; }
	virtual void DisableRetry() { return; }
	virtual bool IsInitialized(void) { return true; }
	virtual bool FlushFileBuffers() { return false; }
	virtual FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs) { return  E_FEATURECANTSAY; }
	virtual bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset){ return false; }
	virtual SV_ULONGLONG GetNumberOfClusters(void) { return 0; }
	virtual bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci) { return false; }
	virtual void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci) { return; }
	virtual SV_UINT GetPhysicalSectorSize(void) { return 0; }

	/// \brief function to return size
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	virtual SV_ULONGLONG GetSize(void) { return 0; }

private:
	cdp_dummyvolumeimpl_t(cdp_dummyvolumeimpl_t const &);
	cdp_dummyvolumeimpl_t& operator=(cdp_dummyvolumeimpl_t const &);

	ACE_HANDLE m_invalidhandle;
};

#endif 
