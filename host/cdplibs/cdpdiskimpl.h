///
///  \file  cdpdiskimpl.h
///
///  \brief wrapper over disk class
///

#ifndef CDPDISKIMPL__H
#define CDPDISKIMPL__H 

#include <string>
#include <boost/shared_ptr.hpp>
#include "svtypes.h"
#include "cdpvolumeimpl.h"
#include "basicio.h"
#include "disk.h"
#include "volumegroupsettings.h"

/// \brief disk implementation
class cdp_diskImpl_t : public cdp_volumeimpl_t
{
public:
	cdp_diskImpl_t(const std::string &name,                       ///< name of disk
                   const VolumeSummaries_t *pVolumeSummariesCache ///< volumes cache to pass to Disk class
                   );
	virtual ~cdp_diskImpl_t();
	virtual VOLUME_STATE GetDeviceState()                                           { return m_disk->GetDeviceState(); }
	virtual bool Hide()                                                             { return m_disk ->OfflineRW(); }
	virtual SV_ULONG LastError() const                                              { return m_disk->LastError(); }
	virtual bool OpenExclusive()                                                    { return m_disk->OpenExclusive(); }
	virtual SV_INT Open(BasicIo::BioOpenMode mode)                                { return m_disk->Open(mode); }
	virtual BasicIo::BioOpenMode OpenMode(void)                                     { return m_disk->OpenMode(); }
	virtual SV_UINT Read(char* buffer, SV_UINT length)                              { return m_disk->Read(buffer, length); }
	virtual SV_UINT FullRead(char* buffer, SV_UINT length)                          { return m_disk->FullRead(buffer, length); }
	virtual SV_UINT Write(char const * buffer, SV_UINT length)                      { return m_disk->Write(buffer, length); }
	virtual SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from)         { return m_disk->Seek(offset, from); }
	virtual SV_LONGLONG Tell() const                                                { return m_disk->Tell(); }
	virtual SV_INT Close()                                                          { return m_disk->Close(); }
	virtual bool Good() const                                                       { return m_disk->Good(); }
	virtual bool Eof() const                                                        { return m_disk->Eof(); }
	virtual void ClearError()                                                       { return m_disk->ClearError(); }
	virtual bool isOpen()                                                           { return m_disk->isOpen(); }
	virtual void SetLastError(SV_ULONG liError)                                     { m_disk->SetLastError(liError); }
	virtual ACE_HANDLE& GetHandle()                                                 { return m_disk->GetHandle(); }
	virtual void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay)     { return m_disk->EnableRetry(fWait, iRetries, iRetryDelay); }
	virtual void DisableRetry()                                                     { return m_disk->DisableRetry(); }
	virtual bool IsInitialized(void)                                                { return m_disk->IsInitialized(); }
	virtual bool FlushFileBuffers()                                                 { return m_disk->FlushFileBuffers(); }
	virtual FeatureSupportState_t SetFileSystemCapabilities(const std::string &fs)  { return m_disk->SetFileSystemCapabilities(fs); }
	virtual bool SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset)
	{
		return m_disk->SetNoFileSystemCapabilities(size, startoffset);
	}
	virtual SV_ULONGLONG GetNumberOfClusters(void)                                  { return m_disk->GetNumberOfClusters(); }
	virtual bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci){ return m_disk->GetVolumeClusterInfo(vci); }
	virtual void PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci){ m_disk->PrintVolumeClusterInfo(vci); }
	virtual SV_UINT GetPhysicalSectorSize(void) { return m_disk->GetPhysicalSectorSize(); }

	/// \brief function to return size
	///
	/// \return
	/// \li \c non zero size: success
	/// \li \c 0            : failure
	virtual SV_ULONGLONG GetSize(void) { return m_disk->GetSize(); }

private:
	cdp_diskImpl_t& operator=(const cdp_diskImpl_t&);
	cdp_diskImpl_t(const cdp_diskImpl_t&);
protected:
	Disk::Ptr m_disk;
};

#endif /* CDPDISKIMPL__H */