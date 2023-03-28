EXTRA_CFLAGS += $(CFLAGS) -Wall -Wstrict-prototypes -DINM_LINUX -D__INM_KERNEL_DRIVERS__

ifeq (, $(BLD_DATE))
	# same as __DATE__
	BLD_DATE=""$(shell date +"%b %d %Y")""
endif
ifeq (, $(BLD_TIME))
	# same as __TIME__
	BLD_TIME=""$(shell date +"%H:%M:%S")""
endif
EXTRA_CFLAGS += -DBLD_DATE="\"${BLD_DATE}\"" -DBLD_TIME="\"${BLD_TIME}\""

# To handle distro and/or update/service pack/patch sepecific issues
VENDOR:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH; \
	if [ -f /etc/redhat-release ]; then echo "redhat"; \
	elif [ -f /etc/SuSE-release ] ; then echo "suse";  \
	elif [ -f /etc/os-release ] && grep -q 'SLES' /etc/os-release ; then echo "suse-os" ; \
	elif [ -f /etc/lsb-release ] && grep -q 'Ubuntu' /etc/lsb-release ; then echo "ubuntu" ; \
	elif [ -f /etc/debian_version ]; then echo "debian"; \
	else echo "OS_UNKNOWN"; fi)

ifeq (yes, $(noerror))
	EXTRA_CFLAGS += -Wno-error
endif

EXTRA_CFLAGS += -D${VENDOR}

ifeq ($(findstring suse, $(VENDOR)), suse)
	ifeq ($(VENDOR), suse)
		DISTRO_VER:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
				grep VERSION /etc/SuSE-release | cut -d" " -f3 | \
				cut -d"." -f 1)
		ifeq ($(PATCH_LEVEL), )
		PATCH_LEVEL:=$(shell grep PATCHLEVEL /etc/SuSE-release | cut -d" " -f3)
		endif
	else
		DISTRO_VER:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
				grep VERSION= /etc/os-release | cut -d"\"" -f 2 | \
				cut -d"-" -f 1)
		PATCH_LEVEL:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
				grep -o -- -SP[0-9] /etc/os-release | cut -d"P" -f 2)
		VENDOR:=suse
	endif
	ifeq ($(PATCH_LEVEL), )
		PATCH_LEVEL:=0
		endif
	EXTRA_CFLAGS += -DDISTRO_VER=${DISTRO_VER} -DPATCH_LEVEL=${PATCH_LEVEL}
ifeq ($(DISTRO_VER), 12)
	        EXTRA_CFLAGS += -mindirect-branch=thunk-inline -mindirect-branch-register
endif
endif

ifeq ($(findstring redhat, $(VENDOR)), redhat)

	IS_CENTOS:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH; cat /etc/redhat-release | grep -i '^centos' | cut -d" " -f 1 | tr A-Z a-z)
	IS_ALMA:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH; cat /etc/redhat-release | grep -i '^alma' | cut -d" " -f 1 | tr A-Z a-z)

	ifeq ($(findstring centos, $(IS_CENTOS)), centos)

		VER_STR=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
			cat /etc/redhat-release | sed 's/Server //' | cut -d" " -f 4)
	else ifeq ($(findstring alma, $(IS_ALMA)), alma)

		VER_STR=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
			cat /etc/redhat-release | sed 's/Server //' | cut -d" " -f 3)
	else

		VER_STR=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
			cat /etc/redhat-release | sed 's/Server //' | cut -d" " -f 6)

	endif
	DISTRO_VER:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
			echo ${VER_STR} | cut -d"." -f 1)


	ifeq ($(DISTRO_VER), )
		DISTRO_VER:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH; \
			      uname -r | cut -d"e" -f 2 | cut -d"." -f 1 | cut -d"l" -f 2)
		ifeq ($(DISTRO_VER), )
			DISTRO_VER:=4
		endif
	endif

	ifeq ($(DISTRO_VER), 4)
		UPDATE:=`if [ \`grep Update /etc/redhat-release | wc -l\`  -le 1 ]; \
			 then echo 0; else cat /etc/redhat-release | cut -d" " -f 10 | \
			 tr -d ")"; fi`
	else
		ifneq ($(VER_STR), $(DISTRO_VER))
			UPDATE:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
				echo ${VER_STR} | cut -d" " -f 7 | \
				cut -d"." -f 2)
		endif
	endif

	ifeq ($(UPDATE), )
		UPDATE:=0
	endif

	STATUS:=$(shell export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH;\
                        grep XenServer /etc/redhat-release)

	ifneq ($(STATUS), )
		DISTRO_VER:=0
	endif

	EXTRA_CFLAGS += -DDISTRO_VER=${DISTRO_VER} -DUPDATE=${UPDATE}

	ifeq ($(findstring 4.18.0-425, $(KDIR)), 4.18.0-425)
		ifeq ($(findstring el8, $(KDIR)), el8)
			EXTRA_CFLAGS += -DSET_INM_QUEUE_FLAG_STABLE_WRITE
		endif
	endif

	ifeq ($(findstring 5.14.0-70.13.1.el9_0.x86_64, $(KDIR)), 5.14.0-70.13.1.el9_0.x86_64)
		EXTRA_CFLAGS += -DRHEL9_0
	endif

	ifeq ($(findstring uek, $(KDIR)), uek)
		EXTRA_CFLAGS += -DUEK
	endif
	ifeq ($(findstring 2.6.32-100.28.5.el6.x86_64, $(KDIR)), 2.6.32-100.28.5.el6.x86_64)
		EXTRA_CFLAGS += -DUEK
	endif
