#include "util/exportdevice.h"
#include "appcommand.h"
#include <sstream>
#include <string>
#include <windows.h>
#include <WinIoCtl.h>
#include "controller.h"
#include "ClusterOperation.h"
#include "system.h"
#include "service.h"
#define WIN_DISKNAME_PREFIX     "\\\\.\\PHYSICALDRIVE"

std::string GetIScsiCliPath()
{
    std::string cmd ;
    char systemRoot[MAX_PATH] ;
    if( 0 == GetSystemDirectoryA( systemRoot, sizeof(systemRoot)) )
    {
		DebugPrintf("GetSystemDirecotry() API Failed. Error :%d\n", GetLastError());
    }
    cmd = systemRoot ;
    if( cmd[cmd.length() - 1] != '\\' )
    {
        cmd += "\\" ;
    }
    cmd += "iscsicli.exe" ;
    return cmd ;

}

void GetVolumePaths(PCHAR VolumeName, std::list<std::string> volumepaths)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DWORD  cChar      = MAX_PATH + 1;
    PCHAR  volPaths   = NULL;
    BOOL   bRet       = FALSE;
#ifndef SCOUT_WIN2K_SUPPORT
    while(true) 
    {
        volPaths = (PCHAR) new BYTE [cChar * sizeof(CHAR)];
        if ( NULL == volPaths ) 
        {
			DebugPrintf(SV_LOG_FATAL, "Out of memory\n") ;
            return;
        }
        bRet = GetVolumePathNamesForVolumeNameA(VolumeName, volPaths, cChar, &cChar);
        if( bRet )
            break;

        if( GetLastError() != ERROR_MORE_DATA ) 
            break;

        delete [] volPaths;
        volPaths = NULL;
    }
