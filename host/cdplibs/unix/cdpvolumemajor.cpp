///
///  \file  cdpvolumemajor.cpp
///
///  \brief unix specific code
///

#include "cdpvolume.h"

cdp_volume_t::CDP_VOLUME_TYPE cdp_volume_t::GetCdpVolumeTypeForDisk(void)
{
	//Unix: Do not use disk class
	return CDP_REAL_VOLUME_TYPE;
}


BasicIo::BioOpenMode cdp_volume_t::PlatformSourceVolumeOpenModeFlags(const int &sourcedevicetype)
{
	//Unix: do direct read
	return BasicIo::BioDirect;
}