endif

#enable the following flag is driver is built on a system without host/ files.
#EXTRA_CFLAGS += -DFILTER_DRIVER_PRIVATE_BUILD

ifeq (yes, $(debug))
    ifeq ($(findstring suse, $(VENDOR)), suse)
	ifeq ($(DISTRO_VER), 11)
	    ifeq ($(PATCH_LEVEL), 2)
		EXTRA_CFLAGS += -DINM_DEBUG -DIDEBUG_MIRROR -g3 -gdwarf-2 -O2
	    endif
	endif
    else
		EXTRA_CFLAGS += -DINM_DEBUG -DIDEBUG_MIRROR -g3 -gdwarf-2 -O0
    endif
else
    ifeq (all, $(debug))
	ifeq ($(findstring suse, $(VENDOR)), suse)
	    ifeq ($(DISTRO_VER), 11)
		ifeq ($(PATCH_LEVEL), 2)
		    EXTRA_CFLAGS += -DINM_DEBUG -DIDEBUG -DIDEBUG_META -DIDEBUG_MIRROR -DIDEBUG_MIRROR_IO -g3 -gdwarf-2 -O2
		endif
	    endif
	else
    	    EXTRA_CFLAGS += -DINM_DEBUG -DIDEBUG -DIDEBUG_META -DIDEBUG_MIRROR -DIDEBUG_MIRROR_IO -g3 -gdwarf-2 -O0
	endif
    else
	    EXTRA_CFLAGS += -O3 -g3
    endif
endif

ifeq (yes, $(err))
	EXTRA_CFLAGS += -DINJECT_ERR
endif

ifeq (yes, $(fabric))
	EXTRA_CFLAGS += -DAPPLIANCE_DRV
endif

# Use -D_FABRIC_LUN_PERSISTENCE for enabling persisting LUN configuration
EXTRA_CFLAGS += -I${WORKING_DIR}/../../../../thirdparty/md5_drv/common/ -I${BLD_DIR} -I${WORKING_DIR}/../../../inmsafecapis/unix/

OBJ_MODULE=involflt.o
ifneq (yes, $(dummy))
INVOLFLT_OBJS = verifier.o		\
		osdep.o        			\
		last_chance_writes.o	\
		filestream_raw.o		\
		tunable_params.o 		\
		ioctl.o					\
        bitmap_api.o			\
		bitmap_operations.o		\
		change-node.o 			\
		data-file-mode.o		\
		data-mode.o 			\
		db_routines.o			\
        driver-context.o        \
		filestream.o			\
		filestream_segment_mapper.o	\
		filter.o 				\
		filter_host.o 			\
		involflt_debug_routines.o 	\
		iobuffer.o				\
		md5.o	\
		metadata-mode.o 		\
		segmented_bitmap.o		\
		statechange.o			\
		target-context.o 		\
		utils.o					\
		work_queue.o			\
		VBitmap.o				\
		file-io.o				\
		telemetry-types.o		\
		telemetry-exception.o	\
		telemetry.o
else
ifeq (yes, $(dummy))
INVOLFLT_OBJS = verifier.o		\
		osdep.o         \
        dummy/bitmap_api.o			\
		bitmap-mode.o			\
	    dummy/bitmap_operations.o		\
		change-node.o 			\
	    dummy/data-file-mode.o		\
		data-mode.o 			\
	    dummy/db_routines.o			\
        driver-context.o        \
		filestream.o			\
	    dummy/filestream_segment_mapper.o	\
		filter.o 			\
		filter_host.o 			\
		involflt_debug_routines.o 	\
	    dummy/iobuffer.o			\
		ioctl.o				\
		md5.o	\
		metadata-mode.o 		\
		proc.o				\
	    dummy/segmented_bitmap.o		\
	    dummy/statechange.o			\
		target-context.o 		\
		tunable_params.o 		\
		utils.o				\
		dummy/work_queue.o			\
		dummy/VBitmap.o			\
		file-io.o			\
		sysfs_common_attributes.o 	\
		sysfs_volume_attributes.o
endif
endif



ifeq (yes, $(fabric))
	INVOLFLT_OBJS += filter_lun.o
else
	INVOLFLT_OBJS += dummy_filter_lun.o
endif


obj-m += ${OBJ_MODULE}
involflt-objs := ${INVOLFLT_OBJS}


all:
	@echo "BLD_DATE: ${BLD_DATE}"
	@echo "BLD_TIME: ${BLD_TIME}"
	@echo "EXTRA_CFLAGS: ${EXTRA_CFLAGS}"
	$(MAKE) -C ${KDIR} M=${BLD_DIR} modules

#cleaning all the build files
clean:
	$(MAKE) -C ${KDIR} M=${WORKING_DIR} clean

