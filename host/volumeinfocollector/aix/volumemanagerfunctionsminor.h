#ifndef VOLUMEMANAGERFUNSMINOR__H_
#define VOLUMEMANAGERFUNSMINOR__H_

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iterator>
#include "voldefs.h"
#include "logicalvolumes.h"
#include "volumegroups.h"
#include "diskpartition.h"
#include "volumemanagerfunctions.h"

#define NATIVEDISKDIR "/dev"
#define NATIVEDEVICEDIR "/dev"

void RemSliceIfReq(const std::string &FullDiskName, std::string &onlydisk);
void RemLastCharIfReq(const std::string &FullDiskName, std::string &onlydisk);
bool IsEmcPowerPathNode(const std::string &device);
bool IsFullEFIDisk(const char *disk);
void GetDiskSetDiskDevts(const std::string &drive, std::set<dev_t> &devts);
bool IsDidDisk(const std::string &drive);
unsigned long long ReadAndFindSize(int fd, const std::string &dskdevicename);
bool CanBeFullDisk(const std::string &device, E_DISKTYPE *pedsktyp);
void GetLvsInSharedDiskSet(Vg_t &diskset);
void GetMetadbInsideDevts(Vg_t &localds);
void GetMetastatInfo(Vg_t &Vg, Lvs_t &Lvs);
void GetMetastatLineInfo(const std::string &line, Vg_t &Vg, Lvs_t &Lvs);
void GetSvmLv(const std::string &firsttok, std::stringstream &ssline, Lvs_t &Lvs);
void GetSvmHotSpares(const std::string &firsttok, std::stringstream &ssline, Vg_t &localds);
bool IsSvmLv(const std::string &lv);
bool IsSvmHsp(const std::string &hsp);

bool ShouldCollectSMISlice(struct partition *p_part);
#ifndef SV_SUN_5DOT8
bool ShouldCollectEFISlice(ushort_t ptag);
#endif /* SV_SUN_5DOT8 */
bool IsBackUpDevice(struct partition *p_part);
int GetVTOCBackUpDevIdx(struct vtoc *pvt);
unsigned long long GetPhysicalOffset(const std::string &disk);
void FillFdiskP0(const std::string &onlydskname, DiskPartitions_t &diskandpartitions);
/*
* refrain from enums being passed using references 
* since passing long long values in sun gcc 
* using reference crashes. enum gets 
* appropriate type based on value and may 
* get long long too
*/
void GetDiskAndItsPartitions(const char *pdskdir,
                             const std::string &diskentryname, 
                             const E_DISKTYPE edsktyp,
                             DiskPartitions_t &diskandpartitions,
                             void *pv_efi_alloc_and_read, 
                             void *pv_efi_free);
bool GetValidDiskSetInsideLv(const std::string &diskset, const std::string &insidetext, std::string &insidelvtostat);

bool DoesLunNumberMatch(const std::string &device, const SV_ULONGLONG *plunnmuber, E_DISKTYPE *pedsktyp);

unsigned long long GetValidSize(const std::string &device, const unsigned long long nsecs, 
                                const unsigned long long secsz, const E_VERIFYSIZEAT everifysizeat);

#endif /* VOLUMEMANAGERFUNSMINOR__H_ */
