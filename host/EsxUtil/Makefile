include top.mak
X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)

SRCS =  \
    ChangeVmConfig.cpp \
    RollBackVM.cpp \
    LinuxReplication.cpp\
    EsxUtil.cpp 

X_CFLAGS = \
    -DVSNAP_USER_MODE \
	-Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
    -Icommon \
    -Icommon/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MINOR_ARCH) \
    -I$(X_ARCH)/config \
    -Iconfig \
    -Iconfig/$(X_MAJOR_ARCH) \
    -Ilog \
    -Ital \
    -Iinmuuid \
    -Iinmuuid/$(X_MAJOR_ARCH) \
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
    -Iconversionlib \
    -Is2libs \
    -Is2libs/common \
    -Is2libs/foundation \
    -Is2libs/thread  \
    -Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
    -Ivacplibs \
    -Icdplibs/$(X_MINOR_ARCH) \
    -Ivsnaplib \
    -Ivirtualvolumelib/unix \
    -Idrivers/Include/linux \
    -Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
    -Idrivers/InVolFlt/$(X_MINOR_ARCH) \
    -Idrivers/lib/$(X_MINOR_ARCH) \
    -Idrivers/InVolFlt/$(X_MINOR_ARCH) \
    -Idrivers/InVirVol/$(X_MINOR_ARCH) \
    -Idrivers/InVirVol/common \
    -Idplibs \
    -Idataprotection \
    -IRetention \
    -Idisklayoutmgr \
    -Iappframeworklib \
    -IInmXmlParser \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
    -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
    -Icurlwrapperlib \
	-Iscopeguard \
    -Isecuritylib \
    -Isecuritylib/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
    $(ACE_INCLUDE) \
    $(SQLITE_INCLUDE) \
    $(BOOST_INCLUDE) \
    $(CURL_INCLUDE) \
    $(OPENSSL_INCLUDE) \
    $(ZLIB_INCLUDE) \
    $(LIBXML2_INCLUDE) \
    $(SIGSLOT_INCLUDE) \
    $(INM_MD5_INCLUDE)

X_LIBS = \
    cxtransportlib \
    npwwn \
    common \
    tal \
    log \
    config \
    s2libs \
    virtualvolumelib \
    volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
    cdplibs \
    vsnaplib \
    vacplibs \
    conversionlib \
    dplibs \
    curlwrapperlib \
	AzureRecoveryLib \
    securitylib

X_THIRDPARTY_LIBS = \
	$(CURL_LIBS) \
	$(ACE_LIBS) \
	$(SQLITE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(LIBXML2_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

ifeq ($(X_OS),$(filter $(X_OS), RHEL8-64 RHEL7-64 SLES11-SP4-64 SLES15-64 DEBIAN11-64 UBUNTU-22.04-64 RHEL9-64 OL8-64 OL9-64))
    X_SYSTEM_LIBS = -luuid -llzma
else
    X_SYSTEM_LIBS = -luuid
endif

X_LDFLAGS = 

BINARY=EsxUtil
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
