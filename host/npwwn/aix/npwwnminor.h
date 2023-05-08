#ifndef _NPWWN__MINOR_H_
#define _NPWWN__MINOR_H_

#include <string>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <sys/types.h>
#include <cstdarg>
#include "svtypes.h"
#include "volumegroupsettings.h"
#include "prismsettings.h"

class Event;

#define FCINFOHBASTATEOFFLINE "offline"
#define FCINFOHBASTATEONLINE "online"

#define SCLICMDNAME "scli"
#define SCLIRPORTOPT "-t"
#define SCLISCANOPT "-do"
#define SCLISCANOPTSUFFIX "rs"
#define SCLICMD "scli -i all | /usr/bin/egrep \'(Node Name|Port Name)\'"
#define EMLXADMCMD "emlxadm / -y get_host_attrs all | /usr/bin/egrep \'(Node WWN|Port WWN)\' | /usr/bin/awk \'{ if (($0 ~ /Port WWN/) && (prev ~ /Node WWN/)) printf(\"%s\\n%s\\n\", prev, $0); prev=$0}\'"

const char WWN_SEP_SCLI = '-';

#define SCLILUNOPT "-l"

/* TODO: this value is coming from POC scripts 
 * should pick up from configurable parameters */
#define NSECS_WAIT_AFTER_DEVFSADM 2

#define EMLXADMCMDFORRPORTS "emlxadm / -y get_port_attrs all | /usr/bin/egrep \'(Host Port:|Port WWN)\'"

#define OSNAMESPACEFORDEVICES "/dev/dsk"
#define DEVFSADMCMD "/usr/sbin/devfsadm"

#define CONTROLLERCHAR 'c'
#define SLICECHAR 's'

#define CFGADMCMD "/usr/sbin/cfgadm"
#define CFGADMFUNCTIONOPT "-c"
#define CFGADMHARDWAREOPT "-c"
#define CONFIGUREARG "configure"
#define CFGADMCONTROLLERATSEP "::"
#define CFGADMCONTROLLERATLUNSEP ","
#define UNCONFIGUREARG "unconfigure"
#define UNUSABLESCSILUNARG "unusable_SCSI_LUN"
#define SHOWSCSILUNARG "show_SCSI_LUN"

#define LUXADMCMD "/usr/sbin/luxadm"
#define LUXADMCMDEXPERTOPT "-e"
#define OFFLINEARG "offline"

const char * const cfgadmopts[] = {"", "c", "f", "fc"};

/* Let this return 0 */
SV_ULONGLONG GetATCountFromUnknownSrcPI(const PIAT_LUN_INFO &piatluninfo, 
                                        const LocalPwwnInfo_t &lpwwn, 
                                        QuitFunction_t &qf);

SV_ULONGLONG GetATCountFromFcHostPI(const PIAT_LUN_INFO &piatluninfo, 
                                    const LocalPwwnInfo_t &lpwwn, 
                                    QuitFunction_t &qf);

SV_ULONGLONG GetATCountFromScsiHostPI(const PIAT_LUN_INFO &piatluninfo, 
                                      const LocalPwwnInfo_t &lpwwn, 
                                      QuitFunction_t &qf);

SV_ULONGLONG GetATCountFromQlaPI(const PIAT_LUN_INFO &piatluninfo, 
                                 const LocalPwwnInfo_t &lpwwn, 
                                 QuitFunction_t &qf);

SV_ULONGLONG GetATCountFromFcInfoPI(const PIAT_LUN_INFO &piatluninfo, 
                                    const LocalPwwnInfo_t &lpwwn, 
                                    QuitFunction_t &qf);

SV_ULONGLONG GetATCountFromScliPI(const PIAT_LUN_INFO &piatluninfo, 
                                  const LocalPwwnInfo_t &lpwwn, 
                                  QuitFunction_t &qf);

SV_ULONGLONG GetATCountFromLpfcPI(const PIAT_LUN_INFO &piatluninfo, 
                                  const LocalPwwnInfo_t &lpwwn, 
                                  QuitFunction_t &qf);

typedef SV_ULONGLONG (*GetATCountFromPI_t)(const PIAT_LUN_INFO &piatluninfo, 
                                           const LocalPwwnInfo_t &lpwwn, 
                                           QuitFunction_t &qf);

const GetATCountFromPI_t GetATCountFromPI[] = {
                                               GetATCountFromUnknownSrcPI, GetATCountFromFcHostPI, GetATCountFromScsiHostPI,
                                               GetATCountFromQlaPI, GetATCountFromFcInfoPI, GetATCountFromScliPI,
                                               GetATCountFromLpfcPI
                                              };
bool ScliScan(const LocalPwwnInfo_t &lpwwn);

void GetATLunDevices(const PIAT_LUN_INFO &piatluninfo, std::set<std::string> &atlundevices);

bool IsATVisibleInScli(const PIAT_LUN_INFO &piatluninfo, 
                       const LocalPwwnInfo_t &lpwwn,
                       const RemotePwwnInfo_t &rpwwn, 
                       const std::string &vendor);

bool RunDevfsAdm(void);

std::string FormCfgAdmArgs(const char *opts, ...);


