#ifndef INMSDK_UTIL_H
#define INMSDK_UTIL_H

#include "InmFunctionInfo.h"
#ifdef SV_WINDOWS
#include <windows.h>
#endif
#include "InmsdkGlobals.h"
#include "dlmapi.h"

const char PAIR_DIFF_SYNC[]		= "Differential Sync";
const char PAIR_RESYNC_STEP2[]	= "Resyncing (Step II)";
const char PAIR_RESYNC_STEP1[]	= "Resyncing (Step I)";
const char PAIR_PAUSE_PENDING[]	= "Pause Pending";
const char PAIR_RESUME_PENDING[] = "Resume Pending" ;
const char PAIR_PAUSED[]			= "Paused";
const char PAIR_PAUSE_TRACKING_PENDING[]		= "Pause Tracking Pending";
const char PAIR_PAUSE_TRACKING_COMPLETED[]	= "Pause tracking Completed";
const char PAIR_PAUSE_TRACKING_FAILED[]	= "Pause tracking Failed";
const char PAIR_DELETE_PENDING[]	= "Delete Pending";
const char PAIR_CLEANUP_DONE[] = "Cleanup Done" ;
const char PAIR_QUEUED[]			= "Queued";
const char PAIR_VOLUME_RESIZED[]	= "Volume Resized";
#define ALLOWED_ATTRIBUTES (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
enum PAIR_QUEUED_STATUS
{
	PAIR_IS_NONQUEUED,
	PAIR_IS_QUEUED
};

void WriteRequestToLog(FunctionInfo &funInfo, const std::string& repopathforhost);
void WriteResponseToLog(FunctionInfo &funInfo, const std::string& repopathforhost);
void LogMessageIntoFile(const std::string &logMessage, const std::string& repopathforhost);
std::string getHostLogPath() ;
std::string getTimeString(unsigned long long timeInSec) ;
std::string getConfigStorePath();
void getConfigStorePath(std::string& );
void getConfigStorePath1(std::string& configStorePath, std::string& hostId) ;
bool ImpersonateAccessToShare(INM_ERROR& error,
							  std::string& errormsg) ;
bool SaveCredentialsInReg(const std::string& user, 
						  const std::string& password, 
						  const std::string& domain,
						  INM_ERROR& errCode,
						  std::string& errmsg) ;
void persistAccessCredentials(const std::string& repoPath, const std::string& username,const std::string& password,
							  const std::string& domain,INM_ERROR& errCode,
								std::string& errmsg, bool guestAccess);

bool ImpersonateAccessToShare( const std::string& user, 
							   const std::string& password,
						       const std::string& domain,
							   INM_ERROR& error,
							   std::string& errormsg) ;
bool CredentailsAvailable() ;
bool GetCredentials(std::string& domain, std::string& username, std::string& password) ;
bool AccessRemoteShare(const std::string& share,INM_ERROR& error,
										std::string& errormsg, bool UnShareAndShare=false) ;
bool AccessRemoteShare(const std::string& share, const std::string& username,
					   const std::string& password, const std::string& domain,
					   bool isGuestAccess, INM_ERROR& error,
					   std::string& errormsg, bool UnShareAndShare=false) ;

bool AccessRemoteShare(INM_ERROR& error,
					   std::string& errormsg) ;
int RegisterSecurityHandleWithDriver() ;
std::string getConfigCachePath(const std::string& repolocation, const std::string& hostId) ;
std::string constructConfigStoreLocation( const std::string& reposiotyPath, const std::string& hostId ) ;
bool isRepositoryConfiguredOnCIFS( const std::string& repopath ) ;
std::string getRepositoryPath(std::string path) ;
std::string getLockPath(const std::string& repoLocationForHost="") ;
bool requireRepoLock(const FunctionInfo& FunData, const std::string& repopath, std::string& repolockpath) ;
bool requireRepoLockForHost( const FunctionInfo& FunData, const std::string& repoPathForHost, std::string& repolockpath); 
bool requireRepoLockForHost( const std::string& apiname, const std::string& repoPathForHost, std::string& repolockpath); 
bool shouldReleaseHostLockAfterRollingBackTransaction(const std::string& apiname) ;
std::string getCataloguePath(); 
bool ChangeNewRepoDriveLetter(const std::string& existingRepo,
                              const std::string& newRepo) ;
bool isShareReadWriteable(const std::string& sharepath, INM_ERROR& error, std::string& errormsg) ;
bool AddConnection(const std::string& share,
				   const std::string& user,
				   const std::string& password,
				   const std::string& domain,
				   bool isGuestAccess,
				   INM_ERROR& error,
				   std::string& errormsg) ;
bool SetSparseAttribute( const std::string& file) ;
bool CloseConnection(const std::string& sharepath) ;
bool CloseConnection() ;
bool WNetAddConnectionWrapper(const std::string& sharepath,
							  const std::string& user,
							  const std::string& password,
							  const std::string& domain,
							  INM_ERROR& error, 
							  std::string& errormsg) ;

bool GetInMageRebootPendingKey();
bool SupportsSparseFiles(const std::string& path ) ;
bool GetUsedSpace(const char * directory, const char * file_pattern, 
								 SV_ULONGLONG & filecount, SV_ULONGLONG & size_on_disk,SV_ULONGLONG& logicalsize);
void RemCredentialsFromRegistry() ;
bool ReadScoutMailRegistryKey (std::string &value);
bool CopySparseFile(std::string &srcFile, std::string &tgtFile, unsigned long& err);
bool GetDiskNameForVolume(const DisksInfoMap_t& diskinfomap, const std::string& vol, std::string& diskname) ;
#ifdef SV_WINDOWS
void CopyRange(HANDLE hFrom, HANDLE hTo, LARGE_INTEGER iPos, LARGE_INTEGER iSize) ; 
std::string GetErrorMessage(DWORD dword) ;
std::string ErrorMessageInReadableFormat (const std::string &repoPath , DWORD dword) ; 
#endif
#endif /* INMSDK_UTIL_H */
