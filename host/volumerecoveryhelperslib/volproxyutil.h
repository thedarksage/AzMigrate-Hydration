#ifndef VOLPROXY_UTIL_H
#define VOLPROXY_UTIL_H

#include <iostream>
#include <string>
#include "portable.h"
#include "portablehelpers.h"
#include "cdpcli.h"
#include "volumerecoveryhelperssettings.h"
#include "serializevolumerecoveryhelperssettings.h"
#include "xmlizevolumerecoveryhelperssettings.h"
#include "inmstrcmp.h"

bool mountRepository(const std::string& sharePath, const std::string& localPath) ;
void GetMountNameFromPath(const std::string& Actualsource, std::string& mountPoint) ;
bool mountVirtualVolume(const std::string& volpackName,const std::string& SrcVolume, std::string& volpackMountPath, bool &alreadyMounted);
bool unMountRepository(const std::string& repositoryName);
bool getlocalpath(ExportedRepoDetails& exprepoobj,std::string& localPath);
bool StopShellHWDetectionService() ;
bool StartShellHWDetactionService() ;
bool DisableAutoMount() ;
bool EnableAutoMount() ;
bool RunCDPCLI( const std::vector<std::string>& cdpcliArgs, CDPCli::cdpProgressCallback_t cdpProgressCallback = NULL,
                                                  void * cdpProgressCallbackContext = NULL ) ;
void getSharedPathAndLocalPath( const std::string& targetIP, const std::string& shareName, std::string& sharePath, std::string& localPath ) ;
SV_ULONG EnableAutoMountUsingDiskPart(  std::string& strInstallPath, SV_ULONG doNotRunDiskPart   )  ;
SV_ULONG DisableAutoMountUsingDiskPart(  std::string& strInstallPath, SV_ULONG doNotRunDiskPart  )  ;
SV_ULONG BringVolumesOnlineUsingDiskPart( std::string &strInstallPath ) ;
#endif/* util.h */
