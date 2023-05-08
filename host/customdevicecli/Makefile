include top.mak

SRCS = customdevicecli.cpp

X_CFLAGS = \
	-DDP_PORTABLE \
	-DBASIC_TEST \
	-DVSNAP_USER_MODE \
	-Ilog \
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
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Is2libs/thread \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
	dplibs \
	cdpapplylib \
	common \
	tal \
	config \
	s2libs \
	log \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	cdplibs \
	vsnaplib \
	conversionlib \
    vacplibs \
    virtualvolumelib \
    cxtransportlib \
	npwwn \
    curlwrapperlib \
    securitylib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
    $(SQLITE_LIBS) \
    $(INM_MD5_LIBS) \
    $(BOOST_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=customdevicecli
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
