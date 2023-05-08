include top.mak
SRCS = \
	cservice.cpp \
	appservice.cpp \
	$(X_MAJOR_ARCH)/servicemajor.cpp \

X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Iapplicationagent \
	-Iapplicationagent/$(X_MAJOR_ARCH) \
	-Ischeduler \
	-Iappframeworklib \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Is2libs/thread \
	-Is2libs/foundation \
	-Is2libs/streams \
	-Is2libs/common \
	-Is2/domain \
	-Icdplibs \
	-IInmXmlParser \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
   -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	-IAzureRecoveryLib/resthelper \
	$(SQLITE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS = \
    dplibs \
    cdpapplylib \
    npwwn \
	appframeworklib \
	scheduler \
    iscsiinterface \
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
    xslibs \
    cxtransportlib \
    curlwrapperlib \
	AzureRecoveryLib \
	AzureRcmLib \
	CommitTagLib \
	AzureADAuthProviderLib \
    securitylib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS) \
	$(SQLITE_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS = -luuid

X_LDFLAGS =

BINARY=appservice
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

