#ifndef __COMMON_H
#define __COMMON_H

#include <string>
#include <windows.h>

namespace DrDrillNS
{
  //#ifdef _UNICODE
	typedef std::string _tstring;
    typedef char _tchar;
  //#endif

  enum SERVICESTATUS
  {
    stopped,
    started,
    paused
  };

  struct VolumeConfiguration
  {
    _tstring strVolume;
    ULONG ulVolSize;//in MB
    _tstring strVolumeGuid;
    bool bIsVolResized;
  };
}
#endif