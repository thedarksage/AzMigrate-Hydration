#ifndef _NPWWN__MINOR_H_
#define _NPWWN__MINOR_H_

#define SYS_CLASSDIR "/sys/class"
#define SYS_DEVSDIR "/sys/devices/platform"
#define HOST_DIRENT "host"
#define NODE_FILENAME "node_name"
#define PORT_FILENAME "port_name"
#define TARGETIDFILENAME "scsi_target_id"
#define ROLESFILENAME "roles"
#define FCHOSTSTATEFILE "port_state"
#define FCHOSTONLINESTATE "Online"

#define PROC_SCSIDIR "/proc/scsi"
#define QLADIR       "qla2xxx"
/* This has to match once and
 * break out */
#define NODENAMEKEY  "-adapter-node="
#define PORTNAMEKEY  "-adapter-port="
/* not using for now */
#define QLAKEY "scsi-qla"
/* this is supposed to work since 
 * for remote ports, if formatting 
 * of pwwns succeed, then only 
 * we treat it as remote */
#define TGTKEY "-target-"

#define WWPNLABEL "WWPN"
#define WWNNLABEL "WWNN"

#define RPORT "rport"
#define FCRPORTDIR "fc_remote_ports"
#define LPSEQRPSEQSEP ":"
#define RPORTLPSEQSEP "-"
#define BUSRPSEQSEP "-"

#define INITIATOR_NAME  "initiatorname"
#define ISCSI_SESSIONDIR    "iscsi_session"
#define SESSION         "session"
#define TARGET_NAME     "targetname"
#define HOST            "host"
#define DEVICE          "device"
#define CONNECTION      "connection"
#define ADDRESS         "address"
#define ISCSI_CONNECTIONDIR    "iscsi_connection"

#define PROCLPFCDIR "/proc/scsi/lpfc"
#define WWPNKEY "WWPN"
#define RPWWN_SEP_LPFC ':'
#define TIDCHAR 't'

#define SINGLEDEVICESCANCMD "scsi add-single-device"
#define SINGLEDEVICEREMCMD "scsi remove-single-device"
#define QLADRVSCANDIR "/proc/scsi/qla2xxx/"
#define QLADRVSCANCMD "scsi-qlascan"

#define OSNAMESPACEFORDEVICES "/dev"

#define SYSDELETEFILE "delete"
#define SYS "/sys"

void GetWwnFromSysClass(
                        const std::string &dir,
                        const int &subdirindex,
                        const std::string &hostdirent,
                        const std::string &nodefile,
                        const std::string &portfile,
                        LocalPwwnInfos_t &lpwwns
                       );
void GetWwnFromProcScsi(
                        const std::string &dir,
                        const std::string &nodekey,
                        const std::string &portkey,
                        LocalPwwnInfos_t &lpwwns
                       );
void GetWwnUsingKey(
                    const std::string &dname, 
                    const std::string &filename, 
                    const std::string &nodekey, 
                    const std::string &portkey, 
                    LocalPwwnInfos_t &lpwwns
                   );

