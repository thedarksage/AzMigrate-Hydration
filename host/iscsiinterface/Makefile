include top.mak
SRCS = \
	$(X_MINOR_ARCH)/iscsiinitiatorregistration.cpp

X_CFLAGS = \
	-Iiscsiinterface \
	-Iiscsiinterface/$(X_MINOR_ARCH) \
	-Ierrorexception \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(BOOST_INCLUDE)

X_LIBS =

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)iscsiinterface
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


