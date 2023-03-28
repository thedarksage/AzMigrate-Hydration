#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <cf.h>
#include <odmi.h>
#include <sys/sysconfig.h>
#include <sys/cfgodm.h>
#include <sys/cfgdb.h>
#include <sys/device.h>
#include <sys/sysmacros.h>

#define LIB_PATH "/usr/lib/drivers"
#define CONTROL_DEVICE "/dev/vsnapcontrol"

void
usage(int error)
{
	printf("Usage:\n\tmodule load [Path to Driver | Driver Name]\n");
	exit(error);
}

int
load_driver(char *driver_path)
{
	struct cfg_load cfg;
	struct cfg_dd cfgdd;
	long major = 0;
	int *minor;
	mid_t	kmid;
	int error = 0;

/*	if( access(driver_path, F_OK) ) {
		printf("Driver %s not found\n", driver_path);
		usage(ENOENT);
	}
*/
	printf("Loading Driver %s\n", driver_path);

	major = genmajor("linvsnap");
	if( major == -1 ) {
		perror("Generate Major");
		error = errno;
		goto out;
	}

	minor = genminor("vsnapcontrol", major, 0, 1, 0, 0);
	if( *minor != 0 ) {
		perror("Cannot allocate minor 0 for control");
		error = -EINVAL;
		goto genminor_failed;
	}

	printf("Loading Driver with major = %d\n", major);

	cfg.path = driver_path;
	cfg.libpath = LIB_PATH;
	cfg.kmid = 0;
	error = sysconfig(SYS_SINGLELOAD | SYS_64BIT, &cfg,
			  sizeof(struct cfg_load));
	if ( error ) {
		perror("Driver Load Error");
		error = errno;
		goto driver_load_failed;
	}

	cfgdd.kmid = cfg.kmid;
	cfgdd.devno = (dev_t)makedev64(major, 0);
	cfgdd.cmd = CFG_INIT;
	cfgdd.ddsptr = NULL;
	cfgdd.ddslen = 0;

	printf("kmid = %d", cfgdd.kmid);

	error = sysconfig(SYS_CFGDD, &cfgdd, sizeof(struct cfg_dd));
	if( error ) {
		perror("Driver Initialise Error");
		error = errno;
		goto driver_config_error;
	}

	mknod(CONTROL_DEVICE, S_IFCHR, (dev_t)makedev64(major, 0));

out:
	return error;

driver_config_error:
	sysconfig(SYS_KULOAD, &cfg, sizeof(struct cfg_load));

driver_load_failed:
	reldevno("vsnapcontrol", 0);

genminor_failed:
	relmajor("linvsnap");
	goto out;
}

int
unload_driver(char *driver_path)
{
	struct cfg_load cfg;
	struct cfg_dd cfgdd;
	long major = 0;
	long minor = 0;
	mid_t	kmid;
	int error;

	printf("Unloading Driver %s\n", driver_path);

	major = genmajor("linvsnap");
	if( major == -1 ) {
		perror("Generate Major");
		error = errno;
		goto out;
	}

	cfg.path = driver_path;
	cfg.libpath = LIB_PATH;
	cfg.kmid = 0;
	error = sysconfig(SYS_QUERYLOAD, &cfg, sizeof(struct cfg_load));
	if ( error ) {
		perror("Driver Query Error");
		error = errno;
		goto out;
	}

	if( cfg.kmid == 0 ) {
		printf("Driver %s not loaded");
		goto out;
	}

	cfgdd.kmid = cfg.kmid;
	cfgdd.devno = (dev_t)makedev64(major, 0);
	cfgdd.cmd = CFG_TERM;
	cfgdd.ddsptr = NULL;
	cfgdd.ddslen = 0;

	error = sysconfig(SYS_CFGDD, &cfgdd, sizeof(struct cfg_dd));
	if( error ) {
		perror("Driver Terminate Error");
		error = errno;
		return error;
	}

	error = sysconfig(SYS_KULOAD, &cfg, sizeof(struct cfg_load));
	if ( error ) {
		perror("Driver Unload Error");
		error = errno;
	}

	reldevno("vsnapcontrol", 0);
	relmajor("linvsnap");
	if (unlink(CONTROL_DEVICE) < 0){
		perror("Error in removing control device");
		error = errno;
	}

out:
	return error;
}

int
main(int argc, char *argv[])
{
	int load = 1; /* default load */
	char *name = NULL;
	int error = 0;
	char *drvname;

	if( argc != 3 ) {
		printf("Insufficient data\n");
		usage(EINVAL);
	}

	if( !strstr("load", argv[1]) ) {
		if( !strstr("unload", argv[1]) ) {
			printf("Unknown Command\nValid Commands: load, unload\n");
			usage(EINVAL);
		} else {
			load = 0;
		}
	}

	printf("Sizeof dev_t = %d", sizeof(dev_t));

	error = odm_initialize();
	if( error ) {
		printf("ODM Initialise failed");
		error = odmerrno;
		goto out;
	}

	if( load )
		error = load_driver(argv[2]);
	else
		error = unload_driver(argv[2]);

	if ( odm_terminate() )
		printf("ODM Terminate failed with error %d", odmerrno);

out:
	exit(error);
}
