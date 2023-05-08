/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : cdpvolume.cpp
*
* Description: 
*/

#include <string>

#include "portablehelpers.h"
#include "cdpvolume.h"
#include "cdprealvolumeimpl.h"
#include "cdpvolumeimpl.h"
#include "volume.h"
#include "cdpemulatedvolumeimpl.h"
#include "cdpdummyvolumeimpl.h"
#include "cdpdiskimpl.h"
#include "inmageex.h"
#include "boost/shared_array.hpp"

using namespace std;

/*
* FUNCTION NAME : cdp_volume_t
*
* DESCRIPTION : constructor
*
* INPUT PARAMETERS : Volume name
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
cdp_volume_t::cdp_volume_t(const std::string& sVolumeName, bool multisparse, const CDP_VOLUME_TYPE &type, const VolumeSummaries_t *pVolumeSummariesCache)
               :m_devicename(sVolumeName),m_newvirtualvolume(multisparse)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, is virtual volume = %s, type = %s, pVolumeSummaries = %p\n", FUNCTION_NAME, multisparse ? "true" : "false", GetStrVolumeType(type).c_str(), pVolumeSummariesCache);

	CDP_VOLUME_TYPE effectivetype = (CDP_DISK_TYPE == type) ? GetCdpVolumeTypeForDisk() : type;
	DebugPrintf(SV_LOG_DEBUG, "Effective type = %s\n", GetStrVolumeType(effectivetype).c_str());

	if (multisparse){
		m_imp.reset(new cdp_emulatedVolumeimpl_t(sVolumeName));
	}
	else if (CDP_REAL_VOLUME_TYPE == effectivetype){
		m_imp.reset(new cdp_realVolumeimpl_t(sVolumeName));
	}
	else if (CDP_DISK_TYPE == effectivetype){
		m_imp.reset(new cdp_diskImpl_t(sVolumeName, pVolumeSummariesCache));
	}
	else if (CDP_DUMMY_VOLUME_TYPE == effectivetype){
		m_imp.reset(new cdp_dummyvolumeimpl_t());
	}
	else{
		throw INMAGE_EX("cdp_volume_t instantiation failed because unknown volume type supplied")(effectivetype)(GetStrVolumeType(effectivetype));
	}

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : ~cdp_volume_t
*
* DESCRIPTION : destructor
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
cdp_volume_t::~cdp_volume_t()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_imp.reset();		
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool cdp_volume_t::Hide()
{
	return m_imp->Hide();	
}

VOLUME_STATE cdp_volume_t::GetDeviceState()
{
	return m_imp->GetDeviceState();
}

bool cdp_volume_t::IsVolumeLocked()
{
	return (VOLUME_HIDDEN == GetDeviceState());
}

SV_ULONG cdp_volume_t::LastError() const
{
	return m_imp->LastError();	
}

bool cdp_volume_t::OpenExclusive()
{
	return m_imp->OpenExclusive();
}

SV_INT cdp_volume_t::Open(BasicIo::BioOpenMode mode)
{
	return m_imp->Open(mode);
}

BasicIo::BioOpenMode cdp_volume_t::OpenMode(void)
{
    return m_imp->OpenMode();
}

SV_UINT cdp_volume_t::Read(char* buffer, SV_UINT length)
{
	return m_imp->Read(buffer, length);		
}

SV_UINT cdp_volume_t::FullRead(char* buffer, SV_UINT length)
{
	return m_imp->FullRead(buffer, length);		
}

SV_UINT cdp_volume_t::Write(char const * buffer, SV_UINT length)
{
		return m_imp->Write(buffer, length);		
}

SV_LONGLONG cdp_volume_t::Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from)
{
	return m_imp->Seek(offset, from);		
}

SV_LONGLONG cdp_volume_t::Tell() const
{
		return m_imp->Tell();		
}

SV_INT cdp_volume_t::Close()
{
	return m_imp->Close();		
}


bool cdp_volume_t::Good() const
{
	return m_imp->Good();		
}

bool cdp_volume_t::Eof() const
{
		return m_imp->Eof();		
}

void cdp_volume_t::ClearError()
{
		m_imp->ClearError();
}

bool cdp_volume_t::isOpen()
{
		return m_imp->isOpen();
}

void cdp_volume_t::SetLastError(SV_ULONG liError)
{
	m_imp->SetLastError(liError);
}

ACE_HANDLE& cdp_volume_t::GetHandle()
{
    return m_imp->GetHandle();
}

void cdp_volume_t::EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay)
{
	m_imp->EnableRetry(fWait, iRetries, iRetryDelay);
}

void cdp_volume_t::DisableRetry()
{
		m_imp->DisableRetry();
}

FeatureSupportState_t cdp_volume_t::SetFileSystemCapabilities(const std::string &fs)
{
		return m_imp->SetFileSystemCapabilities(fs);
}

bool cdp_volume_t::SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset)
{
		return m_imp->SetNoFileSystemCapabilities(size, startoffset);
}

SV_ULONGLONG cdp_volume_t::GetNumberOfClusters()
{
		return m_imp->GetNumberOfClusters();
}

bool cdp_volume_t::GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci)
{
		return m_imp->GetVolumeClusterInfo(vci);
}

void cdp_volume_t::PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci)
{
		m_imp->PrintVolumeClusterInfo(vci);
}

SV_UINT cdp_volume_t::GetPhysicalSectorSize(void)
{
		return m_imp->GetPhysicalSectorSize();
}

SV_ULONGLONG cdp_volume_t::GetSize(void)
{
	return m_imp->GetSize();
}

SV_LONGLONG cdp_volume_t::GetFormattedVolumeSize(const std::string& volumename,bool multisparse)
{
    if(multisparse)
        return cdp_emulatedVolumeimpl_t::GetFormattedVolumeSize(volumename);
    else
        return Volume::GetFormattedVolumeSize(volumename);
}

SV_ULONGLONG cdp_volume_t::GetRawVolumeSize(const std::string& volumename,bool multisparse)
{
    if(multisparse)
        return cdp_emulatedVolumeimpl_t::GetRawVolumeSize(volumename);
    else
        return Volume::GetRawVolumeSize(volumename);
}

std::string cdp_volume_t::GetStrVolumeType(const CDP_VOLUME_TYPE &type)
{
	static const char * const StrVolumeTypes[] = { "Real Volume", "Dummy Volume", "Disk" };
	return (type < CDP_VOLUME_UNKNOWN_TYPE)&&(type >= CDP_REAL_VOLUME_TYPE) ? StrVolumeTypes[type] : "unknown";
}

BasicIo::BioOpenMode cdp_volume_t::GetSourceVolumeOpenMode(const int &sourcedevicetype)
{
	BasicIo::BioOpenMode mode = BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary;
	mode |= PlatformSourceVolumeOpenModeFlags(sourcedevicetype);
	return mode;
}

bool cdp_volume_t::IsInitialized(void)
{
	return m_imp->IsInitialized();
}

bool cdp_volume_t::FlushFileBuffers()
{
    return m_imp ->FlushFileBuffers();
}

cdp_volume_t::CDP_VOLUME_TYPE cdp_volume_t::GetCdpVolumeType(const VolumeSummary::Devicetype & devType)
{
	CDP_VOLUME_TYPE volType = cdp_volume_t::CDP_VOLUME_UNKNOWN_TYPE;

	switch (devType)
	{
	case VolumeSummary::DISK:
		volType = GetCdpVolumeTypeForDisk();
		break;
	case VolumeSummary::VOLPACK:
		volType = CDP_EMULATED_VOLUME_TYPE;
		break;
	default:
		volType = CDP_REAL_VOLUME_TYPE;
	}

	return volType;
}