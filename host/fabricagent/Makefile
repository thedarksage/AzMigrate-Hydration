include top.mak

SRCS = \
	fabricagent.cpp \
	cservice.cpp \
	devicehelper.cpp \

# BUGBUG: DrvDefines.h defined only in win32 directory

X_CFLAGS = \
	-DSV_FABRIC \
	-D_FABRIC_ \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-Ital \
	-Ivacplibs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MINOR_ARCH) \
	-Is2libs/thread \
	-I$(X_ARCH)/config \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
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
	-Ivolumeinfocollector/unix \
	-Ivolumeinfocollector/linux \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
   -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	-IAzureRecoveryLib/resthelper \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
    cxtransportlib \
    npwwn \
	common \
	tal \
	log \
	config \
	s2libs \
	cdplibs \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	vacplibs \
	vsnaplib \
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
	$(XPATH_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS = \
	-lrt \
	-ldl

X_LDFLAGS =

BINARY=fabricagent
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
