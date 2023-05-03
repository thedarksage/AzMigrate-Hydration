include top.mak

SRCS =  \
    AzureRcmCli.cpp \

X_CFLAGS = \
	-IAzureRcmCli \
	-IAzureRcmLib \
	-ICommitTagLib \
	-IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-IAzureRecoveryLib/common \
	-Ilog \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Ierrorexception \
	-Iinmsafecapis \
	-Iscopeguard \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iinmsafeint \
	-Is2libs \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Iexecutecommand \
	-Idirbasename \
	-Icxtransportlib \
	-Icxpslib \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-Ideviceidinformer \
	-Iscsicommandissuer \
	-Iscsicommandissuer/$(X_MINOR_ARCH) \
	-Idevicefilter \
	-Idevicefilter/$(X_MAJOR_ARCH) \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(LIBXML2_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

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
    cxpslib \
    npwwn \
    appdevicelib \
    curlwrapperlib \
    securitylib\
	AzureRecoveryLib \
	AzureRcmLib \
	AzureADAuthProviderLib \

X_THIRDPARTY_LIBS =  \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(SQLITE_LIBS) \
    $(BOOST_LIBS) \
    $(INM_MD5_LIBS)  

X_SYSTEM_LIBS = \

X_LDFLAGS = 

BINARY=AzureRcmCli
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
