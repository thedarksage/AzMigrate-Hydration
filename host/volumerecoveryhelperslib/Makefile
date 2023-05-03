include top.mak

SRCS =  \
	volumerecoveryhelpersproxy.cpp \
	volumerecoveryhelperexception.cpp \
	unix/volproxyutilmajor.cpp \
    volproxyutilcommon.cpp



X_CFLAGS = \
	-Iappframeworklib \
	-IInmXmlParser \
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
	-Ivolumerecoveryhelperslib \
	-IInMageSDKLibs \
	-DVSNAP_USER_MODE \
	-DDP_PORTABLE \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Ital \
    -Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
    -Idrivers/InVolFlt/$(X_MINOR_ARCH) \
    -Idrivers/lib/$(X_MINOR_ARCH) \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Is2libs/common \
	-Is2libs \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivacplibs \
	-Ivsnaplib \
	-Ivirtualvolumelib/$(X_MAJOR_ARCH) \
	-Idisklayoutmgr \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	-Iinmapihandler/confengine \
	-Iconfschema \
	-Iinmtransaction


X_LIBS = \


X_THIRDPARTY_LIBS =

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)volumerecoveryhelperslib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

