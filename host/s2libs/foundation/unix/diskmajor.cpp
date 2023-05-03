///
///  \file diskmajor.cpp
///
///  \brief unix implementation
///

#include <stdexcept>
#include "disk.h"

SV_INT Disk::Open(BasicIo::BioOpenMode mode)
{
	throw std::runtime_error("not implemented");
}

bool Disk::OpenExclusive()
{
	throw std::runtime_error("not implemented");
}


SV_INT Disk::VerifyAndOpen(const std::string &diskDeviceName, BasicIo::BioOpenMode &mode,
           const VolumeSummary::NameBasedOn & nameBasedOn)
{
	throw std::runtime_error("not implemented");
}


SV_ULONGLONG Disk::GetSize(void)
{
	throw std::runtime_error("not implemented");
}

bool Disk::OfflineRW(void)
{
	throw std::runtime_error("not implemented");
}

bool Disk::OnlineRW(void)
{
	throw std::runtime_error("not implemented");
}

bool Disk::OnlineOffline(bool online, bool readOnly, bool persist)
{
        throw std::runtime_error("not implemented");
}

VOLUME_STATE  Disk::GetDeviceState(void)
{
	throw std::runtime_error("not implemented");
}


bool Disk::InitializeAsRawDisk(void)
{
	throw std::runtime_error("not implemented");
}
