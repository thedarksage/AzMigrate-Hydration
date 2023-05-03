#ifndef __SYSTEM__H
#define __SYSTEM__H
#include <string>
#include "appglobals.h"
#include <list>


SVERROR getInterfaceGuidList(std::list<std::string>& list) ;
SVERROR getNetworkNameFromGuid(const std::string& interfaceguid, std::string& networkname) ;
bool isDynamicDNSRegistrationEnabled(const std::string& interfaceguid);
bool isDynamicDNSRegistrationEnabled() ;
void ChangeDynamicDNSUpdateStatus(const std::string& interfaceguid, bool enable);
void ChangeDynamicDNSUpdateStatus(bool enable) ;
void RunSystemTests() ;
SVERROR getEnvironmentVariable(const std::string& variableName, std::string& value) ;
SVERROR getDnsName(std::string& domainName) ;
SVERROR getIPAddressFromGuid(const std::string& interfaceguid, std::string& ipaddress) ;
SVERROR isFirewallEnabled(InmFirewallStatus& status) ;
SVERROR getNetBiosName( std::string& netBiosName) ;
bool RebootSystems(const char* pSystemTobeShutDown, bool bForce, char* szDomain,char* szUser,char* szPassword);
bool SetShutDownPrivilege() ;
SVERROR setDynamicDNSUpdateKey(SV_LONGLONG value);
bool isDisableDynamicUpdateSet();
bool CheckCacheDirSpace(std::string& cachedir, SV_ULONGLONG& capacity, SV_ULONGLONG& freeSpace, SV_ULONGLONG& expectedFreeSpace);
bool checkCMMinReservedSpacePerPair(std::string& cachedir, SV_ULONGLONG& capacity, SV_ULONGLONG& freeSpace, SV_ULONGLONG& expectedFreeSpace, SV_ULONGLONG& m_totalNumberOfPairs, SV_ULONGLONG& totalSpaceForCM, SV_ULONGLONG& totalCMSpaceReq);
SV_ULONGLONG getCacheDirConsumedSpace(std::string& cacheDir);
void getOSVersionInfo(OSVERSIONINFOEX& osVersionInfo);

SVERROR readAndChangeServiceLogOn(HANDLE& userToken_);
SVERROR LogOnGrantProcess(HANDLE& userToken_);
SVERROR QueryAppServiceTofindAccountInfo(bool& isDomainAccount);
bool InMLogon(const std::string& userName, const std::string& domain, const std::string& password,HANDLE &userToken_);
SVERROR revertToOldLogon();
std::string GetSystemDir() ;
void getOSVersionInfo(OSVERSIONINFOEX& osVersionInfo) ;
#endif
