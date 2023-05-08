#ifndef _VV_DEBUG_DEFINES_H_
#define _VV_DEBUG_DEFINES_H_

// Debug Modules for Driver entry points
#define DM_VV_DRIVER_INIT       0x00000001
#define DM_VV_DRIVER_UNLOAD     0x00000002

#define DM_VV_CREATE            0x00000010
#define DM_VV_CLOSE             0x00000020
#define DM_VV_READ              0x00000040
#define DM_VV_WRITE             0x00000080
#define DM_VV_DEVICE_CONTROL    0x00000100
#define DM_VV_IOCTL_TRACE		0x00000200

#define DM_VV_CMD_QUEUE         0x00010000
#define DM_VV_CMD               0x00020000
#define DM_VV_PNP               0x00030000
#define DM_VV_VSNAP				0x00040000

#define DL_VV_NONE         0x00000000
#define DL_VV_ERROR        0x00000001
#define DL_VV_WARNING      0x00000002
#define DL_VV_INFO         0x00000003
#define DL_VV_VERBOSE      0x00000004
#define DL_VV_FUNC_TRACE   0x00000005
#define DL_VV_TRACE_L1     0x00000006
#define DL_VV_TRACE_L2     0x00000007

#define TEST_DRIVER

#endif // _VV_DEBUG_DEFINES_H_