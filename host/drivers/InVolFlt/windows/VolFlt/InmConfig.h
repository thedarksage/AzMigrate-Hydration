#pragma once

#define VOLUME_FLT
#ifdef VOLUME_FLT //It is mandatory to enabled VOLUME_FLT for following
//Enable both the macros. As it was non-goal to get compiled and tested for them individually
// Enable following Macro for Cluster Support
#define VOLUME_CLUSTER_SUPPORT 
// Enable following Macro for NoReboot Support
#define VOLUME_NO_REBOOT_SUPPORT
//#define INMAGE_DBG
//#define _CUSTOM_COMMAND_CODE_
#endif

