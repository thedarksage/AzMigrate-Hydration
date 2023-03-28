include top.mak
SRCS = \
    	$(X_MAJOR_ARCH)/XenServerDiscovery.cpp \
	$(X_MAJOR_ARCH)/isXen.cpp \

X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-Ital \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/thread  \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Ivacplibs \
	-Ivsnaplib \
	-Ivirtualvolumelib/unix \
	-Idrivers/Include/linux \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/linux \
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
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	$(LIBXML2_INCLUDE) \
	$(CURL_INCLUDE)	\
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)xslibs
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


