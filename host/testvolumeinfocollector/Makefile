include top.mak

SRCS = \
	testvolumeinfocollector.cpp

X_CFLAGS = \
	-DDUMP_INFOS_TO_STDOUT \
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
	-Inpwwn \
	-Ilog \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(BOOST_INCLUDE)

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
	AzureRecoveryLib \
    securitylib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
    $(SQLITE_LIBS) \
    $(BOOST_LIBS) \
    $(INM_MD5_LIBS)


X_SYSTEM_LIBS = 

X_LDFLAGS =

BINARY=testvolumeinfocollector
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
