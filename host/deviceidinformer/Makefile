include top.mak

SRCS = \
	deviceidinformer.cpp \
	scsi_idinformer.cpp \

X_CFLAGS = \
	-Ideviceidinformer \
	-Iscsicommandissuer \
	-Iscsicommandissuer/$(X_MINOR_ARCH) \
	-Iplatforminfo \
	-Iplatforminfo/$(X_MAJOR_ARCH) \
	-Iplatforminfo/$(X_MINOR_ARCH) \
	-Iexecutecommand \
	-Iexecutecommand/$(X_MAJOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Ilog \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)deviceidinformer
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
