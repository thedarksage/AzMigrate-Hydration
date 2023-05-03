include top.mak

SRCS = failovercommandutil.cpp \
	main.cpp


X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-DDP_PORTABLE \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v2  \
	-Iappframeworklib \
	-Ischeduler \
	-Ital \
	-Icommon \
   -Iinmsafeint \
   -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
   -IInmXmlParser \
	-Ilog \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
   -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS = \

X_THIRDPARTY_LIBS = \
	$(SQLITE_LIBS) \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(BOOST_LIBS) \
	$(ZLIB_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

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



BINARY=FailoverCommandUtil
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

