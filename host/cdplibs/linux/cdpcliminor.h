#ifndef CDPCLI__MINOR__H__
#define CDPCLI__MINOR__H__

#define VSNAP_DEVICE_PREFIX "/dev/vs/cli"
#define UNLOAD_DRVR_COMMAND "/sbin/modprobe -r"
#ifdef INMAGE_ALLOW_UNSUPPORTED
#define LOAD_DRVR_COMMAND   "/sbin/modprobe -v --allow-unsupported"
#else
#define LOAD_DRVR_COMMAND   "/sbin/modprobe -v"
#endif

#endif /* CDPCLI__MINOR__H__ */