void GetATLunPathFromPIATWwn(const PIAT_LUN_INFO &piatluninfo,
                             const LocalPwwnInfo_t &lpwwn, 
                             const bool &bshouldscan,
                             Channels_t &channelnumbers,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromPIATWwn(const PIAT_LUN_INFO &piatluninfo, 
                            const LocalPwwnInfo_t &lpwwn,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn);

void DeleteATForPIAT(const PIAT_LUN_INFO &piatluninfo, 
                     const LocalPwwnInfo_t &lpwwn, 
                     Channels_t &channels,
                     QuitFunction_t &qf,
                     RemotePwwnInfo_t &rpwwn);

void UpdateATLunState(const PIAT_LUN_INFO &piatluninfo,
                      const std::string &vendor,
                      const LocalPwwnInfo_t &lpwwn,
                      const Channels_t::size_type nchannels,
                      SV_LONGLONG *pchannels,
                      RemotePwwnInfo_t &rpwwn);

void UpdateATLunStateFromChannel(const PIAT_LUN_INFO &piatluninfo, 
                                 const std::string &vendor,
                                 const LocalPwwnInfo_t &lpwwn,
                                 SV_LONGLONG *pchannel,
                                 RemotePwwnInfo_t &rpwwn);

std::string FormSingleScanCmd(const SV_LONGLONG host,
                              const SV_LONGLONG channel,
                              const SV_LONGLONG target,
                              const SV_ULONGLONG lun);

std::string FormFullScanCmd(const LocalPwwnInfo_t &lpwwn);

std::string GetDevtFileForLun(const SV_LONGLONG host,
                              const SV_LONGLONG channel,
                              const SV_LONGLONG target,
                              const SV_ULONGLONG lun);

std::string GetVendorFromProc(const SV_LONGLONG host,
                              const SV_LONGLONG channel,
                              const SV_LONGLONG target,
                              const SV_ULONGLONG lun,
                              const std::string &lunName,
                              const std::string &vendorName,
                              RemotePwwnInfo_t &rpwwn);


bool IsMatchingVendorFound(const std::string &hostinfo,
                           const SV_LONGLONG host,
                           const SV_LONGLONG channel,
                           const SV_LONGLONG target,
                           const SV_ULONGLONG lun,
                           std::string &vendor,
                           std::string &lunModel);

bool DriverScanFromUnknownSrc(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromFcHost(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromScsiHost(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromIscsiHost(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromQla(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromFcInfo(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromScli(const LocalPwwnInfo_t &lpwwn);

bool DriverScanFromLpfc(const LocalPwwnInfo_t &lpwwn);

typedef bool (*DriverScan_t)(const LocalPwwnInfo_t &lpwwn);

const DriverScan_t DriverScan[] = {
                                   DriverScanFromUnknownSrc, DriverScanFromFcHost, DriverScanFromScsiHost, 
                                   DriverScanFromQla, DriverScanFromFcInfo, DriverScanFromScli, 
                                   DriverScanFromLpfc, DriverScanFromIscsiHost
                                  };

bool UpdateATNumberFromFcHost(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                              const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                              QuitFunction_t &qf);

bool UpdateATNumberFromFcInfo(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                              const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                              QuitFunction_t &qf);

bool UpdateATNumberFromUnknownSrc(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                  const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                  QuitFunction_t &qf);

bool UpdateATNumberFromScsiHost(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                QuitFunction_t &qf);

bool UpdateATNumberFromIscsiHost(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                QuitFunction_t &qf);

bool UpdateATNumberFromQla(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                           const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf);

bool UpdateATNumberFromScli(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                            const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf);

bool UpdateATNumberFromLpfc(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                            const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf);


typedef bool (*UpdateATNumberFromPIAT_t)(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                         const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                         QuitFunction_t &qf);

const UpdateATNumberFromPIAT_t UpdateATNumberFromPIAT[] = {
                                                           UpdateATNumberFromUnknownSrc, UpdateATNumberFromFcHost, UpdateATNumberFromScsiHost, 
                                                           UpdateATNumberFromQla, UpdateATNumberFromFcInfo, UpdateATNumberFromScli, 
                                                           UpdateATNumberFromLpfc, UpdateATNumberFromIscsiHost
                                                          };

std::string DeleteSingleDeviceCmd(const LocalPwwnInfo_t &lpwwn, 
                                  const RemotePwwnInfo_t &rpwwn,
                                  const SV_ULONGLONG *patlunnumber);

std::string SysDeleteCmd(const LocalPwwnInfo_t &lpwwn, 
                         const RemotePwwnInfo_t &rpwwn,
                         const SV_ULONGLONG *patlunnumber);

/* TODO: make to 120 seconds, if 
 *       required after testing */
#define NSECS_TO_WAIT_FOR_ATLUN 60

/* TODO: made a common define since 
 *       if there has to be changes, 
 *       just change the common value; 
 *       On testing, if needed the values 
 *       can be separated */
#define NSECS_TO_WAIT_FOR_VALID_ATLUN NSECS_TO_WAIT_FOR_ATLUN
#define NSECS_TO_WAIT_FOR_ATLUN_DELETE NSECS_TO_WAIT_FOR_ATLUN

void SingleDeviceScan(const PIAT_LUN_INFO &piatluninfo,
                      const LocalPwwnInfo_t &lpwwn, 
                      const Channels_t::size_type nchannels,
                      SV_LONGLONG *pchannels,
                      QuitFunction_t &qf,
                      RemotePwwnInfo_t &rpwwn);

void FullDeviceScan(const PIAT_LUN_INFO &piatluninfo,
                    const LocalPwwnInfo_t &lpwwn, 
                    const Channels_t::size_type nchannels,
                    SV_LONGLONG *pchannels,
                    QuitFunction_t &qf,
                    RemotePwwnInfo_t &rpwwn);

typedef void (*ScanAT_t)(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn, 
                         const Channels_t::size_type nchannels,
                         SV_LONGLONG *pchannels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn);

const ScanAT_t ScanAT[] = {SingleDeviceScan, FullDeviceScan};

const char * const ScanATStr[] = {"single device scan", "full device scan"};

#endif /* _NPWWN__MINOR_H_ */
