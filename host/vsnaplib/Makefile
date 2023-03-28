include top.mak

SRCS = \
	DiskBtreeCore.cpp \
	VsnapCommon.cpp \
	DiskBtreePortableHelpers.cpp \
	VsnapUser.cpp \
	$(X_MAJOR_ARCH)/vsnapusermajor.cpp \
	$(X_MINOR_ARCH)/vsnapuserminor.cpp \

X_CFLAGS = \
	-Wcast-align \
	-DDP_PORTABLE \
	-DBASIC_TEST \
	-DVSNAP_USER_MODE \
	-Iconversionlib \
	-Is2libs/foundation \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Ivsnaplib \
	-Ivsnaplib/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
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
	-Idrivers/InVirVol/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/common \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Is2libs/common -Is2libs \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivacplibs \
	-Ilog \
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

BINARY=$(X_LIBPRE)vsnaplib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

