include top.mak

SRCS =  \
	main.cpp \
	PushClient.cpp \
	pushspecparser.cpp \
	$(X_MAJOR_ARCH)/pushclientmajor.cpp

X_CFLAGS = \
	-Is2libs/thread \
	-Is2libs/foundation \
	-Is2libs/common \
	-Iconfig \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
    -Ierrorexception \
	-Ierrorexception/$(X_MAJOR_ARCH) \
    -Ierrorexception/$(X_MINOR_ARCH) \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icxpsclient/$(X_MAJOR_ARCH) \
    -Icurlwrapperlib \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
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
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
    -Ifr_common/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(SIGSLOT_INCLUDE) \
    -IPushInstall/Common/Service \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH)\
	-Iscopeguard \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-Ital \
	-IAzureRecoveryLib/resthelper \
	-IpushInstallerCli 

X_LIBS = \
    dplibs \
    cdpapplylib \
    npwwn \
	cdplibs \
	config \
	common \
	log \
	s2libs \
	vacplibs \
	vsnaplib \
	conversionlib \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
    virtualvolumelib \
    cxtransportlib \
    tal \
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
    $(INM_MD5_LIBS) \

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=pushinstallclient
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
