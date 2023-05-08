#pragma once

// Debug Modules for Driver entry points
#define DM_DRIVER_INIT      0x00000001
#define DM_DRIVER_UNLOAD    0x00000002
#define DM_DRIVER_PNP       0x00000004
#define DM_FLUSH            0x00000008

// Debug modules on functionality
#define DM_PAGEFILE         0x00000010
#define DM_DEV_ARRIVAL      0x00000020
#define DM_SHUTDOWN         0x00000040
#define DM_DATA_FILTERING   0x00000080

// Bitmap Related modules.
#define DM_BITMAP_OPEN      0x00000100
#define DM_BITMAP_READ      0x00000200
#define DM_BITMAP_WRITE     0x00000400
#define DM_BITMAP_CLOSE     0x00000800

#define DM_REGISTRY         0x00001000
#define DM_DEV_CONTEXT      0x00002000
#define DM_CLUSTER          0x00004000
#define DM_MEM_TRACE        0x00008000
#define DM_IOCTL_TRACE      0x00010000
#define DM_POWER            0x00020000
#define DM_WRITE            0x00040000
#define DM_DIRTY_BLOCKS     0x00080000
#define DM_DEVICE_STATE     0x00100000
#define DM_INMAGE_IOCTL     0x00200000
#define DM_TAGS             0x00400000
#define DM_TIME_STAMP       0x00800000
#define DM_CAPTURE_MODE     0x01000000
#define DM_WO_STATE         0x02000000
#define DM_FILE_OPEN        0x04000000
#define DM_FILE_RAW_WRAPPER 0x08000000

#define DM_BYPASS           0x80000000
#define DM_ALL              0xFFFFFFFF

#define DL_NONE         0x00000000
#define DL_ERROR        0x00000001
#define DL_WARNING      0x00000002
#define DL_INFO         0x00000003
#define DL_VERBOSE      0x00000004
#define DL_FUNC_TRACE   0x00000005
#define DL_TRACE_L1     0x00000006
#define DL_TRACE_L2     0x00000007
#define DL_TRACE_L3     0x00000008

