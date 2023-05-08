include top.mak

SRCS =  \
    RcmTestClient.cpp \
    SimulatedTestMachine.cpp

X_CFLAGS = \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-ICommitTagLib \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-IAzureRcmLib \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(LIBXML2_INCLUDE)

X_LIBS = \
	dplibs \
	cdpapplylib \
	npwwn \
    cdplibs \
    config \
    common \
    s2libs \
    tal \
    vsnaplib \
    log \
    volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
	appdevicelib \
    virtualvolumelib \
    cdplibs \
    vacplibs \
    conversionlib \
    cxtransportlib \
    npwwn \
    appdevicelib \
    curlwrapperlib \
    securitylib\
	AzureRecoveryLib \
	AzureRcmLib \

X_THIRDPARTY_LIBS =  \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(SQLITE_LIBS) \
    $(BOOST_LIBS) \
    $(INM_MD5_LIBS) \
	$(LIBXML2_LIBS)

X_SYSTEM_LIBS = \

X_LDFLAGS = 

BINARY=RcmTestClient
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
