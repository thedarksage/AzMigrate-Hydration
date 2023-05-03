###############################################################################
# Makefile
#	To build the vsnap as a kernel modules
#
# $Id: Makefile,v 1.12 2016/03/30 11:26:46 chvirani Exp $
###############################################################################

##
# cflags that would be included with the kernel cflags while compiling
##
EXTRA_CFLAGS += -Wall -D_LINUX_	-DINM_LINUX -D__INM_KERNEL_DRIVERS__	# warn all
EXTRA_CFLAGS += -Wstrict-prototypes		# warn if prototypes are not avail
EXTRA_CFLAGS += -Winline 			# warn if cannot inline
EXTRA_CFLAGS += -I${obj}/../../../../inmsafecapis/unix/

##
# to enable debug
#	usage: $make debug=yes
# to enable debug for development purpose (includes both)
# 	usage: $make debug=dev
##
ifeq (yes, $(debug))
	EXTRA_CFLAGS += -D_VSNAP_DEBUG_	# enable vsnap debugging
	EXTRA_CFLAGS += -g3 		# maximum debugging info
	EXTRA_CFLAGS += -gdwarf-2	# debugging info in DWARF-2 format
	EXTRA_CFLAGS += -Wno-unused-variable	# unused variables
else
	ifeq (dev, $(debug))
		EXTRA_CFLAGS += -D_VSNAP_DEBUG_		# vsnap debugging
		EXTRA_CFLAGS += -D_VSNAP_DEV_DEBUG_	# debugging for dev
		EXTRA_CFLAGS += -g3 			# maximum debug info
		EXTRA_CFLAGS += -gdwarf-2		# in DWARF-2 format
	else
		EXTRA_CFLAGS += -O3			# on all optimizations
		EXTRA_CFLAGS += -Wno-unused-variable	# unused variables
endif
endif

##
# files to be compiled and the object file to be made
##
OBJ_MODULE=linvsnap.o
#VSNAP_OBJS =    vdev_debug.o		\
#		vdev_proc_common.o	\
#		disk_btree_core.o 	\
#		vdev_mem_debug.o 	\
#		vdev_proc.o 		\
#		vsnap_common.o 		\
#		vsnap_control.o 	\
#		vdev_control_common.o 	\
#		vsnap_init.o 		\
#		vsnap_ioctl.o 		\
#		vsnap_kernel.o		\
#		vsnap_kernel_helpers.o	\
#		vsnap_open.o		\
#		vv_control.o		\
#		disk_btree_cache.o	\
#		vdev_thread.o		\
#		vdev_thread_common.o	\
#		vsnap_io.o		\
#		vsnap_io_common.o
#

# This file gives an empty driver
# Uncomment the above VSNAP_OBJS
# to generate original driver
VSNAP_OBJS = vsnap_init_blank.o


##
# build env vars
##
KERNEL_REL_VER=/bin/uname -r
KDIR=/lib/modules/$(shell $(KERNEL_REL_VER))/build
WORKING_DIR=`/bin/pwd`


##
# set the vars required by kernel make files
##
obj-m := $(OBJ_MODULE)			# create this kernel obj
linvsnap-objs := $(VSNAP_OBJS)		# use these objects to create .ko


##
# make options
##
all:
	    $(MAKE) -C $(KDIR) SUBDIRS=$(WORKING_DIR) modules

clean:
	    $(MAKE) -C $(KDIR) SUBDIRS=$(WORKING_DIR) clean

