include top.mak

SRCS = unregagent.cpp 

X_CFLAGS = \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ilog \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \

X_LIBS = \
    dplibs \
    cdpapplylib \
    npwwn \
	common \
	tal \
	config \
	s2libs \
	log \
	cdplibs \
	vsnaplib \
	vacplibs \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	conversionlib \
    virtualvolumelib \
    cxtransportlib \
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

BINARY=unregagent
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

