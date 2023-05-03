#ifndef UTIL_H
#define UTIL_H
#include <string>
#include <time.h>
#include <list>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <fstream>
#include <algorithm>
#include <functional>
#include <iterator>
#include <set>
#include <map>
#include "svtypes.h"

#define SAFE_ACE_CLOSE_HANDLE(hd) do { if( hd !=  ACE_INVALID_HANDLE ) {  ACE_OS::close(hd) ; hd = ACE_INVALID_HANDLE; } } while(false)

std::string convertTimeToString(time_t timeInSec);
time_t convertStringToTime(std::string szTime);
void trimSpaces( std::string& s);
std::string getInstallationPath();
std::string getAgentPath(std::string);
bool isfound(std::list<std::string>& , std::string);
bool isFoundInMap(std::map<std::string, std::string>& valueMap, std::string searchKey);
std::list<std::string> tokenizeString(const std::string,std::string );
void convertVolumeNames(std::string& volumeName, const bool& toUpper = true);
void sanitizeVolumePathName(std::string& volumePath);
std::string searchStringInList(std::list<std::string> &,const std::string &strPattern);
void SetupWinSockDLL();
std::string getFileContents(const std::string &strFileName);
SVERROR getSizeOfFile( const std::string& filePath, SV_ULONGLONG& fileSize );
void GetValueFromMap(std::map<std::string,std::string>& propertyMap,const std::string& propertyKey,std::string& KeyValue,bool bOtpional=true);
bool CopyConfFile(std::string const & SourceFile, std::string const & DestinationFile);
bool removeChars( const std::string& stuff_to_trim, std::string& s );
std::string getOutputFileName(SV_ULONG policyId,SV_INT groupId=0,SV_ULONGLONG instanceId=0);
std::string getPolicyFileName(SV_ULONG policyId, SV_INT groupId = 0, SV_ULONGLONG instanceId = 0);
void getFileNameForUpLoad(SV_ULONG policyId,SV_ULONGLONG instanceId,std::map<SV_INT,std::string>& uploadFileName);
bool CheckVolumesAccess(std::list<std::string> &volList, SV_ULONG count = 1 );
bool getSupportingApplicationNames(const std::string& serviceName, std::set<std::string>& selectedApplicationNameSet);
bool isDiscoveryApllicable(const std::set<std::string>& appNameSet);
bool isValidApplicationName(const std::string& serviceName, const std::string& appName);
bool isAvailableInTheSet(const std::string& appTypeinStr, const std::set<std::string>& selectedAppNameSet);
std::string generateLogFilePath( const std::string& cmd ) ;
void truncateApplicationPolicyLogs ();
std::string getInmPatchHistory(std::string pszInstallPath);
void GetLastNLinesFromBuffer(std::string const &strBuffer, int lastNLines, std::string &strFinalNLines);
#endif
