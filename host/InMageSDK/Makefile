include top.mak

SRCS = \
	InMageSDK.cpp \
	RequestValidator.cpp



X_CFLAGS = \
	-IInmXmlParser \
	-IInMageSDKLibs \
	-DVSNAP_USER_MODE \
	-DDP_PORTABLE \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
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
	-Iinmtransaction \
	-Iconfschema \
	-Iinmapihandler \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE)


X_LIBS = 
  

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)InMageSDK
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

