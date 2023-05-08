DRVNAME=linvsnap
#COMPILER = gcc
COMPILER = cc
default_target:	all

# This one is for Sun, aka STANDARD, make. GNU make will ignore it.
# there is a tab after sh, so it is seen as target 'ARCH:sh' and not used.
ARCH:sh	=	uname -m

# This one is for Sun, aka STANDARD, make. GNU make will ignore it.
# there is a tab after sh, so it is seen as target 'ARCH:sh' and not used.
VERS:sh	=	uname -r

PROCESSOR_TYPE:sh = uname -p
CTFCONVERT=/opt/onbld/bin/$(PROCESSOR_TYPE)/ctfconvert -g  -L LINVSNAP
CTFMERGE=/opt/onbld/bin/$(PROCESSOR_TYPE)/ctfmerge -l LINVSNAP

# you MUST HAVE -O to go with -Wall, otherwise you will miss
# uninitialized warnings
NONDEBUGFLAGS=-g -O -D_ENABLE_PROC_

#For x86 standard solaris
# CC_i86pc=$(COMPILER) -xarch=amd64 -xmodel=kernel

#For sparc
# For sol 8 assume < sunstudio 11
CC_5.8=-xarch=v9 -D_SOLARIS_8_
# For sol 8 assume >= sunstudio 12
CC_5.9=-m64 -D_SOLARIS_9_
CC_5.10=-m64 -D_SOLARIS_10_
CC_5.11=-m64 -D_SOLARIS_11_

CC_sun4u=$(COMPILER) $(CC_$(VERS)) -D_MEM_ALIGN_
CC_i86pc=$(COMPILER) $(CC_$(VERS)) -xarch=sse2a -xmodel=kernel -xtarget=generic

CC=$(CC_$(ARCH)) $(NONDEBUGFLAGS) ${INCLUDE_FLAGS}

CFLAGS_i86pc = -Di386 -D_LITTLE_ENDIAN_
# there are lots of "if defined(386)", but not the other way around
# gcc defines __sparc, but not __sparcv9
CFLAGS_sun4u = -D__sparcv9  -misalign -D_BIG_ENDIAN_

CFLAGS=-D_KERNEL -D_SOLARIS_ -D_SYSCALL32 $(CFLAGS_$(ARCH)) -D__INM_KERNEL_DRIVERS__ -I./../../../../inmsafecapis/unix

# -g is not allowed in LDFLAGS
LD_FLAGS_sun4u=elf64_sparc
LD_FLAGS_i86pc=elf_x86_64
K_LDFLAGS=-r
LINVSNAP_OBJS=$(ARCH)/disk_btree_core.o         \
    	      $(ARCH)/vdev_mem_debug.o          \
	      $(ARCH)/vdev_proc.o               \
	      $(ARCH)/vsnap_common.o            \
	      $(ARCH)/vsnap_control.o           \
	      $(ARCH)/vdev_control_common.o     \
	      $(ARCH)/vsnap_init.o              \
	      $(ARCH)/vsnap_io.o                \
	      $(ARCH)/vsnap_ioctl.o             \
	      $(ARCH)/vsnap_kernel.o            \
	      $(ARCH)/vsnap_kernel_helpers.o    \
	      $(ARCH)/vsnap_open.o              \
	      $(ARCH)/vv_control.o              \
	      $(ARCH)/vdev_disk_helpers.o       \
	      $(ARCH)/vsnap_io_common.o         \
	      $(ARCH)/vdev_thread.o             \
	      $(ARCH)/vdev_thread_common.o      \
	      $(ARCH)/disk_btree_cache.o	\
	      $(ARCH)/vdev_scsi.o		\
	      $(ARCH)/vdev_proc_common.o	\
	      $(ARCH)/vdev_debug.o

# You CAN HAVE multiple .c files. Just add them here as .o files
ARCH_OBJS=$(LINVSNAP_OBJS)

# THis is the target filename of the final driver module,
# in an architecture-specific subdirectory
ARCH_MODULE=$(ARCH)/$(DRVNAME)

ID_sun4u = sparcv9
ID_i86pc = amd64
INSTDIR=$(ID_$(ARCH))

MKDIR=mkdir

QUIET_AS = @echo '    ' AS '   '   $@.s ${$@_1};
QUIET_CC = @echo '    ' CC '   '   $@ ${$@_1};
QUIET_LD = @echo '    ' LD '   '   $@;
QUIET_RM = @echo '    ' RM '   '   ${ARCH};
QUIET_CP = @echo '    ' CP '   '   $*;
QUIET_MKDIR = @echo '    ' MKDIR ''   ${ARCH};
QUIET_CTFCONV=@echo '    ' CTC'   '   $@ ${$@_1};
QUIET_CTFMERGE=@echo '    ' CTM'   '   $@;
QUIET_MISC = @echo '    ' ;



all:	$(ARCH_MODULE)

mkdir:
	${QUIET_MKDIR} mkdir -p $(ARCH)

expand:
	${QUIET_CC} $(CC) $(CFLAGS) -c -E -o $@.e $<

$(ARCH_MODULE):	 $(MKDIR) $(ARCH) $(ARCH_OBJS)
	${QUIET_LD} ld $(K_LDFLAGS) -o $(ARCH_MODULE) $(ARCH_OBJS)
	${QUIET_CTFMERGE}$(CTFMERGE) -o $(ARCH_MODULE) $(LINVSNAP_OBJS)

$(ARCH)/%.o: %.c
	${QUIET_AS} $(CC) $(CFLAGS) $(DEBUG_FLAGS) -c -S -o $@.s $<
	${QUIET_CC} $(CC) $(CFLAGS) $(DEBUG_FLAGS) -c -o $@ $<
	${QUIET_CTFCONV} $(CTFCONVERT) $@

clean:
	${QUIET_RM} /bin/rm -rf $(ARCH)

install:
	@echo "Installing the driver "
	rem_drv linvsnap
	/bin/rm -f /kernel/drv/$(INSTDIR)/linvsnap*
	/bin/cp ${ARCH}/linvsnap /kernel/drv/$(INSTDIR)/linvsnap
	/bin/cp linvsnap.conf /kernel/drv/$(INSTDIR)/
	/bin/cp linvsnap.conf /kernel/drv/
	add_drv linvsnap

uninstall:
	@echo "Uninstalling the driver"
	/bin/rm -f /kernel/drv/$(INSTDIR)/linvsnap*
