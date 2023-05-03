/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : cdpemulatedvolumeimpl.h
*
* Description: 
*/
#ifndef CDPEMULATEDVOLUMEIMPL__H
#define CDPEMULATEDVOLUMEIMPL__H 

#include "basicio.h"
#include "cdpvolumeimpl.h"
#include "svtypes.h"
#include "cdpdrtd.h"

#include <boost/shared_ptr.hpp>
#include <string>
#include <map>

typedef std::map<std::string, ACE_HANDLE> cdp_synchandles_t;

class cdp_emulatedVolumeimpl_t:public cdp_volumeimpl_t
{
public:
    cdp_emulatedVolumeimpl_t(const std::string & device_name);
    ~cdp_emulatedVolumeimpl_t();

    static SV_LONGLONG GetFormattedVolumeSize(const std::string&);
    static SV_ULONGLONG GetRawVolumeSize(const std::string&);
	virtual VOLUME_STATE GetDeviceState()    { return VOLUME_HIDDEN; }
    virtual bool Hide()         {return true;}
    virtual bool Eof() const    {return (m_currentoffset>=m_volumesize);}
    virtual void ClearError()   {m_error = SV_SUCCESS; m_good=true;}
    virtual bool isOpen()       {return is_open;}
    virtual bool IsInitialized(void)  { return true;}
    virtual SV_LONGLONG Tell() const  {return m_currentoffset; }
    virtual bool Good() const         {return m_good;}
    virtual SV_ULONG LastError() const          {return m_error;}
    virtual ACE_HANDLE& GetHandle() {  return m_invalidhandle;}
    virtual void SetLastError(SV_ULONG liError) {m_error = liError; }
    virtual void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay) {}
    virtual void DisableRetry()                                                 {}

    virtual bool OpenExclusive();    
    virtual SV_INT Open(BasicIo::BioOpenMode mode);
    virtual BasicIo::BioOpenMode OpenMode(void);

    // returns zero on success else SV_FAILURE
    virtual SV_INT Close();

    virtual SV_UINT Read(char* buffer, SV_UINT length);
    virtual SV_UINT FullRead(char* buffer, SV_UINT length);
    virtual SV_UINT Write(char const * buffer, SV_UINT length);
    virtual SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from);    
    
    virtual bool FlushFileBuffers(); 

    virtual FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs)  { return E_FEATURECANTSAY;}
    virtual bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset){ return false;}
    virtual SV_ULONGLONG GetNumberOfClusters(void){ return 0;}
    virtual bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci){ return false;}
    virtual void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci){}
    virtual SV_UINT GetPhysicalSectorSize(void){ return 0; }

	/// \brief function to return size
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	virtual SV_ULONGLONG GetSize(void){ return 0; }

protected:    


private:

    cdp_emulatedVolumeimpl_t(cdp_emulatedVolumeimpl_t const & );
    cdp_emulatedVolumeimpl_t& operator=(cdp_emulatedVolumeimpl_t const & );

    bool all_zeroes(const char * const buffer, const SV_UINT & length);                         

    bool punch_hole_sync_sparsefile(const std::string & filename,
        ACE_HANDLE file_handle,
        const SV_OFFSET_TYPE & offset,
        const SV_UINT & length);

    bool split_drtd_by_sparsefilename(const SV_OFFSET_TYPE & offset,
        const SV_UINT & length,
        cdp_drtds_sparsefile_t & splitdrtds);

    bool get_sync_handle(const std::string & filename, 
        ACE_HANDLE & handle);

    bool close_handles();

    bool set_open_parameters(BasicIo::BioOpenMode mode);

    bool ModeOn(BasicIo::BioOpenMode chkMode, BasicIo::BioOpenMode mode)
   	{
        return (chkMode == (chkMode & mode));
    }

    std::string m_volumename;
    bool m_good;
    bool is_open; 
    SV_INT m_error;

    BasicIo::BioOpenMode m_mode;
    SV_ULONG m_access;
    SV_ULONG m_share;
    SV_OFFSET_TYPE m_currentoffset;
    SV_OFFSET_TYPE m_volumesize;
    SV_OFFSET_TYPE m_maxfilesize;

    cdp_synchandles_t m_handles;
    ACE_HANDLE m_invalidhandle;    
	SV_UINT m_max_rw_size;
	bool m_sparse_enabled;
	bool m_punch_hole_supported;
};

#endif 
