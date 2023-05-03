///
///  \file  CVirtualDisk.h
///
///  \brief contains declarations of CVirtualDisk class
///

#ifndef CVIRTUALDISK_H
#define CVIRTUALDISK_H

#include <Windows.h>
#include "CDiskDevice.h"
#include "PlatformAPIs.h"
#include <initguid.h>
#include <windows.h>
#include <winioctl.h>
#include <VirtDisk.h>
#include <cguid.h>


class CVirtualDisk
{
private:
	ULONGLONG		m_ullMaxVhdSize;
	CDiskDevice*	m_mountedVDisk;
	
	WCHAR			m_vDiskPath[MAX_PATH];
	WCHAR			m_diskPath[MAX_PATH];
	IPlatformAPIs*	m_platformAPIs;
	void*			m_hVDisk;

public:
	CVirtualDisk(const WCHAR* wszVhdPath, ULONGLONG ullMaxSize, IPlatformAPIs* platformAPIs);
	~CVirtualDisk();
	CDiskDevice*	MountVDisk();
	void			UnMountVDisk();
	void			CreateVDisk();
	void			Open();
};
#endif /* CVIRTUALDISK_H */
