#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>
#include <winioctl.h>

#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include <ace/OS.h>

#include <ace/OS_NS_fcntl.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_errno.h>
#include <ace/Global_Macros.h>
#include <ace/config-lite.h>
#include <ace/Process.h>
//#include "..\common\win32\svtypes.h"
#include <sstream>
//#include "..\common\portablehelpers.h"

#define NTFS_BOOT_SECTOR_LENGTH 512

/* volume boot sector information
 */

struct vbs_info 
{
	unsigned short bytes_per_sector;
	unsigned char sectors_per_cluster;
	unsigned short sectors_per_track;
	unsigned short no_heads;
	unsigned int hidden_sectors;
	unsigned long long total_sectors;
	unsigned long long mft_start_cluster;
	unsigned long long mft2_start_cluster;
	unsigned int cluster_per_frs;
	unsigned int cluster_per_idx;
	unsigned long long serial_number;
};

#define FLAG_VERBOSE_MASK	0x1
#define FLAG_QUIET_MASK		0x2
#define FLAG_FIX_MASK		0x4
#define FLAG_HELP_MASK		0x8
#define SV_MAX_PATH			256
struct info {
	ACE_HANDLE hdl;
	char szVolume[MAX_PATH];
	char boot_sector[NTFS_BOOT_SECTOR_LENGTH];
	struct vbs_info vbs;
	PARTITION_INFORMATION part_info;
	NTFS_VOLUME_DATA_BUFFER ntfs_info;
	DISK_GEOMETRY disk_geometry;
	int cli_flags;
};

class bootSectFix
{
private:
	SVERROR fix_bpb(struct info *i);
	int print_diag_results(struct info *i);
	int gather_info(struct info *i);
	int process_options(int argc, char *argv[], struct info *i);
	int validate_volume(char *szVol);
	int dump_drive_geometry(DISK_GEOMETRY *disk_geometry, char *szVol);
	int get_drive_geometry(ACE_HANDLE hdl, char *szVol, DISK_GEOMETRY *disk_geometry,int quiet);
	int dump_ntfs_info(NTFS_VOLUME_DATA_BUFFER *ntfs_info, char *szVol);
	int get_ntfs_info(ACE_HANDLE hdl, char *szVol, NTFS_VOLUME_DATA_BUFFER *ntfs_info,int quiet);
	int dump_partition_info(PARTITION_INFORMATION *part_info, char *szVol);
	int get_partition_info(ACE_HANDLE hdl, char *szVol, PARTITION_INFORMATION *part_info,int quiet);
	int dump_bpb(struct vbs_info *vbs, char *szVol);
	int get_bpb(ACE_HANDLE hdl, char *szVol, char *boot_sector, struct vbs_info *vbs, int quiet);
	void print_help();
	bool shouldFixCorruptedBootSector;

public:
	int fixBootSector(int argc,char* argv[]);
	bootSectFix(bool shouldFix ){
		shouldFixCorruptedBootSector = shouldFix;
	}
};