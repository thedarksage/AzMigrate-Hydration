include top.mak

SRCS = \
	scsicommandissuer.cpp \
	$(X_MINOR_ARCH)/platformbasedscsicommandissuer.cpp \


X_CFLAGS = \
	-Iscsicommandissuer \
	-Iscsicommandissuer/$(X_MINOR_ARCH) \
	-Iplatforminfo \
	-Iplatforminfo/$(X_MAJOR_ARCH) \
	-Iplatforminfo/$(X_MINOR_ARCH) \
	-Iexecutecommand \
	-Iexecutecommand/$(X_MAJOR_ARCH) \
	-Idirbasename \
	-Idirbasename/$(X_MAJOR_ARCH) \
	-Ilog \
	-Iconfig \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)scsicommandissuer
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
