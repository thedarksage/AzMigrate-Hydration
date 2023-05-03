///
///  \file  cdpvolumemajor.cpp
///
///  \brief windows specific code
///

#include "cdpvolume.h"

cdp_volume_t::CDP_VOLUME_TYPE cdp_volume_t::GetCdpVolumeTypeForDisk(void)
{
	return CDP_DISK_TYPE;
}


BasicIo::BioOpenMode cdp_volume_t::PlatformSourceVolumeOpenModeFlags(const int &sourcedevicetype)
{
	BasicIo::BioOpenMode mode = 0;
	//For windows disk, use direct read
	if (VolumeSummary::DISK == sourcedevicetype)
		mode |= BasicIo::BioNoBuffer;

	return mode;
}