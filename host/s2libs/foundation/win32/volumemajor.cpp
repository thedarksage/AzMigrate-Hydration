/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volume_port.cpp
 *
 * Description: 
 */
#include <windows.h>
#include <winioctl.h>

#include "hostagenthelpers.h"
#include "autoFS.h"
#include "portable.h"
#include "entity.h"

#include "error.h"
#include "synchronize.h"

#include "genericfile.h"
#include "volume.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"

/** **/
/*
 * FUNCTION NAME : Init
 *
 * DESCRIPTION : 
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
int Volume::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int rc = SV_SUCCESS;
    memset(m_sVolumeGUID, 0, sizeof(m_sVolumeGUID));
    std::string sVolName = GetName();
	if(IsDrive(sVolName))
	{
    	UnformatVolumeNameToDriveLetter(sVolName);
		if (!DriveLetterToGUID(sVolName[0], m_sVolumeGUID, sizeof(m_sVolumeGUID))) 
		{
			m_bInitialized = false;
			return SV_FAILURE;
		}
	}
	else if ( IsMountPoint(sVolName) ) {
		std::string mountPtGuid = sVolName;
        if(!FormatVolumeNameToGuid(mountPtGuid))
		{
			rc = SV_FAILURE;
			m_bInitialized = false;
			return rc;
		}
		ExtractGuidFromVolumeName(mountPtGuid);
		USES_CONVERSION;		
		inm_memcpy_s(m_sVolumeGUID, sizeof(m_sVolumeGUID), A2W(mountPtGuid.c_str()), (GUID_SIZE_IN_CHARS * sizeof(wchar_t)));
	}
	/* Set the m_bIsFilteredVolume to true by default for windows volumes.*/
	m_bIsFilteredVolume = true;
	m_bInitialized = true;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rc;
}

/*
 * FUNCTION NAME : GetBytesPerSector
 *
 * DESCRIPTION : 
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
const unsigned long int Volume::GetBytesPerSector()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    unsigned long int uliBytesPerSector         =0;

    unsigned long int uliSectorsPerCluster      =0;
    unsigned long int uliNumberOfFreeClusters   =0;
    unsigned long int uliTotalNumberOfClusters  =0;

	// PR#10815: Long Path support
	if ( !SVGetDiskFreeSpace( GetPath().c_str(),
							&uliSectorsPerCluster,
							&uliBytesPerSector,
							&uliNumberOfFreeClusters,
							&uliTotalNumberOfClusters)
						  )
    {
        uliBytesPerSector = 0;
        DebugPrintf( SV_LOG_ERROR,
					 "FAILED: Unable to detect Bytes per Sector on volume"
					 " %s. %s. @LINE %d in FILE %s\n",
					 GetPath().c_str(),
					 Error::Msg().c_str(),
					 LINE_NO,
					 FILE_NAME);
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return uliBytesPerSector;
}

/*
 * FUNCTION NAME : GetFreeBytes
 *
 * DESCRIPTION : 
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
SV_ULONGLONG Volume::GetFreeBytes(const std::string& sVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AutoLock FreeByteGuard(m_FreeByteLock);

	ULARGE_INTEGER ulTotalNumberOfFreeBytes;
	if ( IsExisting(sVolume ) )
	{
		// PR#10815: Long Path support
		if ( !SVGetDiskFreeSpaceEx(sVolume.c_str(),NULL,NULL,&ulTotalNumberOfFreeBytes) )
		{
			DebugPrintf( SV_LOG_ERROR,
						 "FAILED: Unable to detect free space on volume"
						 " %s. %s. @LINE %d in FILE %s\n",
						 sVolume.c_str(),
						 Error::Msg().c_str(),
						 LINE_NO,
						 FILE_NAME);
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return ulTotalNumberOfFreeBytes.QuadPart;
}

/*
 * FUNCTION NAME : GetRawVolumeSize
 *
 * DESCRIPTION : 
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
SV_ULONGLONG Volume::GetRawVolumeSize(const std::string& sVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string formattedVolume = sVolume;
    AutoLock TotalByteGuard(m_TotalByteLock);

	ULARGE_INTEGER ulTotalNumberOfFreeBytes;
	GET_LENGTH_INFORMATION Length_Info;
	Length_Info.Length.HighPart	= 0;
	Length_Info.Length.LowPart	= 0;
	Length_Info.Length.QuadPart	= 0;

	ulTotalNumberOfFreeBytes.HighPart	= 0;
	ulTotalNumberOfFreeBytes.LowPart	= 0;
	ulTotalNumberOfFreeBytes.QuadPart	= 0;

    FormatVolumeNameToGuid(formattedVolume);
	unsigned long int uliBytesReturned = 0;
	// PR#10815: Long Path support
	HANDLE hHandle = SVCreateFile( formattedVolume.c_str(),
								 GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,
								 NULL,
								 OPEN_EXISTING,
								 FILE_ATTRIBUTE_NORMAL,
								 NULL );
	if ( INVALID_HANDLE_VALUE == hHandle )
	{
		DebugPrintf( SV_LOG_ERROR,
					 "FAILED: An error occurred while opening volume"
					 " %s. %s. @LINE %d in FILE %s\n",
					 sVolume.c_str(),
					 Error::Msg().c_str(),
					 LINE_NO,
					 FILE_NAME);
		return 0;
	}
	/*
	 * IMPORTANT: It turns out from user mode, a file system will slightly reduce
	 * the size of a volume by hiding some blocks. You need to call the ioctl
	 * DeviceIoControl(volumeHandle, FSCTL_ALLOW_EXTENDED_DASD_IO, 
	 *                 NULL, 0, NULL, 0, &status, NULL);
	 * on any volume that uses this function, or expect to read or write ALL of a
	 * volume. This tells the filesystem to let the underlying volume do the bounds
	 * checking, not the filesystem This ioctl will probably fail on raw volumes,
	 * just ignore the error and call it on every volume. You may 
	 * need #define _WIN32_WINNT 0x0510 before your headers to get this ioctl
	 * However, we will continue if the ioctl fails.
	 */

	unsigned long unused = 0;
	DeviceIoControl(hHandle, FSCTL_ALLOW_EXTENDED_DASD_IO,
		NULL, 0, NULL, 0, &unused, NULL);

	if ( !DeviceIoControl( hHandle,
								IOCTL_DISK_GET_LENGTH_INFO,
								NULL,
								0,
								&Length_Info, 
								sizeof(Length_Info),
								&uliBytesReturned,
								NULL) )
	{
		DebugPrintf( SV_LOG_WARNING,
		             "Warning: An IOCTL did not complete sucessfully while"
					 "trying to determine the total bytes in volume %s. %s."
					 "@LINE %d in FILE %s\n",
					 sVolume.c_str(),
					 Error::Msg().c_str(),
					 LINE_NO,
					 FILE_NAME);

	}

	CloseHandle(hHandle);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Length_Info.Length.QuadPart;
}

/*
 * FUNCTION NAME : GetVolumeGUID
 *
 * DESCRIPTION : 
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
const wchar_t* Volume::GetVolumeGUID() const
{
    return m_sVolumeGUID;
}
