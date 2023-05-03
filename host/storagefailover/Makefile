include top.mak

SRCS =  \
	$(X_MINOR_ARCH)/storagefailover.cpp \
	$(X_MINOR_ARCH)/prockill.cpp \
	main.cpp

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
	-Ivsnaplib \
	-Ivirtualvolumelib/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS = \
    cxtransportlib \
    npwwn \
	common \
	tal \
	s2libs \
	cdplibs \
	vsnaplib \
	vacplibs \
	virtualvolumelib \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	config \
	log \
	conversionlib \
        curlwrapperlib \
	AzureRecoveryLib \
        securitylib


X_THIRDPARTY_LIBS = \
	$(SQLITE_LIBS) \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS = 

BINARY=storagefailover
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

