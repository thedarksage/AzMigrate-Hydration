#include <vector>
#include <iostream>
#include <string>
#include <iostream>
#include "portablehelpersmajor.h"
#include "unixvirtualvolume.h"
//#include "../drivers/invirvol/win32/VVDevControl.h"
#include "VsnapUser.h"
#include "file.h"
#include <boost/lexical_cast.hpp>
#include "../inmsdkutil.h"
#include "../InmsdkGlobals.h"
#include "inmageex.h"
//#include "InMageSecurity.h"
#include "../../disklayoutmgr/dlwindows.h"

//TCHAR InmszLocalPassword[] = _T("InMage@01&#2UIQdnpKl");
#define INM_MAX_PASSWD_LEN		256
#define OPTION_ENCRYPT				"encrypt"
#define OPTION_ENCRYPT_MYSQL		"mysql"
#define InmINMAGE_FAILVOER_KEY _T("Software\\SV Systems\\Failover\\")
#define InmINMAGE_FAILVOER_MYSQL_KEY _T("Software\\SV Systems\\Failover\\MySql\\")

bool ChangeNewRepoDriveLetter(const std::string& existingRepo,
                              const std::string& newRepo)
{
	return false ;
}

bool SetSparseAttribute( const std::string& file) 
{
	return false ;
}

int
RegisterSecurityHandleWithDriver()
{
    bool bResult = false ;
	return bResult ;
}
bool CredentailsAvailable()
{
	return false ;
}

bool  GetCredentials(std::string& domain, std::string& username, std::string& password)
{
    bool bResult = false ;
	return bResult ;
}
bool CloseConnection()
{
	return false ;
}
bool CloseConnection(const std::string& share)
{
	bool bret = true ;
	return bret ;
}
bool AddConnection(const std::string& share,
				   const std::string& user,
				   const std::string& password,
				   const std::string& domain,
				   bool isGuestAccess,
				   INM_ERROR& error,
				   std::string& errormsg)
{
	bool process = false ;
	return process ;
}
bool ImpersonateAccessToShare(INM_ERROR& error,
							  std::string& errormsg)
{
	bool bret = false ;
	return bret ;
}
bool SaveCredentialsInReg(const std::string& user, 
						  const std::string& password, 
						  const std::string& domain,
						  INM_ERROR& errCode,
						  std::string& errmsg)
{
	bool ret = false ;
	return ret ;
}

bool ImpersonateAccessToShare( const std::string& user, 
							   const std::string& password,
						       const std::string& domain,
							   INM_ERROR& error,
							   std::string& errormsg)
{
	bool ret = false ;
	return ret ;
}
bool AccessRemoteShare(INM_ERROR& error,
					   std::string& errormsg)
{
	bool ret = false ;
	return ret ;
}	

bool AccessRemoteShare(const std::string& share,
					   INM_ERROR& error,
					   std::string& errormsg, bool UnShareAndShare)
{
	bool process = false ;
	return process ;
}
bool AccessRemoteShare(const std::string& share,const std::string& username,const std::string& password,const std::string& domain,bool isGuestAccess,
					   INM_ERROR& error,
					   std::string& errormsg, bool UnShareAndShare)
{
	bool process = false ;
	return process ;
}
bool GetUsedSpace(const char * directory, const char * file_pattern,SV_ULONGLONG & filecount, SV_ULONGLONG & size_on_disk, SV_ULONGLONG& logicalsize )
{
	bool ret = false ;
	return ret ;
}
bool GetInMageRebootPendingKey()
{
	bool ret = false ;
	return ret ;
}
bool isShareReadWriteable(const std::string& sharepath,INM_ERROR & error,std::string& errormsg)
{
	bool ret = false ;
	return ret ;
}
bool SupportsSparseFiles(const std::string& path)
{
	bool ret = false ;
	return ret ;
}

bool WNetAddConnectionWrapper(const std::string& sharepath,
							  const std::string& user,
							  const std::string& password,
							  const std::string& domain,
							  INM_ERROR& error, 
							  std::string& errormsg)
{
	bool process = false ;
	return process ;
}
void RemCredentialsFromRegistry()
{
    
}
bool CopySparseFile(std::string &srcFile, std::string &tgtFile, unsigned long& err)
{
	bool ret = false ;
	return ret ;
}
bool ReadScoutMailRegistryKey (std::string &value)
{
	bool ret = false ;
	return ret ;
}
bool GetDiskNameForVolume(const DisksInfoMap_t& diskinfomap, const std::string& vol, std::string& diskname)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bret = false ;
	std::string volumename = vol ;
	if( volumename.length() == 1 )
	{
		volumename += ":\\" ;
	}
	if( volumename[volumename.length() - 1] != '\\' )
	{
		volumename += "\\" ;
	}
	DebugPrintf(SV_LOG_DEBUG, "No of disks on the system %d\n", diskinfomap.size()) ;
	DebugPrintf(SV_LOG_DEBUG, "Finding the disk name for %s\n", volumename.c_str()); 
	DisksInfoMap_t::const_iterator iter(diskinfomap.begin());
    DisksInfoMap_t::const_iterator iterEnd(diskinfomap.end());
	for (/* empty */; iter != iterEnd; ++iter) 
	{
		diskname = iter->second.DiskInfoSub.Name ;
		DebugPrintf(SV_LOG_DEBUG, "disk name %s\n", iter->second.DiskInfoSub.Name) ;
		VolumesInfoVec_t::const_iterator tgtVolIter((*iter).second.VolumesInfo.begin());
        VolumesInfoVec_t::const_iterator tgtVolIterEnd((*iter).second.VolumesInfo.end());
        for (/* empty */; tgtVolIter != tgtVolIterEnd; ++tgtVolIter) 
		{
			DebugPrintf(SV_LOG_DEBUG, "volume name %s\n", tgtVolIter->VolumeName) ;
			if( tgtVolIter->VolumeName == volumename )
			{
				DebugPrintf(SV_LOG_DEBUG, "Found the volume name %s in disk %s\n", volumename.c_str(), diskname.c_str()) ;
				bret = true ;
				break ;
			}
		}
		if( bret == true )
		{
			break ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
	return bret ;
}
void persistAccessCredentials(const std::string& repoPath, const std::string& username,const std::string& password,
							  const std::string& domain,INM_ERROR& errCode,
								std::string& errmsg, bool guestAccess)
{

}
							   
