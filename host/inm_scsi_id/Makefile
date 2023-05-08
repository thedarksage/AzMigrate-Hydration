include top.mak

SRCS = \
        inm_scsi_id.cpp \

X_CFLAGS = \
    -Wcast-align \
	-Iinm_scsi_id \
	-Ideviceidinformer \
	-Iplatforminfo \
	-Iplatforminfo/$(X_MAJOR_ARCH) \
	-Iplatforminfo/$(X_MINOR_ARCH) \
	-Iscsicommandissuer \
	-Iscsicommandissuer/$(X_MINOR_ARCH) \
	-Idirbasename \
	-Idirbasename/$(X_MAJOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	$(BOOST_INCLUDE) \

X_LIBS = \
    deviceidinformer \
    platforminfo \
    scsicommandissuer \
    dirbasename \

X_THIRDPARTY_LIBS =

X_SYSTEM_LIBS = 

X_LDFLAGS =

BINARY=inm_scsi_id
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
