include top.mak

SRCS = \
       $(X_MAJOR_ARCH)/virtualvolumemajor.cpp \
       $(X_MINOR_ARCH)/virtualvolumeminor.cpp \

X_CFLAGS = \
	-DDP_PORTABLE \
	-DBASIC_TEST \
	-Icommon/$(X_MAJOR_ARCH) \
	-Is2libs/foundation \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Ivirtualvolumelib \
	-Ivirtualvolumelib/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
    -Ideviceidinformer \
    -Iplatforminfo \
    -Iplatforminfo/$(X_MAJOR_ARCH) \
    -Iplatforminfo/$(X_MINOR_ARCH) \
    -Iexecutecommand \
    -Iexecutecommand/$(X_MINOR_ARCH) \
    -Iscsicommandissuer \
    -Iscsicommandissuer/$(X_MINOR_ARCH) \
    -Idirbasename \
    -Idirbasename/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-I$(X_ARCH)/config \
	-ISnapShotMgr/include \
	-IRetention \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Is2libs/common -Is2libs \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivacplibs \
	-Idrivers/InVirVol/common \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(SIGSLOT_INCLUDE) \


X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)virtualvolumelib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

