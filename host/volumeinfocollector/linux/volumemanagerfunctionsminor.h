#ifndef VOLUMEMANAGERFUNSMINOR__H_
#define VOLUMEMANAGERFUNSMINOR__H_

#include <sstream>
#include <cctype>
#include <string>
#include <set>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <functional>
#include <cstring>

#include "volumedefines.h"

#define NATIVEDISKDIR "/dev"
#define BLOCKENTRY "block"

typedef std::vector<std::string> PartitionNegAttrs_t;

bool ShouldCollectPartition(const std::string &partitionAttr, 
                            PartitionNegAttrs_t &partitionNegAttrs);
bool IsLinuxPartition(const std::string &device);
bool IsVxDmpPartition(const std::string &disk, const std::string &partition);
std::string GetDeviceNameFromBlockEntry(const std::string &blockentry);
void GetScsiHCTLFromSysBlock(CollectDeviceHCTLPair_t c);
void GetScsiHCTLFromSysClass(CollectDeviceHCTLPair_t c);
bool ShouldPreferSysBlockForHCTL(void);
std::string GetScsiHCTLFromSysBlockDevice(const std::string &devicedir, bool bWithHctl = false);
std::string GetNameBasedOnScsiHCTL(const std::string &device);
std::string GetNameBasedOnScsiHCTLFromSysClass(const std::string &device);
std::string GetNameBasedOnScsiHCTLFromSysBlock(const std::string &device);
#endif /* VOLUMEMANAGERFUNSMINOR__H_ */
