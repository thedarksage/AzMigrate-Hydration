#ifndef XENSERVERDISCOVERY_H
#define XENSERVERDISCOVERY_H

#include <list>
#include <vector>
#include <algorithm>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>

#include "volumegroupsettings.h"
#include "executecommand.h"

#define DEV_MAPPER_PATH			"/dev/mapper/"
#define DEV						"/dev/"
#define VG_NAME_PREFIX			"VG_XenStorage-"
#define LV_NAME_PREFIX			"LV-"
#define VHD_NAME_PREFIX			"VHD-"
#define XAPI_LOST_CONNECTION	"Error: Connection refused (calling connect )"

static bool IsRepeatingHyphen( char ch1, char ch2 ) { return (('-' == ch1) && (ch2 == ch1)); }

//static void error();

bool check_xapi_service();

bool get_pool_param(const std::string pool_param, std::string& result);

bool get_vm_param(const std::string uuid, const std::string vm_param, std::string& vmname);

bool get_host_param(const std::string uuid, const std::string host_param, std::string& value);

bool get_vm_vbd_list(const std::string vmuuid, std::list<std::string>& vbdlist);

bool get_vbd_param(const std::string uuid, const std::string vbd_param, std::string& vdiuuid);

bool get_vbd_userdevice(const std::string vmuuid, const std::string vdiuuid, const std::string vbd_param, std::string& value);

bool get_vdi_param(const std::string uuid, const std::string vdi_param, std::string& sruuid);

bool get_vdi_writable(const std::string uuid, bool& isWritable);

bool get_sr_shared(const std::string& sruuid, bool& issharedsr);

bool get_sr_is_lvm(const std::string sruuid, bool &islvmsr);

bool get_pooledvdi_on_pooledvm(const std::string vmuuid, std::vector<std::string>& pvdilist);

bool get_all_pooled_vms(std::list<std::string>& poolvms);

bool get_all_vms(ClusterGroupInfos_t& vmlist);

bool get_hostname(std::string& hostname);

bool get_this_host(const std::string hostname, std::string& thishost);

bool get_device_uuid(const std::string device, std::string& uuid);

bool get_uuid_device(const std::string uuid, std::string& device);

bool is_shared_vdi(const std::string vdiuuid, bool &issharedvdi);

bool get_all_vms_on_host(const std::string hostuuid, std::list<std::string>& allvms);

bool get_all_vms_on_localhost(std::list<std::string>& allvms);

bool get_globalvmlist(std::list<std::string>& globallist);

bool get_globallist(std::string par , std::list<std::string>& globallist);

bool get_devmapper_path(const std::string vdiuuid, std::string& devmapperpath);

bool get_motion_task(std::list<std::string>& tasklist);

int is_xsowner(const std::string device, bool& owner);

bool is_lvavailable(const std::string lv, bool& avail);

bool getAllLogicalVolumes(std::list<std::string>& lvdevices);

int getAvailableVDIs(VDIInfos_t &);

void replaceHyphen(std::string &temp);

bool set_if_lvhdsr(const std::string sruuid, bool& lvhdsr);

bool isValidLV(const std::string lvname, bool& validLV);

int isXenPoolMember(bool& isMember, PoolInfo &);

int isPoolMaster(bool& isMaster);

int getXenPoolInfo(ClusterInfo &, VMInfos_t &, VDIInfos_t &);

void printXenPoolInfo(ClusterInfo &, VMInfos_t &, VDIInfos_t &);

#endif //XENSERVERDISCOVERY_H
