//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpunix.cpp
//
// Description: 
// This file contains unix specific implementation for routines
// defined in cdpplatform.h
//

#include "cdpplatform.h"
#include "cdpglobals.h"
#include "error.h"
#include "portable.h"

#include <sstream>
#include <sys/statvfs.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
  

#include <stdarg.h>
#include "drvstatus.h"
#include "VsnapUser.h"
#include "VsnapShared.h"
#include "localconfigurator.h"
#include "inmsafecapis.h"
using namespace std;

#ifndef O_RDONLY
#define O_RDONLY 0
#endif


#if 0
//
// Test harness: Verify we get same results as Windows FileTimeToSystemTime()
//
#include <windows.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>

int main()
{
    srand( static_cast<unsigned>( time( NULL ) ) );
    LARGE_INTEGER liTime;

    for( int i = 0; i < 1000000; i++ ) {
        liTime.HighPart = (rand()<<16) | rand();
        liTime.LowPart  = (rand()<<16) | rand();

        SV_TIME svTime;
        bool rc1, rc2;
        rc1 = ToSvTime( liTime.QuadPart, svTime );

        FILETIME ft;
        ft.dwHighDateTime = liTime.HighPart;
        ft.dwLowDateTime  = liTime.LowPart;
        SYSTEMTIME st;
        rc2 = FileTimeToSystemTime( &ft, &st ) ? true : false; 

        if( rc1 != rc2 ) {
            printf( "return codes mismatched for %i64 (%d %d)\n", liTime.QuadPart, rc1, rc2 );
        }
        else {
            if( !(  svTime.wDay == st.wDay &&
                svTime.wDayOfWeek == st.wDayOfWeek &&
                svTime.wHour == st.wHour &&
                svTime.wMilliseconds == st.wMilliseconds &&
                svTime.wMinute == st.wMinute &&
                svTime.wMonth == st.wMonth &&
                svTime.wSecond == st.wSecond &&
                svTime.wYear == st.wYear ) ) 
            {
                printf( "conversion failed for %i64\n", liTime.QuadPart );
            }
        }

        unsigned long long convertedTime;
        if( !ToFileTime( svTime, convertedTime ) ) {
            printf( "Couldn't convert %i64 back to filetime!\n", liTime.QuadPart );
        }
        else {
            if( abs( static_cast<unsigned long>( liTime.QuadPart - convertedTime ) ) > 10000 ) {
                printf( "Conversion of %i64 came back as %i64\n", liTime.QuadPart, convertedTime );
            }
        }
    }

    printf( "All done.\n" );

    return 0;
}
#endif

bool CDPNewFormat()
{	
    return true;
}

bool GetDiskFreeSpaceP(const string & dir,SV_ULONGLONG *quota,SV_ULONGLONG *capacity,SV_ULONGLONG *ulTotalNumberOfFreeBytes)
{
    struct statvfs64 buf;
    if( statvfs64( dir.c_str(), &buf ) == -1 ) {
        return false;
    }
    // Total number of bytes  = Total number of blocks on file system in units of f_frsize 
    //							 * File system block size	
    if( NULL != capacity )
        *capacity =  buf.f_blocks * buf.f_bsize;		

    // Free space for non-privileged process = 
    //			Number of free blocks available to non-privileged process * File system block size
    if( NULL != quota )
        *quota = buf.f_bavail * buf.f_bsize;

    // Total free space = Total number of free blocks * File system block size
    if( NULL != ulTotalNumberOfFreeBytes )
    {
        // see PR #6869 for the change below
        //*ulTotalNumberOfFreeBytes = buf.f_bfree  * buf.f_bsize;  
        *ulTotalNumberOfFreeBytes = buf.f_bavail * buf.f_bsize;
    }

    return true;
}

bool GetDiskFreeCapacity(const string & dir, SV_ULONGLONG & freespace)
{
    // no ACE equivalent available
    /*struct statvfs buf;
    if( statvfs( dir.c_str(), &buf ) < 0 ) {
    return false;
    }
    freespace = buf.f_bavail * buf.f_bsize;
    return true;*/
    return  GetDiskFreeSpaceP(dir,NULL,NULL,&freespace);
}



bool OpenVsnapControlDevice(ACE_HANDLE &CtrlDevice)
{	
    char *CtrlDevName = "/dev/vsnapcontrol"; 
    CtrlDevice= open(CtrlDevName, O_RDONLY, 0);

    if(ACE_INVALID_HANDLE ==CtrlDevice )
        return false;


    return true;
}

long OpenInVirVolDriver(ACE_HANDLE &hDevice)
{
    if(!OpenVsnapControlDevice(hDevice))
        return 1;

    return 0;
}

void CloseVVControlDevice(ACE_HANDLE& CtrlDevice)
{	
    ACE_OS::close(CtrlDevice);
    CtrlDevice=ACE_INVALID_HANDLE;
}

int IssueMapUpdateIoctlToVVDriver(ACE_HANDLE& CtrlDevice, 
								  const char *VolumeName, 
								  const char *DataFileName,
								  const SV_UINT & length,
								  const SV_OFFSET_TYPE & volume_offset,
								  const SV_OFFSET_TYPE & file_offset,
								  bool Cow)
{
    int bResult;

    UPDATE_VSNAP_VOLUME_INPUT LockInput;
    
    memset(&LockInput, 0, sizeof LockInput);

    inm_strcpy_s(LockInput.ParentVolumeName, ARRAYSIZE(LockInput.ParentVolumeName), VolumeName);
    inm_strcpy_s(LockInput.DataFileName, ARRAYSIZE(LockInput.DataFileName), DataFileName);
    LockInput.VolOffset = volume_offset;
    LockInput.Length = length;
    LockInput.FileOffset = file_offset;
    LockInput.Cow = Cow;

    bResult = ioctl(CtrlDevice, IOCTL_INMAGE_UPDATE_VSNAP_VOLUME, &LockInput);
    if(bResult < 0)	
    {
        DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s IOCTL_INMAGE_UPDATE_VSNAP_VOLUME failed with errno = %d for volume %s for data file %s\n", 
                    LINE_NO, FILE_NAME, errno, LockInput.ParentVolumeName, LockInput.DataFileName);
    }

    return bResult;
}


void TerminateProcess(ACE_Process_Manager* pm, pid_t pid)
{
    pm->terminate(pid, SIGINT);
}
