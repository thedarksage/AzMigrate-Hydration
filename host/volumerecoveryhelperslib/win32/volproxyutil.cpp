#include "volproxyutil.h"
#include <windows.h>
#include <Winnetwk.h>
#include <winbase.h>
#include <winsvc.h>
#include <tchar.h>
#include "service.h"
#include "registry.h"
#include "appcommand.h"
#include "diskpartwrapper.h"

bool mountRepository(const std::string& sharePath, const std::string& localPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true ;
}

bool unMountRepository(const std::string& repositoryPath)
{
	return true;
}

std::string GetSystemDir()
{
	TCHAR szSysDrive[MAX_PATH];
	std::string SystemDrive ;
	DWORD dw = GetSystemDirectory(szSysDrive, MAX_PATH);
     if(dw == 0)
	 {
		DebugPrintf( SV_LOG_ERROR , "GetSystemDirectory failed with error: %d\n", GetLastError() ) ;
	 }
	 else
	 {
		 SystemDrive = std::string(szSysDrive);
		 size_t pos = SystemDrive.find_first_of(":");
		 if( pos  != std::string::npos )
		 SystemDrive.assign(SystemDrive,0,pos);
	 }
	 return SystemDrive;
}

void GetMountNameFromPath(const std::string& Actualsource, std::string& mountPoint)
{
        mountPoint = Actualsource;
 		replace(mountPoint.begin(), mountPoint.end(), '\\', '_');
		replace(mountPoint.begin(), mountPoint.end(), ':', '_');
        mountPoint = GetSystemDir()+ ":\\" + mountPoint ;
}

void getSharedPathAndLocalPath( const std::string& targetIP, const std::string& shareName, std::string& sharePath, std::string& localPath )
{
    sharePath = "\\\\" + targetIP + "\\" + shareName ;
    localPath = sharePath ;

    return ;
}

bool StopShellHWDetectionService()
 {
    bool bReturn = true ;
    InmServiceStatus   serviceStatus ;
    isServiceInstalled( "ShellHWDetection", serviceStatus ) ;
    if( serviceStatus == INM_SVCSTAT_INSTALLED )
    {
        if( StopSvc(  "ShellHWDetection" ) != SVS_OK )
            bReturn = false ;
    }
    return bReturn ;
}

bool StartShellHWDetactionService()
{
     bool bReturn = true ;
    InmServiceStatus   serviceStatus ;
    isServiceInstalled( "ShellHWDetection", serviceStatus ) ;
    if( serviceStatus == INM_SVCSTAT_INSTALLED )
    {
        if( StartSvc(  "ShellHWDetection" ) != SVS_OK )
            bReturn = false ;
    }
    return bReturn ;
}

bool DisableAutoMount()
{
    return ( setDwordValue( "SYSTEM\\CurrentControlSet\\Services\\mountmgr", "NoAutoMount", 1 ) == SVS_OK ) ;
}

bool EnableAutoMount()
{
    return ( setDwordValue( "SYSTEM\\CurrentControlSet\\Services\\mountmgr", "NoAutoMount", 0 ) == SVS_OK ) ;
}

SV_ULONG EnableAutoMountUsingDiskPart( std::string& strInstallPath, SV_ULONG doNotRunDiskPart  )
{
    SV_ULONG dwExitCode = 0 ;
    if( 0 == doNotRunDiskPart )
    {
        DISKPART::DiskPartWrapper diskPartWrapper;
        if( !diskPartWrapper.automount(true) )
        {
            DebugPrintf(SV_LOG_ERROR, "Error Failed to enable automount: %s", diskPartWrapper.getError().c_str());
            // FIXME: for now ignore DiskPartWrapper errors
            // dwExitCode = ERROR_DISK_OPERATION_FAILED;
        }
    }
    return dwExitCode ;
}

SV_ULONG DisableAutoMountUsingDiskPart( std::string &strInstallPath, SV_ULONG doNotRunDiskPart )
{

    SV_ULONG dwExitCode = 0 ;
    if( 0 == doNotRunDiskPart )
    {
        DISKPART::DiskPartWrapper diskPartWrapper;
        if( !diskPartWrapper.automount(false) )
        {
            DebugPrintf(SV_LOG_ERROR, "Error Failed to disable automount: %s", diskPartWrapper.getError().c_str());
            // FIXME: for now ignore DiskPartWrapper errors
            // dwExitCode = ERROR_DISK_OPERATION_FAILED;
        }
    }
    return dwExitCode ;
}

SV_ULONG BringVolumesOnlineUsingDiskPart( std::string &strInstallPath )
{
    SV_ULONG dwExitCode = 0 ;
    DISKPART::DiskPartWrapper diskPartWrapper;
    if( !diskPartWrapper.onlineVolumes() )
    {
        DebugPrintf(SV_LOG_ERROR, "Error Failed to bring volumes online: %s", diskPartWrapper.getError().c_str());
        dwExitCode = ERROR_DISK_OPERATION_FAILED;
    } else {
        // TODO: not sure we need to do this any more since we can now use guids 
        // for now try it but ignore errors
       diskPartWrapper.removeCdDvdDriveLetterMountPoint();
    }
    return dwExitCode ;
}