#endif
    if( bRet )
    {
        for (PCHAR volPathsIdx = volPaths; volPathsIdx[0] != '\0'; volPathsIdx += strlen(volPathsIdx) + 1 ) 
        {
            char volumebuf[MAX_PATH + 1] ;
			inm_sprintf_s(volumebuf, ARRAYSIZE(volumebuf), "  %s", volPathsIdx);
            volumepaths.push_back(volumebuf) ;
        }
    }

    if ( volPaths != NULL )
        delete [] volPaths;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void EnumerateVolumes(std::map<std::string, std::list<std::string> >& volumeNamesMap )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    CHAR   szDvcName[MAX_PATH]	= "";
    HANDLE hFind				= INVALID_HANDLE_VALUE;
    CHAR  volName[MAX_PATH]		= "";

    hFind = FindFirstVolume(volName, ARRAYSIZE(volName));

    if (hFind == INVALID_HANDLE_VALUE)
    {
        DebugPrintf(SV_LOG_ERROR, "FindFirstVolume failed with error code %d\n", GetLastError());
        return;
    }

    while(true)
    {
        size_t index = strlen(volName) - 1;

        if (volName[0] != '\\' || volName[1] != '\\' || volName[2] != '?'  || volName[3] != '\\' || volName[index] != '\\') 
        {
            DebugPrintf(SV_LOG_ERROR, "FindFirstVolumeA/FindNextVolumeA returned a bad path: %s\n", volName);
            break;
        }
        volName[index] = '\0';

        DWORD cCount = QueryDosDevice(&volName[4], szDvcName, ARRAYSIZE(szDvcName)); 

        volName[index] = '\\';

        if ( cCount == 0 ) 
        {
            DebugPrintf(SV_LOG_ERROR, "QueryDosDevice failed with error code %d\n", GetLastError());
            break;
        }

        std::list<std::string> volumes ;
        GetVolumePaths(volName,volumes);
        volumeNamesMap.insert(std::make_pair(volName, volumes)) ;

        if ( !FindNextVolume(hFind, volName, ARRAYSIZE(volName)) ) 
        {
			DWORD dwLstError  = GetLastError();
            if ( dwLstError != ERROR_NO_MORE_FILES) 
            {
                DebugPrintf(SV_LOG_ERROR,"FindNextVolume failed with error code %d\n", dwLstError);
            }
            break;
        }
    }

    FindVolumeClose(hFind);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void GetVolumeNameToDiskMappings(const std::map<std::string, std::list<std::string> >& volumeNamesMap, 
                                 std::map<std::string, std::string>& volumeToDiskMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, std::list<std::string> >::const_iterator begIter, endIter ;
    begIter = volumeNamesMap.begin() ;
    endIter = volumeNamesMap.end() ;
    while( begIter != endIter )
    {
        std::string volumdGuid = begIter->first ;
        std::string::size_type pos = volumdGuid.find_last_of("\\");
        if(pos != std::string::npos)
        {
            volumdGuid.erase(pos);
        }
        HANDLE	hVolume;	
		hVolume = CreateFileA(volumdGuid.c_str(),GENERIC_READ, 
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT  , NULL 
                             );
		if( hVolume == INVALID_HANDLE_VALUE ) 
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open handle for volume-%s with Error Code - %d \n", volumdGuid.c_str(), GetLastError());
			break;
		}

		ULONG	bytesWritten;
        UCHAR	DiskExtentsBuffer[0x400];
        PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
		if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                            NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer),
                            &bytesWritten, NULL) ) 
		{	
            SV_ULONG uDiskNumber;
			for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
				uDiskNumber	= DiskExtents->Extents[extent].DiskNumber;
			}
            char  DiskName[MAX_PATH + 1];
			inm_sprintf_s(DiskName, ARRAYSIZE(DiskName), "%s%lu", WIN_DISKNAME_PREFIX, uDiskNumber);
            volumeToDiskMap.insert(std::make_pair(volumdGuid, DiskName)) ;

		}
		else
		{
            DebugPrintf(SV_LOG_WARNING,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed for volume-%s with Error code - %lu.\n", volumdGuid.c_str(), GetLastError());
			CloseHandle(hVolume);
            hVolume = INVALID_HANDLE_VALUE ;
		}
        begIter++ ;
        if( INVALID_HANDLE_VALUE != hVolume )
		    CloseHandle(hVolume);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool AddTargetPortal(const std::string& targetIp)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    cmdStream<< GetIScsiCliPath() << " QAddTargetPortal "<< targetIp ;
    AppCommand cmd(cmdStream.str(), 300) ;
    SV_ULONG exitCode ;
    std::string output ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    cmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
    if( exitCode == 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "Added target Portal %s\n", targetIp.c_str()) ;
        bRet = true ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to add the target portal %s with exit code %d\n", targetIp.c_str(), exitCode) ;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool CheckWhetherAlreadyLoggedIn(const std::string& targetIp, const std::string& targetName)
{
    bool bRet ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    SV_ULONG exitCode ;
    std::string output ;
    cmdStream << GetIScsiCliPath()<< " TargetInfo " << targetName ;
    AppCommand cmd(cmdStream.str(), 300) ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    cmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
    if( exitCode == 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "target %s is already available\n", targetName.c_str()) ;   
        bRet = true ;
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Target %s is not available yet. error code %d\n", targetName.c_str(), exitCode) ;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool AddTarget(const std::string& targetIp, const std::string& targetName)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    SV_ULONG exitCode ;
    std::string output ;
    cmdStream << GetIScsiCliPath() << " QAddTarget " << targetName ;
    AppCommand cmdAddTarget(cmdStream.str(), 300) ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    cmdAddTarget.Run(exitCode , output, Controller::getInstance()->m_bActive, "", h) ;
    if( exitCode == 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully added the target %s\n", targetName.c_str()) ;
        bRet = true ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to add the target %s with error %d\n", targetName.c_str(), exitCode) ;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool LoginTarget(const std::string& targetIp, const std::string& targetName, std::string& sessionId)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    SV_ULONG exitCode ;
    std::string output ;
    cmdStream << GetIScsiCliPath() << " QLoginTarget " << targetName ;
    AppCommand cmdLogin(cmdStream.str(), 300) ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    cmdLogin.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
    if( exitCode == 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "Logged into the target %s\n", targetName.c_str()) ;
        bRet = true ;
    }
    else
    {
        if( output.find("The target has already been logged in" ) != std::string::npos)
        {
            DebugPrintf(SV_LOG_DEBUG, "Already logged in %s\n", targetName.c_str()) ;
            bRet = true ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to login to the target %s with error code %d\n", targetName.c_str(), exitCode) ;
        }
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool PersistTargetSession(const std::string& targetIp, const std::string& targetName)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    std::string output ;
    SV_ULONG exitCode ;
    cmdStream << GetIScsiCliPath() << " persistentlogintarget " << targetName << " T * * * * * * * * * * * * * * * 0" ;
    AppCommand persistSessionCmd(cmdStream.str(), 300) ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    persistSessionCmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h);
    if( exitCode == 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully persisted the login session for target name %s\n" , targetName.c_str()) ;
        bRet = true ;
    }
    else
    {
        if( output.find("The target has already been logged in via") != std::string::npos )
        {
           bRet = true ;
        }
        else
        {
           DebugPrintf(SV_LOG_ERROR, "Failed to persist the login session\n") ;
        }
    }
    if( bRet )
    {
        OSVERSIONINFOEX osVersionInfo ;
        getOSVersionInfo(osVersionInfo) ;
        if(osVersionInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
        {
            ClusterOp objClusterOp;
            std::string outputFileName ;
            Controller::getInstance()->QuitRequested(10) ;
            if(objClusterOp.BringW2k8ClusterDiskOnline(outputFileName,exitCode))
            {
                if(exitCode != 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to bring the disks online\n") ;
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "Brought disks online\n") ;
                }
            }
        }
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool RemovePersistentTarget(const std::string& targetIp, const std::string& targetName)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    cmdStream << GetIScsiCliPath() << " listpersistenttargets" ;
    AppCommand cmd(cmdStream.str(), 300) ;
    SV_ULONG exitCode ;
    std::string output ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    cmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
    std::string initiatorName ;
    if( exitCode == 0 )
    {
        bool ok = false ;
        std::stringstream opstream ;
        opstream.str(output) ;
        char line[1024] ;
        while( opstream.getline(line,1024) && !opstream.eof() )
        {
            std::string lineStr = line ;
            if( lineStr.find("Target Name") != std::string::npos )
            {
                if( lineStr.find(targetName) != std::string::npos )
                {
                    ok = true ;
                }
            }
            if( ok )
            {
                if( lineStr.find("Initiator Name") != std::string::npos )
                {
                    initiatorName = lineStr.substr(lineStr.find_first_of(":") + 1 ) ;
                    break ;
                }
            }
        }
    }
    if( initiatorName.compare("") == 0 )
    {
        DebugPrintf(SV_LOG_WARNING, "Nothing to unpersist for the target %s\n", targetName.c_str()) ;
        bRet = true ;
    }
    else
    {
        cmdStream.str("") ;
        cmdStream.clear() ;
        cmdStream << GetIScsiCliPath()<< " removepersistenttarget " << " " << initiatorName << " " << targetName << " * " << targetIp <<  " " << 3260 ;
        AppCommand unpersistCmd(cmdStream.str(), 300) ;
        Controller::getInstance()->QuitRequested(3) ;
        unpersistCmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
        if( exitCode == 0 )
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully unpersisted the target %s\n", targetName.c_str()) ;
            bRet = true ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unpersist the target %s\n", targetName.c_str()) ;
        }
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool LogoutFromTargetSession(const std::string& targetIp, const std::string& targetName)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
    SV_ULONG exitCode ;
    std::string output ;
    cmdStream << GetIScsiCliPath() << " reporttargetmappings" ;
    AppCommand tgtMappingsCmd(cmdStream.str(), 300) ;
    Controller::getInstance()->QuitRequested(3) ;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    tgtMappingsCmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
    if( exitCode == 0 )
    {
        std::string sessionId ;
        std::stringstream opstream ;
        opstream.str(output) ;
        char line[1024]; 
        bool ok = false; 
        while( opstream.getline(line, 1024) && !opstream.eof() )
        {
            std::string lineStr = line ;
            if( lineStr.find("Session Id") != std::string::npos )
            {
                sessionId = lineStr.substr(lineStr.find_first_of(":") + 1 ) ;
            }
            if( lineStr.find("Target Name") != std::string::npos )
            {
                if(lineStr.find(targetName) != std::string::npos )
                {
                    ok = true ;
                    break ;
                }
            }
        }
        if( sessionId.compare("") != 0 )
        {
            DebugPrintf(SV_LOG_DEBUG, "Session id for target %s is %s\n", targetName.c_str(), sessionId.c_str()) ;
            cmdStream.str("") ;
            cmdStream.clear() ;
            cmdStream << GetIScsiCliPath() << " logouttarget " << sessionId ;
            AppCommand logoutCmd(cmdStream.str(), 300) ;
            Controller::getInstance()->QuitRequested(3) ;
            logoutCmd.Run(exitCode, output, Controller::getInstance()->m_bActive, "", h) ;
            if( exitCode == 0 )
            {
                DebugPrintf(SV_LOG_DEBUG, "Successfully logged out from the target session for %s\n", targetName.c_str()) ;
                bRet = true ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to logout from target session with error %s\n", exitCode) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "No session for target %s\n", targetName.c_str()) ;
            bRet = true ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to report target mappings. exit Code %s\n", exitCode) ;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool AssignDriveLetter(const std::string& dosDeviceName, const std::string& oldmountpoint, const std::string& mountPoint)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, std::list<std::string> > volumeNameToPathsMap ;
    std::map<std::string, std::string> volumeNameToDiskMap ;
    EnumerateVolumes(volumeNameToPathsMap) ;
    GetVolumeNameToDiskMappings(volumeNameToPathsMap, volumeNameToDiskMap) ;
    std::map<std::string, std::string>::iterator volumeNameToDiskIterBeg, volumeNameToDiskIterEnd ;
    volumeNameToDiskIterBeg = volumeNameToDiskMap.begin() ;
    volumeNameToDiskIterEnd = volumeNameToDiskMap.end() ;
    while( volumeNameToDiskIterBeg != volumeNameToDiskIterEnd )
    {
        if( strcmpi(volumeNameToDiskIterBeg->second.c_str(),dosDeviceName.c_str()) == 0 )
        {
            if( oldmountpoint.compare("") != 0 )
            {
                if( DeleteVolumeMountPoint(oldmountpoint.c_str()) )
                {   
                    DebugPrintf(SV_LOG_DEBUG, "Removed old mount point\n") ;
                }   
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to Removed old mount point %d\n", GetLastError()) ;
                }
            }
            std::string volumeguid = volumeNameToDiskIterBeg->first  ;
            volumeguid += "\\" ;
            if( SetVolumeMountPoint(mountPoint.c_str(), volumeguid.c_str()) )
            {
                DebugPrintf(SV_LOG_DEBUG, "Successfully set the mount point for %s to %s\n", volumeNameToDiskIterBeg->first.c_str(), mountPoint.c_str()) ;
                bRet = true ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to set the mount point for %s to %s with error %d\n", volumeNameToDiskIterBeg->first.c_str(), mountPoint.c_str(), GetLastError()) ;
            }
            break ;
        }
        volumeNameToDiskIterBeg ++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool AssignDriveLetterIfRequired(const std::string& targetName, const std::string& mountpoint)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_ULONG exitCode ;
    std::stringstream cmdStream ; 
    cmdStream << GetIScsiCliPath() << " SessionList" ;
    std::string output ;
    AppCommand cmd (cmdStream.str(), 300);
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    cmd.Run(exitCode, output,Controller::getInstance()->m_bActive, "", h) ;
    bool foundSession = false ;
    std::string dosDeviceName ;
    std::string volumePath ;
    if( exitCode == 0 )
    {
        char line[1024];
        std::stringstream opstream ;
        opstream.str(output) ;
        std::string lineStr ;
        while( opstream.getline(line, 1024) && !opstream.eof() )
        {
            lineStr = line ;
            if( lineStr.find("Target Name") != std::string::npos )
            {
                if( lineStr.find(targetName) != std::string::npos )
                {
                    foundSession = true ;
                    break ;
                }
            }
        }
        if( foundSession )
        {
            while( opstream.getline(line, 1024) && !opstream.eof() )
            {
                lineStr = line ;
                if( lineStr.find("Legacy Device Name") != std::string::npos )
                {
                    DebugPrintf(SV_LOG_DEBUG, "line %s and length %d\n", lineStr.c_str(), lineStr.length()) ;
                    dosDeviceName = lineStr.substr(lineStr.find_first_of(":") + 1) ;
                    std::string stuff_to_trim = " \n\b\t\a\r\xc" ;
                    dosDeviceName.erase( dosDeviceName.find_last_not_of(stuff_to_trim) + 1) ;
                    dosDeviceName.erase(0 , dosDeviceName.find_first_not_of(stuff_to_trim) ) ;
                    if( dosDeviceName.compare("") != 0 )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Dos Device Name %s\n", dosDeviceName.c_str()) ;
                        break ; 
                    }
                }
            }

            while( opstream.getline(line, 1024) && !opstream.eof() )
            {
                lineStr = line ;
                if( lineStr.find("Volume Path Names") != std::string::npos )
                {
                    opstream.getline(line, 1024) ;
                    lineStr = line ;
                    std::string stuff_to_trim = " \n\b\t\a\r\xc" ;
                    lineStr.erase( lineStr.find_last_not_of(stuff_to_trim) + 1) ;
                    lineStr.erase(0 , lineStr.find_first_not_of(stuff_to_trim) ) ;

                    if( lineStr.compare("") != 0 && lineStr.find("Session Id") == std::string::npos )
                    {
                        volumePath = lineStr ;
                        DebugPrintf(SV_LOG_DEBUG, "Volume Name %s\n", volumePath.c_str()) ;
                    }
                    break ;
                }
            }
        }
        if( dosDeviceName.compare("") != 0 )
        {
            std::string mountPointforAssigning = mountpoint ;
            if( mountPointforAssigning.length() == 1 )
            {
                mountPointforAssigning += ":\\" ;
            }
            if( mountPointforAssigning.length() > 3 )
            {
                if( mountPointforAssigning[mountPointforAssigning.length() - 1 ] != '\0' )
                {
                    mountPointforAssigning += "\\" ;
                }
            }
            if( volumePath.compare(mountPointforAssigning) != 0 )
            {
                bRet = AssignDriveLetter(dosDeviceName, volumePath, mountPointforAssigning) ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "No need to assign the drive letter\n") ;
                bRet = true ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Couldn't get the dos device name for the target %s\n", targetName.c_str()) ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "failed to get the session list with error %s\n", exitCode) ;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool LoginToTarget(const std::string& targetIp, const std::string& targetName, std::string& sessionId, const std::string& mountpoint)
{
    bool bRet = false ;

    if( AddTargetPortal(targetIp) )
    {
        if( AddTarget(targetIp, targetName) )
        {
            if( LoginTarget(targetIp, targetName, sessionId) )
            {
                if( PersistTargetSession(targetIp, targetName) )
                {
                    bRet = AssignDriveLetterIfRequired(targetName, mountpoint) ;
                }
            }
        }
    }
    return bRet ;
}


bool LogoutFromTarget(const std::string& targetIp, const std::string& targetName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;\
    bool bRet = false ;
    if( RemovePersistentTarget(targetIp, targetName) )
    {
        bRet = LogoutFromTargetSession(targetIp, targetName);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool ExportDeviceUsingIscsi(const std::string& deviceName, DeviceExportParams& exportParams, std::stringstream& stream)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string targetname, targetip, sessionid, mountpoint, devicenameafterlogin ;
    std::string operation ;
    if( exportParams.m_Exportparams.find("TargetName") != exportParams.m_Exportparams.end() )
    {
        targetname = exportParams.m_Exportparams.find("TargetName")->second ;
    }
    if( exportParams.m_Exportparams.find("TargetIP") != exportParams.m_Exportparams.end() )
    {
        targetip = exportParams.m_Exportparams.find("TargetIP")->second ;
    }
    if( exportParams.m_Exportparams.find("SessionId") != exportParams.m_Exportparams.end() )
    {
        sessionid = exportParams.m_Exportparams.find("SessionId")->second ;
    }
    if( exportParams.m_Exportparams.find("MountPoint") != exportParams.m_Exportparams.end() )
    {
        mountpoint = exportParams.m_Exportparams.find("MountPoint")->second ;
    }
    if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
    {
        operation = exportParams.m_Exportparams.find("Operation")->second ;
    }
    DebugPrintf(SV_LOG_DEBUG, "Target Name %s\n", targetname.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "Target IP %s\n", targetip.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "session id %s\n", sessionid.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "mount point %s\n", mountpoint.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "operation %s\n", operation.c_str()) ;
    //AddTargetPortal("testmachine") ;    
    if( StartSvc("msiscsi") == SVS_OK )
    {
        if( operation.compare("Login") == 0 )
        {
            if( targetname.compare("") == 0 ||
                targetip.compare("") == 0 )
            {
                DebugPrintf(SV_LOG_DEBUG, "some of the Non-optional parameters not available\n") ; 
            }
            else
            {
                exportParams.m_Exportparams.insert(std::make_pair("SessionId", "SessionId"));
                if( LoginToTarget(targetip, targetname, sessionid, mountpoint) )
                {
                    bRet = true ;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Error in logging in to the target session\n") ;
                }
            }
        }
        else if( operation.compare("Logout") == 0 )
        {
            if( targetname.compare("") == 0 ||
                targetip.compare("") == 0 )
            {
                DebugPrintf(SV_LOG_DEBUG, "some of the Non-optional parameters not available\n") ; 
            }
            else
            {
                if( LogoutFromTarget(targetip, targetname) )
                {
                    bRet = true ;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to un-persist the session %s\n", sessionid.c_str()) ;
                }
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to start the msiscsi service\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ExportDeviceUsingCIFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ExportDeviceUsingNFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)
{
    DebugPrintf(SV_LOG_ERROR,"ExportDeviceUsingNFS not yet supported\n") ;
    return false ;
}
