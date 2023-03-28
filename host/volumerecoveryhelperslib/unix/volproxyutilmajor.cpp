#include "volproxyutil.h"
#include <boost/algorithm/string/replace.hpp>
#include "executecommand.h"

bool mountRepository(const std::string& sharePath, const std::string& localPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bret = false;

	std::stringstream cmdStream, results;
	//cmdStream << "mount -t nfs " << sharePath << " " << localPath ;
    cmdStream << "mount -t cifs " << sharePath << " " << localPath << " -o guest";
	SVMakeSureDirectoryPathExists(localPath.c_str());
	if (executePipe(cmdStream.str(), results)) 
	{
		DebugPrintf(SV_LOG_DEBUG, "Mount operation %s successful\n", cmdStream.str().c_str()) ;
		bret = true;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Mount operation %s failed with error %s\n", cmdStream.str().c_str(),results.str().c_str()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bret;
}

void GetMountNameFromPath(const std::string& Actualsource, std::string& mountPoint)
{

}

bool unMountRepository(const std::string& repositoryPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bret = false;
	std::stringstream cmdStream, results;
	cmdStream << "umount " << repositoryPath;
    if (executePipe(cmdStream.str(), results)) 
	{
		DebugPrintf(SV_LOG_DEBUG, "UnMount operation %s successful\n", cmdStream.str().c_str()) ;
        bret = true;
    }
	else
	{
		DebugPrintf(SV_LOG_ERROR, "UnMount operation %s failed with error %s\n",cmdStream.str().c_str(),results.str().c_str()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bret;
}

void getSharedPathAndLocalPath( const std::string& targetIP, const std::string& shareName, std::string& sharePath, std::string& localPath )
{
    sharePath = "//" + targetIP + "/" + shareName ;
    localPath = "/" + shareName ;
    
    return ;
}

bool StopShellHWDetectionService()
 {
    return true ;    
}

bool StartShellHWDetactionService()
{
    return true ;
}

bool DisableAutoMount() 
{
    return true ;
}

bool EnableAutoMount() 
{
    return true ;
}

SV_ULONG EnableAutoMountUsingDiskPart( std::string& strInstallPath, SV_ULONG doNotRunDiskPart )
{
    return 0 ;
}

SV_ULONG DisableAutoMountUsingDiskPart( std::string& strInstallPath, SV_ULONG doNotRunDiskPart )
{
    return 0 ;
}
SV_ULONG BringVolumesOnlineUsingDiskPart( std::string &strInstallPath )
{
	return 0 ;
}
