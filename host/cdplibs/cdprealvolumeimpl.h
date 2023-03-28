/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : cdprealvolumeimpl.h
 *
 * Description: 
 */
#ifndef CDPREALVOLUME__H
#define CDPREALVOLUME__H 

#include <string>
#include <boost/shared_ptr.hpp>
#include "svtypes.h"
#include "cdpvolumeimpl.h"
#include "basicio.h"
#include "volume.h"

class cdp_realVolumeimpl_t: public cdp_volumeimpl_t
{
public:
    
    cdp_realVolumeimpl_t(const std::string&);
    virtual ~cdp_realVolumeimpl_t();
	virtual VOLUME_STATE GetDeviceState()                                           { return ::GetVolumeState(m_volume->GetName().c_str()); }
    virtual bool Hide()                                                             { return m_volume->Hide(); }
    virtual SV_ULONG LastError() const                                              { return m_volume->LastError(); }
    virtual bool OpenExclusive()                                                    { return m_volume->OpenExclusive(); }
    virtual SV_INT Open(BasicIo::BioOpenMode mode)                                { return m_volume->Open(mode); }
    virtual BasicIo::BioOpenMode OpenMode(void)                                     { return m_volume->OpenMode(); }
    virtual SV_UINT Read(char* buffer, SV_UINT length)                              { return m_volume->Read(buffer, length); }
    virtual SV_UINT FullRead(char* buffer, SV_UINT length)                          { return m_volume->FullRead(buffer, length); }
    virtual SV_UINT Write(char const * buffer, SV_UINT length)                      { return m_volume->Write(buffer, length); }
    virtual SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from)         { return m_volume->Seek(offset, from); }
    virtual SV_LONGLONG Tell() const                                                { return m_volume->Tell(); }
    virtual SV_INT Close()                                                          { return m_volume->Close(); }    
    virtual bool Good() const                                                       { return m_volume->Good(); }
    virtual bool Eof() const                                                        { return m_volume->Eof(); }
    virtual void ClearError()                                                       { return m_volume->ClearError(); }
    virtual bool isOpen()                                                           { return m_volume->isOpen(); }
    virtual void SetLastError(SV_ULONG liError)                                     { m_volume->SetLastError(liError); }
    virtual ACE_HANDLE& GetHandle()                                                 { return m_volume->GetHandle(); }
    virtual void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay)     { return m_volume->EnableRetry(fWait, iRetries, iRetryDelay); }
    virtual void DisableRetry()                                                     { return m_volume->DisableRetry(); }
    virtual bool IsInitialized(void)                                                { return m_volume->IsInitialized();}
    virtual bool FlushFileBuffers()                                                 { return m_volume->FlushFileBuffers();}
    virtual FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs)  { return m_volume->SetFileSystemCapabilities(fs);}
    virtual bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset) 
    { 
        return m_volume->SetNoFileSystemCapabilities(size, startoffset);
    }
    virtual SV_ULONGLONG GetNumberOfClusters(void)                                  { return m_volume->GetNumberOfClusters();}
    virtual bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci){ return m_volume->GetVolumeClusterInfo(vci);}
    virtual void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci){ m_volume->PrintVolumeClusterInfo(vci);}
    virtual SV_UINT GetPhysicalSectorSize(void) { return m_volume->GetPhysicalSectorSize();}

	/// \brief function to return size
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	virtual SV_ULONGLONG GetSize(void) { return m_volume->GetSize(); }

private:
    cdp_realVolumeimpl_t& operator=(const cdp_realVolumeimpl_t&);
    cdp_realVolumeimpl_t(const cdp_realVolumeimpl_t&);
protected:
    Volume::Ptr m_volume;
};

#endif 

