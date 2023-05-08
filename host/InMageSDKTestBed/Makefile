include top.mak

SRCS = InMageSDKTestBed.cpp \

X_CFLAGS = \
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
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Is2libs/common \
	-Is2libs \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivacplibs \
	-IInMageSDK \
	-Ivsnaplib \
    -IInmXmlParser \
    -IInMageSDKLibs \
	-Ivirtualvolumelib/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS = \
	npwwn \
	common \
	tal \
	s2libs \
	cdplibs \
	vsnaplib \
	vacplibs \
	virtualvolumelib \
	volumeinfocollector \
	appdevicelib \
	config \
	log \
	conversionlib \
	cxtransportlib \
	scheduler \
	InMageSDKLibs \
	InmXmlParser \
	confschema \
	inmapihandler \
	inmtransaction \
	InMageSDK

X_THIRDPARTY_LIBS = \
	$(SQLITE_LIBS) \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS = 

BINARY=InMageSDKTestBed
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

