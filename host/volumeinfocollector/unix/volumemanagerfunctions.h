#ifndef VOLUMEMANAGER__FUNS__H
#define VOLUMEMANAGER__FUNS__H

#include <string>
#include <set>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <iterator>
#include <functional>
#include <cstring>
#include "voldefs.h"
#include "portablehelpers.h"

typedef std::map<dev_t, std::string> DevtToDeviceName_t;
typedef DevtToDeviceName_t::const_iterator ConstDevtToDeviceNameIter_t;

const std::string UNIX_DIR_SEP = "/";

std::string GetOnlyDiskName(const std::string &devname);
void GetVxDmpNodes(std::set<std::string> &vxdmpnodes);
bool IsVxDmpNode(
                 const std::string &node,
                 const std::set<std::string> &vxdmpnodes
                );
std::string GetPartitionID(const std::string &diskid, const std::string &partitionname);
std::string GetPartitionNumber(const std::string &partitionname);
void GetVxDevToOsName(std::map<std::string, std::string> &vxdevtonativename);
bool IsDriveUnderVxVM(const std::string &status);
bool IsVxVMVset(const std::string vol, const std::vector<std::string> &vxvmvsets);
void GetVxVMVSets(std::set<dev_t> &vxvmvsets);
bool GetDiskDevts(const std::string &onlydisk, std::set<dev_t> &devts);
std::string GetNativeDiskDir(void);
std::string GetPartitionNumberFromNameText(const std::string &partition);
std::string GetRawDeviceName(const std::string &dskname);
std::string GetBlockDeviceName(const std::string &rdskname);
void GetDevtToDeviceName(const std::string &dir, DevtToDeviceName_t &devttoname);

bool IsReadable(const int &fd, const std::string &devicename, 
                const unsigned long long nsecs, const unsigned long long blksize,
                const E_VERIFYSIZEAT everifysizeat);

bool GetModelVendorFromAtaCmd(const std::string &devicename, std::string &vendor, std::string &model);
std::string GetHCTLWithDelimiter(const std::string &hctldelimited, const std::string &delim);
std::string GetHCTLWithHCTL(const std::string &hctldelimited, const std::string &delim);
#endif /* VOLUMEMANAGER__FUNS__H */
