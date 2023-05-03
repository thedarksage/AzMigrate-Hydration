#ifndef _INMAGE_BITMAP_API_H
#define _INMAGE_BITMAP_API_H

// thse are flags used in the bitmap file header
#define BITMAP_FILE_VERSION               (0x00010004)
#define BITMAP_FILE_VERSION2              (0x0001000C)
#define BITMAP_FILE_ENDIAN_FLAG           (0x000000FF)

#define MAX_WRITE_GROUPS_IN_BITMAP_HEADER (31)
#define MAX_CHANGES_IN_WRITE_GROUP        (64)
#define DISK_SECTOR_SIZE                  (512)
#define HEADER_CHECKSUM_SIZE              (16)
#define HEADER_CHECKSUM_DATA_SIZE (sizeof(bitmap_header_t) - HEADER_CHECKSUM_SIZE)
#define LOG_HEADER_SIZE ((DISK_SECTOR_SIZE * MAX_WRITE_GROUPS_IN_BITMAP_HEADER) + DISK_SECTOR_SIZE)
#define LOG_HEADER_OFFSET                 (LOG_HEADER_SIZE)

#define MAX_KDIRTY_CHANGES                (256) /* temporary fix */


/** log header 
 *   +--------------------------------------------------------------------+
 *   | validation_checksum | endian | header_size | version | data_offset |
 *   +--------------------------------------------------------------------+
 *   | data_offset | bitmap_offset | bitmap_size | bitmap_granularity     |
 *   +--------------------------------------------------------------------+
 *   | volume_size | recovery_state | last_chance_changes | boot_cycles   |
 *   + -------------------------------------------------------------------+
 *   | changes_lost                                                       |
 *   +--------------------------------------------------------------------+
 */
 
typedef struct _logheader_tag
{
	unsigned char     validation_checksum[HEADER_CHECKSUM_SIZE];
	unsigned int    endian;
	unsigned int    header_size;
	unsigned int    version;
	unsigned int    data_offset;
	unsigned long long    bitmap_offset;
	unsigned long long    bitmap_size;
	unsigned long long    bitmap_granularity;
	long long     volume_size;
	unsigned int    recovery_state;
	unsigned int    last_chance_changes;
	unsigned int    boot_cycles;
	unsigned int    changes_lost;
	unsigned int    resync_required;
	unsigned int    resync_errcode;
	unsigned long long resync_status;
} logheader_t;

// recovery states
#define BITMAP_LOG_RECOVERY_STATE_UNINITIALIZED   0
#define BITMAP_LOG_RECOVERY_STATE_CLEAN_SHUTDOWN  1
#define BITMAP_LOG_RECOVERY_STATE_DIRTY_SHUTDOWN  2
#define BITMAP_LOG_RECOVERY_STATE_LOST_SYNC       3

typedef struct _last_chance_changes_tag
{
	union 
	{
		unsigned long long length_offset_pair[MAX_CHANGES_IN_WRITE_GROUP];
		unsigned char sector_fill[DISK_SECTOR_SIZE];
	}u;

} last_chance_changes_t;


/** Bitmap header 
 *   +----------------------------------+
 *   | log header  		        |
 *   +----------------------------------+
 *   | length and offset pair group 1 --+-----------+
 *   +----------------------------------+           |
 *   | length and offset pair group 2 --+------+    |
 *   +----------------------------------+      |    |
 *   |  ...                             |      |    |
 *   +----------------------------------+      |    |
 *   | length and offset pair group 31--+--+   |    |
 *   +----------------------------------+  |   |    |
 *                                         |   |    |
 *    +------------------------------------+   |    |
 *    |   +------------------------------------+    V
 *    |   |         +---------------------------------------------------------------------+
 *    |   |         | length_offset_pair1 | length_offset_pair2 | .. length_offset_pair64 |
 *    |   |         +---------------------------------------------------------------------+
 *    |   |
 *    |   |     +---------------------------------------------------------------------+
 *    |   + --->| length_offset_pair1 | length_offset_pair2 | .. length_offset_pair64 |
 *    |         +---------------------------------------------------------------------+
 *    |        ...
 *    |        ...
 *    |        ...
 *    |    +---------------------------------------------------------------------+
 *    +--->| length_offset_pair1 | length_offset_pair2 | .. length_offset_pair64 |
 *         +---------------------------------------------------------------------+
 */

typedef struct _bitmap_header_tag
{
	union {
		logheader_t    header;
		unsigned char        sector_fill[DISK_SECTOR_SIZE];
	}u;

	last_chance_changes_t  change_groups[MAX_WRITE_GROUPS_IN_BITMAP_HEADER];
} bitmap_header_t;

#endif