bool IsATLunCleanedUpFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                                const LocalPwwnInfo_t &lpwwn,
                                QuitFunction_t &qf,
                                const RemotePwwnInfo_t &rpwwn);

std::string GetLunStateInCfgAdm(const LocalPwwnInfo_t &lpwwn,
                                const RemotePwwnInfo_t &rpwwn, 
                                const SV_ULONGLONG *patlunnumber);

std::string GetFcInfoOnlyDiskName(const std::string &dir, 
                                  const LocalPwwnInfo_t &lpwwn,
                                  const RemotePwwnInfo_t &rpwwn,
                                  const SV_ULONGLONG *patlunnumber);

void FcInfoLunNormalScan(const PIAT_LUN_INFO &piatluninfo, 
                         const LocalPwwnInfo_t &lpwwn,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn, 
                         const char *opts);

void FcInfoLunRetryScan(const PIAT_LUN_INFO &piatluninfo, 
                        const LocalPwwnInfo_t &lpwwn,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn, 
                        const char *opts);

typedef void (*FcInfoScan_t)(const PIAT_LUN_INFO &piatluninfo,
                             const LocalPwwnInfo_t &lpwwn,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn,
                             const char *opts);

const FcInfoScan_t FcInfoScan[] = {FcInfoLunNormalScan, FcInfoLunRetryScan};


bool IsFcInfoLunCleaned(const PIAT_LUN_INFO &piatluninfo, 
                        const LocalPwwnInfo_t &lpwwn,
                        QuitFunction_t &qf,
                        const RemotePwwnInfo_t &rpwwn, 
                        const char *opts);

bool IsFcInfoLunCleanedOnRetry(const PIAT_LUN_INFO &piatluninfo, 
                               const LocalPwwnInfo_t &lpwwn,
                               QuitFunction_t &qf,
                               const RemotePwwnInfo_t &rpwwn, 
                               const char *opts);

typedef bool (*FcInfoClean_t)(const PIAT_LUN_INFO &piatluninfo,
                              const LocalPwwnInfo_t &lpwwn,
                              QuitFunction_t &qf,
                              const RemotePwwnInfo_t &rpwwn,
                              const char *opts);

const FcInfoClean_t FcInfoClean[] = {IsFcInfoLunCleaned, IsFcInfoLunCleanedOnRetry};

bool UnconfigureAndConfigure(const std::string &uncfgcmd, 
                             const std::string &cfgcmd, 
                             QuitFunction_t &qf);

bool CollectRemotePortsForLpfcPwwn(const std::string &hostpwwninfo, LocalPwwnInfo_t *pLocalPwwnInfo);

std::string FindDiskEntryFromDiskName(const PIAT_LUN_INFO &piatluninfo, const std::string &dir, const std::string &onlydiskname);

void ScanFcInfoATLun(const PIAT_LUN_INFO &piatluninfo, 
                     const LocalPwwnInfo_t &lpwwn,
                     QuitFunction_t &qf,
                     RemotePwwnInfo_t &rpwwn);


void UpdateFcInfoATLunState(const PIAT_LUN_INFO &piatluninfo,
                            const std::string &vendor, 
                            const LocalPwwnInfo_t &lpwwn,
                            RemotePwwnInfo_t &rpwwn);

void GetAdapterFromPI(const LocalPwwnInfo_t &localPwwnInfo, QuitFunction_t &qf, std::string &fcadapter);
void GetRemotePwwnsFromFcAdapter(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf, std::string &fcadapter);
void GetRemotePwwnsFromIScsiAdapter(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf, std::string &adapter);
void GetATLunPathsFromAdapter(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &localPwwnInfo, RemotePwwnInfo_t &rpwwn, QuitFunction_t &qf, const std::string &fcadapter, const bool &bshouldscan);
void RunCfgMgr(const std::string &fcAdapter);
void DeleteOfflineATLunPathsFromAdapter(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn, RemotePwwnInfo_t &rpwwn, QuitFunction_t &qf, const std::string &fcadapter);
void GetLocalFcPwwnInfos(LocalPwwnInfos_t &lpwwns);
void GetLocalIScsiPwwnInfos(LocalPwwnInfos_t &lpwwns);
void GetLocalIScsiToePwwnInfos(LocalPwwnInfos_t &lpwwns);
void GetLocalIScsiSwPwwnInfos(LocalPwwnInfos_t &lpwwns);
void GetFcAdapterFromPI(const LocalPwwnInfo_t &localPwwnInfo, QuitFunction_t &qf, std::string &fcadapter);
void GetIScsiAdapterFromPI(const LocalPwwnInfo_t &localPwwnInfo, QuitFunction_t &qf, std::string &adapter);
void GetATLunPathsFromFcAdapter(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn, RemotePwwnInfo_t &rpwwn, QuitFunction_t &qf, const std::string &fcadapter, const bool &bshouldscan);
void GetATLunPathsFromIScsiAdapter(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn, RemotePwwnInfo_t &rpwwn, QuitFunction_t &qf, const std::string &adapter, const bool &bshouldscan);
void GetNameFromVpdForDisk(const std::string disk, std::string &applianceTargetLUNName);
#endif /* _NPWWN__MINOR_H_ */
