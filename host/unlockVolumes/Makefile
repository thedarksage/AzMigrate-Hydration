include top.mak

SRCS = \
	main.cpp \

X_CFLAGS = \
	-Iservice \
	-Is2libs/thread \
	-Is2libs/foundation \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
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
	-I$(X_ARCH)/config \
	-Iconfig/unix \
	-Ilog \
	-Is2libs/common \
	-Is2/domain \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CDK_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
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
	cxtransportlib \
	virtualvolumelib \
        curlwrapperlib \
	AzureRecoveryLib \
        securitylib


X_THIRDPARTY_LIBS = \
	$(SQLITE_LIBS) \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(BOOST_LIBS) \
	$(ZLIB_LIBS) \
	$(CDK_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS = \
	$(LIBCURSES)

X_LDFLAGS =

BINARY=unlockVolumes
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
