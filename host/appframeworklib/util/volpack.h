#ifndef VOLUME_PROVISION_H
#define VOLUME_PROVISION_H
#include "appglobals.h"


class VolPack
{
public:
		SV_UINT m_uTimeOut;
		std::string m_MountPointName;
		SV_ULONGLONG m_volPackSize;
		std::string m_stdOut;
		std::string getCdpcliPath();
		std::string m_strStdOut;
	std::string m_finalMountPoint;
	VolPack(const std::string& strCommand, const SV_ULONGLONG& volPackSize, const SV_UINT& timeout)
	{
		m_uTimeOut = timeout ;
		m_MountPointName = strCommand;
		m_volPackSize = volPackSize;
		m_finalMountPoint = "";
	}
    bool volpackProvision(std::string& ouputFileName,SV_ULONG &exitCode ) ;	
    bool volpackDeletion(std::string& ouputFileName,SV_ULONG &exitCode);
	bool volpackResize(std::string& outputFileName, SV_ULONG& exitCode);
	std::string getMountPointName()
	{
		return m_MountPointName;
	}
	std::string getFinalMountPoint()
	{
		return m_finalMountPoint;
	}
	std::string stdOut() { return m_stdOut;}
} ;

#endif
